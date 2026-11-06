// HopCast: the chat panel dock. GPL-2.0-or-later.
#include "HopCastChatDock.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLibrary>
#include <QMainWindow>
#include <QMetaObject>
#include <QHBoxLayout>
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

		auto *overlayRow = new QHBoxLayout;
		overlayButton_ = new QPushButton(QStringLiteral("Show over my game"), outer);
		overlayButton_->setObjectName(QStringLiteral("chatOverlayButton"));
		overlayButton_->setCheckable(true);
		overlayButton_->setToolTip(QStringLiteral("Put the chat over your game: see-through, always on top, clicks go to the game"));
		moveButton_ = new QPushButton(QStringLiteral("Move / resize"), outer);
		moveButton_->setObjectName(QStringLiteral("chatOverlayMoveButton"));
		moveButton_->setCheckable(true);
		moveButton_->setEnabled(false);
		moveButton_->setToolTip(QStringLiteral("Drag the overlay to where you want it, then turn this off to lock it"));
		overlayRow->addWidget(overlayButton_, 1);
		overlayRow->addWidget(moveButton_);
		outerLayout->addLayout(overlayRow);
		connect(overlayButton_, &QPushButton::clicked, this, [this](bool on) {
			hopcast::setChatOverlay(on);
			syncOverlayButtons();
		});
		connect(moveButton_, &QPushButton::clicked, this, [this](bool on) {
			hopcast::setChatOverlayMove(on);
			syncOverlayButtons();
		});
		syncTimer_.setInterval(700);
		connect(&syncTimer_, &QTimer::timeout, this, [this]() { syncOverlayButtons(); });
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
			if (visible)
				syncTimer_.start();
			else
				syncTimer_.stop();
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

	static QLibrary &sharedLibrary()
	{
		static QLibrary lib;
		return lib;
	}
	static QLibrary *loadedLibrary() { return sharedLibrary().isLoaded() ? &sharedLibrary() : nullptr; }

	// The overlay needs the page, so asking for it (by hand, from the tray or the dock) starts the chat panel even while the
	// dock stays closed.
	bool ensurePanel()
	{
		opened_ = true;
		if (!panel_ && !tried_)
			loadPanel();
		return panel_ != nullptr;
	}

	void syncOverlayButtons()
	{
		const bool on = hopcast::chatOverlayOn();
		overlayButton_->setChecked(on);
		moveButton_->setEnabled(on);
		moveButton_->setChecked(on && hopcast::chatOverlayMove());
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
			if (chat.value(QStringLiteral("crashedLastTime"), false).toBool())
				say(QStringLiteral("The chat panel's browser engine stopped HopCast last time, so the panel was turned off to keep HopCast safe. "
						   "To try it again, turn on \"Allow the Chat panel\" in Settings > General > Chat, then close and open this panel."));
			else
				say(QStringLiteral("The Chat panel is turned off. Turn it on in Settings > General > Chat, then close and open this panel again."));
			tried_ = false; // look again the next time it is opened
			return;
		}
		const QString path = libraryPath();
		if (!QFile::exists(path)) {
			say(QStringLiteral("The chat panel isn't part of this build (its browser engine wasn't included)."));
			return;
		}
		QLibrary &library = sharedLibrary(); // stays loaded for the life of the program
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
		// Crash guard: say "the browser engine is running" before it starts and take it back at a clean exit. If HopCast is
		// found with the mark still set, the engine took it down, and the panel is switched off for next time.
		const qint64 started = QDateTime::currentSecsSinceEpoch();
		chat.setValue(QStringLiteral("engineRunning"), started);
		chat.setValue(QStringLiteral("engineAlive"), started);
		chat.sync();
		// For the first two minutes note every 20 seconds that HopCast is still alive. A crash from the engine comes right
		// after it starts; a HopCast that is closed with a kill, a log-out or a power-off later on is not a crash.
		beat_.setInterval(20000);
		connect(&beat_, &QTimer::timeout, this, [this, started]() {
			const qint64 now = QDateTime::currentSecsSinceEpoch();
			QSettings c = hopcast::openChatSettings();
			c.setValue(QStringLiteral("engineAlive"), now);
			c.sync();
			if (now - started >= 120)
				beat_.stop();
		});
		beat_.start();
		static bool exitHooked = false;
		if (!exitHooked) {
			exitHooked = true;
			QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
				QSettings c = hopcast::openChatSettings();
				c.remove(QStringLiteral("engineRunning"));
				c.remove(QStringLiteral("engineAlive"));
				c.sync();
			});
		}
		QWidget *panel = create(holder_);
		if (!panel) {
			say(QStringLiteral("The chat panel couldn't start."));
			return;
		}
		delete message_;
		message_ = nullptr;
		layout_->addWidget(panel);
		panel_ = panel;
	}

	QWidget *holder_ = nullptr;
	QWidget *panel_ = nullptr;
	QPushButton *overlayButton_ = nullptr;
	QPushButton *moveButton_ = nullptr;
	QTimer syncTimer_;
	QTimer beat_;
	QLabel *message_ = nullptr;
	QVBoxLayout *layout_ = nullptr;
	bool tried_ = false;
	bool opened_ = false;
};

} // namespace

