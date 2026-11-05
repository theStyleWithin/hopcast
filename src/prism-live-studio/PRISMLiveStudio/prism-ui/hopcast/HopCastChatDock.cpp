// HopCast: the chat panel dock. GPL-2.0-or-later.
#include "HopCastChatDock.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLibrary>
#include <QMainWindow>
#include <QVBoxLayout>

namespace {

class ChatDock : public QDockWidget {
public:
	explicit ChatDock(QWidget *parent) : QDockWidget(QStringLiteral("Chat"), parent)
	{
		setObjectName(QStringLiteral("hopcastChatDock"));
		holder_ = new QWidget(this);
		layout_ = new QVBoxLayout(holder_);
		layout_->setContentsMargins(0, 0, 0, 0);
		setWidget(holder_);
		// load the browser engine only when the dock is first shown
		connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
			if (visible && !tried_)
				loadPanel();
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
		auto *label = new QLabel(text, holder_);
		label->setWordWrap(true);
		label->setMargin(10);
		label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		layout_->addWidget(label);
	}

	void loadPanel()
	{
		tried_ = true;
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
		layout_->addWidget(panel);
	}

	QWidget *holder_ = nullptr;
	QVBoxLayout *layout_ = nullptr;
	bool tried_ = false;
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
