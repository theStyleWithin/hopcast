// HopCast: the chat panel dock. GPL-2.0-or-later.
#include "HopCastChatDock.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLibrary>
#include <QMainWindow>
#include <QMetaObject>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "HopCastChatSettings.h"

namespace {

class ChatDock : public QDockWidget {
public:
	explicit ChatDock(QWidget *parent) : QDockWidget(QStringLiteral("Chat"), parent)
	{
		setObjectName(QStringLiteral("hopcastChatDock"));
		auto *outer = new QWidget(this);
		auto *outerLayout = new QVBoxLayout(outer);
		outerLayout->setContentsMargins(6, 6, 6, 0);
		auto *settings = new QPushButton(QStringLiteral("Chat settings..."), outer);
		settings->setObjectName(QStringLiteral("chatSettingsButton"));
		settings->setToolTip(QStringLiteral("Open Settings > General > Chat to set the chat link"));
		outerLayout->addWidget(settings);
		holder_ = new QWidget(outer);
		layout_ = new QVBoxLayout(holder_);
		layout_->setContentsMargins(0, 0, 0, 0);
		outerLayout->addWidget(holder_, 1);
		setWidget(outer);

		connect(settings, &QPushButton::clicked, this, [this]() {
			if (QWidget *w = window())
				QMetaObject::invokeMethod(w, "onPopupSettingView", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("General")),
							  Q_ARG(QString, QString()));
		});

		// The panel starts only when you open it yourself (View > Docks > Chat, or its right-click entry). If a saved
		// window layout brings the dock back at start-up it is hidden again, so the chat can never stop HopCast from opening.
		connect(toggleViewAction(), &QAction::triggered, this, [this]() { opened_ = true; });
		connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
			if (!visible)
				return;
			// wait one turn of the event loop: a click on the menu entry reports "triggered" just after the dock shows
			QTimer::singleShot(0, this, [this]() {
				if (!isVisible())
					return;
				if (!opened_)
					hide();
				else if (!tried_)
					loadPanel();
			});
		});
	}

private:
	static QString libraryPath()
	{
		const QString dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
		return dir + QStringLiteral("/hopcast-chat.dll");
#else
		return dir + QStringLiteral("/../lib/libhopcast-chat.so");
#endif
	}

	void say(const QString &text)
	{
		delete message_;
		message_ = new QLabel(text, holder_);
		message_->setWordWrap(true);
		message_->setMargin(10);
		message_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		layout_->addWidget(message_);
	}

	void loadPanel()
	{
		tried_ = true;
		QSettings chat = hopcast::openChatSettings();
		if (!chat.value(QStringLiteral("enabled"), true).toBool()) {
			say(QStringLiteral("The Chat panel is turned off. Turn it on in Settings > General > Chat, then close and open this panel again."));
			tried_ = false; // look again the next time it is opened
			return;
		}
		const QString path = libraryPath();
		if (!QFile::exists(path)) {
			say(QStringLiteral("The chat panel isn't part of this build (its browser engine wasn't included)."));
			return;
		}
		static QLibrary library; // stays loaded for the life of the program
		library.setFileName(path);
		if (!library.load()) {
			say(QStringLiteral("The chat panel's browser engine couldn't be loaded on this computer:\n") + library.errorString());
			return;
		}
		using Abi = int (*)();
		using Create = QWidget *(*)(QWidget *);
		const auto abi = reinterpret_cast<Abi>(library.resolve("hopcast_chat_abi"));
		const auto create = reinterpret_cast<Create>(library.resolve("hopcast_chat_create"));
		if (!abi || !create || abi() != 1) {
			say(QStringLiteral("The chat panel doesn't match this version of HopCast."));
			return;
		}
		QWidget *panel = create(holder_);
		if (!panel) {
			say(QStringLiteral("The chat panel couldn't start."));
			return;
		}
		delete message_;
		message_ = nullptr;
		layout_->addWidget(panel);
	}

	QWidget *holder_ = nullptr;
	QLabel *message_ = nullptr;
	QVBoxLayout *layout_ = nullptr;
	bool tried_ = false;
	bool opened_ = false;
};

} // namespace

namespace hopcast {

QDockWidget *createChatDock(QMainWindow *window)
{
	auto *dock = new ChatDock(window);
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	window->addDockWidget(Qt::RightDockWidgetArea, dock);
	dock->hide();
	return dock;
}

} // namespace hopcast