namespace hopcast {

namespace {
ChatDock *g_dock = nullptr;

// the library's overlay entry points, found once the chat panel has been loaded
using OverlaySet = int (*)(int);
using OverlayState = int (*)();
} // namespace

QDockWidget *createChatDock(QMainWindow *window)
{
	{
		// The last run ended with the browser engine still marked as running: it never exited cleanly. Only a stop within two
		// minutes of the engine starting counts as the engine taking HopCast down (then the chat panel is turned off, and
		// switched back on in Settings > General > Chat). A kill or a shut-down much later is not blamed on the chat.
		QSettings chat = openChatSettings();
		if (chat.contains(QStringLiteral("engineRunning"))) {
			const qint64 started = chat.value(QStringLiteral("engineRunning")).toLongLong();
			const qint64 alive = chat.value(QStringLiteral("engineAlive"), started).toLongLong();
			chat.remove(QStringLiteral("engineRunning"));
			chat.remove(QStringLiteral("engineAlive"));
			if (qEnvironmentVariableIsEmpty("HOPCAST_NO_CRASH_GUARD") && alive - started < 120) {
				chat.setValue(QStringLiteral("enabled"), false);
				chat.setValue(QStringLiteral("crashedLastTime"), true);
			}
			chat.sync();
		}
	}
	auto *dock = new ChatDock(window);
	g_dock = dock;
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	window->addDockWidget(Qt::RightDockWidgetArea, dock);
	dock->hide();
	return dock;
}


bool chatOverlayAvailable()
{
	return g_dock && QFile::exists(QCoreApplication::applicationDirPath() +
#ifdef Q_OS_WIN
				       QStringLiteral("/hopcast-chat.dll"));
#else
				       QStringLiteral("/../lib/libhopcast-chat.so"));
#endif
}

static QLibrary *loadedChatLibrary()
{
	return ChatDock::loadedLibrary();
}

bool chatOverlayOn()
{
	QLibrary *lib = loadedChatLibrary();
	const auto state = lib ? reinterpret_cast<OverlayState>(lib->resolve("hopcast_chat_overlay_state")) : nullptr;
	return state && (state() & 1);
}

bool chatOverlayMove()
{
	QLibrary *lib = loadedChatLibrary();
	const auto state = lib ? reinterpret_cast<OverlayState>(lib->resolve("hopcast_chat_overlay_state")) : nullptr;
	return state && (state() & 2);
}

bool setChatOverlay(bool on)
{
	if (!g_dock)
		return false;
	if (on) {
		QSettings chat = openChatSettings();
		if (!chat.value(QStringLiteral("enabled"), true).toBool() || !g_dock->ensurePanel())
			return false;
	}
	QLibrary *lib = loadedChatLibrary();
	const auto set = lib ? reinterpret_cast<OverlaySet>(lib->resolve("hopcast_chat_overlay_set")) : nullptr;
	if (!set)
		return false;
	return set(on ? 1 : 0) != 0;
}

bool setChatOverlayMove(bool move)
{
	QLibrary *lib = loadedChatLibrary();
	const auto set = lib ? reinterpret_cast<OverlaySet>(lib->resolve("hopcast_chat_overlay_move")) : nullptr;
	return set && set(move ? 1 : 0) != 0;
}

} // namespace hopcast
