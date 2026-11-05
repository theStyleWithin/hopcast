// HopCast: the chat panel's web view. GPL-2.0-or-later.
//
// Built as its own small library so HopCast still starts on a computer where the browser engine (Qt WebEngine) can't be
// loaded; the app opens it on demand with QLibrary (see HopCastChatDock.cpp). It shows one page, the chat link you paste,
// and nothing else: no tabs, no downloads, links inside the chat open in your normal browser.
#include <QCursor>
#include <QDesktopServices>
#include <QFrame>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QSaveFile>
#include <QScreen>
#include <QWindow>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWidget>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define HOPCAST_CHAT_EXPORT extern "C" __declspec(dllexport)
#else
#define HOPCAST_CHAT_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {

// The page you pointed at stays put. Anything you click inside it that leads somewhere else opens in your own browser,
// pop-up windows are never made, and files are never downloaded.
class ChatPage : public QWebEnginePage {
public:
	using QWebEnginePage::QWebEnginePage;

protected:
	bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
	{
		if (type == NavigationTypeLinkClicked) {
			if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))
				QDesktopServices::openUrl(url);
			return false;
		}
		return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
	}

	QWebEnginePage *createWindow(WebWindowType) override
	{
		// a page asked for a new window (target=_blank, window.open): hand its link to the browser instead
		auto *probe = new QWebEnginePage(profile(), this);
		connect(probe, &QWebEnginePage::urlChanged, probe, [probe](const QUrl &u) {
			if (u.scheme() == QLatin1String("http") || u.scheme() == QLatin1String("https"))
				QDesktopServices::openUrl(u);
			probe->deleteLater();
		});
		return probe;
	}
};

QString dataDir()
{
	QString d = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	if (d.isEmpty())
		d = QDir::homePath();
	return d + QStringLiteral("/HopCast/chat-profile");
}

// http(s) only. A bare "kick.com/..." gets https:// in front. Anything else (file:, javascript:, ...) is refused.
QUrl cleanLink(const QString &text)
{
	QString t = text.trimmed();
	if (t.isEmpty() || t.contains(QLatin1Char(' ')))
		return QUrl();
	if (!t.contains(QLatin1String("://")))
		t = QStringLiteral("https://") + t;
	const QUrl u(t, QUrl::StrictMode);
	if (!u.isValid() || u.host().isEmpty() || (u.scheme() != QLatin1String("http") && u.scheme() != QLatin1String("https")))
		return QUrl();
	return u;
}

#ifdef Q_OS_LINUX
// Chromium's sandbox needs unprivileged user namespaces; some systems switch them off. Try once in a throw-away child.
bool sandboxPossible()
{
	const pid_t pid = fork();
	if (pid < 0)
		return true; // can't tell: leave the sandbox on
	if (pid == 0)
		_exit(unshare(CLONE_NEWUSER) == 0 ? 0 : 1);
	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif

QSettings openSettings()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("HopCast"), QStringLiteral("Chat"));
}

#ifdef Q_OS_LINUX
// KDE on Wayland: a program may not keep its own window on top, but KWin can be told to (a window rule). Once, before the
// overlay is first shown, add a rule that matches only the overlay's exact title. Best effort: nothing else is touched.
void ensureKdeOverlayRule()
{
	if (!QGuiApplication::platformName().contains(QLatin1String("wayland")) ||
	    !qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("KDE"), Qt::CaseInsensitive))
		return;
	QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
	if (base.isEmpty())
		base = QDir::homePath() + QStringLiteral("/.config");
	const QString path = base + QStringLiteral("/kwinrulesrc");
	const QString name = QStringLiteral("hopcast-chat-overlay");

	QString text;
	QFile in(path);
	if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
		text = QString::fromUtf8(in.readAll());
		in.close();
	}
	if (text.contains(QLatin1Char('[') + name + QLatin1Char(']')))
		return;

	QStringList lines = text.split(QLatin1Char('\n'));
	QStringList rules;
	int general = -1, generalEnd = -1;
	for (int i = 0; i < lines.size(); ++i) {
		if (lines[i].trimmed() == QLatin1String("[General]")) {
			general = i;
			generalEnd = i + 1;
			while (generalEnd < lines.size() && !lines[generalEnd].startsWith(QLatin1Char('['))) {
				if (lines[generalEnd].startsWith(QLatin1String("rules=")))
					rules = lines[generalEnd].mid(6).split(QLatin1Char(','), Qt::SkipEmptyParts);
				++generalEnd;
			}
			break;
		}
	}
	rules << name;
	const QStringList header = {QStringLiteral("[General]"), QStringLiteral("count=%1").arg(rules.size()), QStringLiteral("rules=") + rules.join(QLatin1Char(','))};
	if (general >= 0) {
		for (int i = generalEnd - 1; i >= general; --i)
			lines.removeAt(i);
		for (int i = header.size() - 1; i >= 0; --i)
			lines.insert(general, header[i]);
	} else {
		for (int i = header.size() - 1; i >= 0; --i)
			lines.prepend(header[i]);
	}
	while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
		lines.removeLast();
	lines << QString() << QLatin1Char('[') + name + QLatin1Char(']')
	      << QStringLiteral("Description=HopCast chat overlay")
	      << QStringLiteral("above=true") << QStringLiteral("aboverule=2")
	      << QStringLiteral("acceptfocus=false") << QStringLiteral("acceptfocusrule=2")
	      << QStringLiteral("skippager=true") << QStringLiteral("skippagerrule=2")
	      << QStringLiteral("skipswitcher=true") << QStringLiteral("skipswitcherrule=2")
	      << QStringLiteral("skiptaskbar=true") << QStringLiteral("skiptaskbarrule=2")
	      << QStringLiteral("positionrule=4") << QStringLiteral("sizerule=4")
	      << QStringLiteral("title=HopCast Chat Overlay") << QStringLiteral("titlematch=1") << QString();

	QSaveFile out(path);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
		return;
	out.write(lines.join(QLatin1Char('\n')).toUtf8());
	if (!out.commit())
		return;
	QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
										 QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure")));
}
#endif

// Shown over the overlay while it is in "move / resize" mode: drag anywhere to move, drag an edge to resize.
class DragLayer : public QWidget {
public:
	explicit DragLayer(QWidget *parent) : QWidget(parent)
	{
		setObjectName(QStringLiteral("chatOverlayDrag"));
		setMouseTracking(true);
		setAttribute(Qt::WA_StyledBackground, true);
		setStyleSheet(QStringLiteral("background: rgba(20,20,20,120); border: 2px solid #f5e700;"));
		auto *l = new QVBoxLayout(this);
		auto *t = new QLabel(QStringLiteral("Drag to move.\nDrag an edge to resize.\nTurn off \"Move / resize\" to lock it so clicks go to your game."), this);
		t->setAlignment(Qt::AlignCenter);
		t->setWordWrap(true);
		t->setStyleSheet(QStringLiteral("color: white; background: transparent; border: none;"));
		l->addWidget(t);
	}

protected:
	static Qt::Edges edgesAt(const QPoint &p, const QSize &s)
	{
		const int m = 14;
		Qt::Edges e;
		if (p.x() < m)
			e |= Qt::LeftEdge;
		if (p.x() >= s.width() - m)
			e |= Qt::RightEdge;
		if (p.y() < m)
			e |= Qt::TopEdge;
		if (p.y() >= s.height() - m)
			e |= Qt::BottomEdge;
		return e;
	}

	void mouseMoveEvent(QMouseEvent *ev) override
	{
		const Qt::Edges e = edgesAt(ev->position().toPoint(), size());
		if (e == (Qt::LeftEdge | Qt::TopEdge) || e == (Qt::RightEdge | Qt::BottomEdge))
			setCursor(Qt::SizeFDiagCursor);
		else if (e == (Qt::RightEdge | Qt::TopEdge) || e == (Qt::LeftEdge | Qt::BottomEdge))
			setCursor(Qt::SizeBDiagCursor);
		else if (e & (Qt::LeftEdge | Qt::RightEdge))
			setCursor(Qt::SizeHorCursor);
		else if (e & (Qt::TopEdge | Qt::BottomEdge))
			setCursor(Qt::SizeVerCursor);
		else
			setCursor(Qt::SizeAllCursor);
	}

	void mousePressEvent(QMouseEvent *ev) override
	{
		if (ev->button() != Qt::LeftButton)
			return;
		QWindow *w = window()->windowHandle();
		if (!w)
			return;
		const Qt::Edges e = edgesAt(ev->position().toPoint(), size());
		if (e)
			w->startSystemResize(e);
		else
			w->startSystemMove();
	}
};

// The chat over your game: no frame, see-through, always on top, never takes the keyboard focus. Locked (the normal state) it
// is also click-through, so the mouse and keys go to the game. It doesn't make a second browser: it borrows the Chat panel's
// one page while it is showing, and gives it back when it is turned off.
class OverlayWindow : public QWidget {
public:
	OverlayWindow()
		: QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus)
	{
		setObjectName(QStringLiteral("hopcastChatOverlay"));
		setWindowTitle(QStringLiteral("HopCast Chat Overlay"));
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_ShowWithoutActivating);
		layout_ = new QVBoxLayout(this);
		layout_->setContentsMargins(0, 0, 0, 0);
		drag_ = new DragLayer(this);
		drag_->hide();
		hint_ = new QLabel(this);
		hint_->setObjectName(QStringLiteral("chatOverlayHint"));
		hint_->setWordWrap(true);
		hint_->setMargin(8);
		hint_->setStyleSheet(QStringLiteral("color: white; background: rgba(20,20,20,200); border-radius: 6px;"));
		hint_->hide();
		saveTimer_.setSingleShot(true);
		saveTimer_.setInterval(600);
		connect(&saveTimer_, &QTimer::timeout, this, [this]() { saveRect(); });
	}

	void adopt(QWebEngineView *view)
	{
		view_ = view;
		layout_->addWidget(view);
		view->setAttribute(Qt::WA_TranslucentBackground);
		view->page()->setBackgroundColor(Qt::transparent);
		view->show();
		drag_->raise();
	}

	QWebEngineView *release()
	{
		QWebEngineView *v = view_;
		if (v) {
			layout_->removeWidget(v);
			v->page()->setBackgroundColor(Qt::white);
		}
		view_ = nullptr;
		return v;
	}

	bool locked() const { return locked_; }

	// a short message over the chat (for example "no chat link yet"), since a see-through empty window shows nothing
	void setHint(const QString &text)
	{
		hint_->setText(text);
		hint_->setVisible(!text.isEmpty());
		layoutOverlays();
		hint_->raise();
	}

	void setLocked(bool locked)
	{
		locked_ = locked;
		layoutOverlays();
		drag_->setVisible(!locked);
		drag_->raise();
		hint_->raise();
		applyInputFlag();
	}

	// show over the game (without taking focus)
	void present()
	{
#ifdef Q_OS_LINUX
		ensureKdeOverlayRule();
#endif
		restoreRect();
		setLocked(true);
		show();
		raise();
		applyInputFlag();
	}

protected:
	void showEvent(QShowEvent *ev) override
	{
		QWidget::showEvent(ev);
		applyInputFlag();
	}
	void moveEvent(QMoveEvent *ev) override
	{
		QWidget::moveEvent(ev);
		if (isVisible())
			saveTimer_.start();
	}
	void resizeEvent(QResizeEvent *ev) override
	{
		QWidget::resizeEvent(ev);
		layoutOverlays();
		if (isVisible())
			saveTimer_.start();
	}

private:
	// the drag layer covers the whole window; the hint sits at the top
	void layoutOverlays()
	{
		drag_->setGeometry(rect());
		hint_->setFixedWidth(qMax(100, width() - 24));
		hint_->adjustSize();
		hint_->move(12, 12);
	}

	void applyInputFlag()
	{
		if (QWindow *w = windowHandle())
			w->setFlag(Qt::WindowTransparentForInput, locked_);
	}

	// "x,y,w,h"; where the window system lets a program place its own window (X11, Windows) it comes back there
	void restoreRect()
	{
		QSettings s = openSettings();
		const QStringList p = s.value(QStringLiteral("overlayRect")).toString().split(QLatin1Char(','));
		QRect r;
		if (p.size() == 4)
			r = QRect(p[0].toInt(), p[1].toInt(), qBound(200, p[2].toInt(), 4000), qBound(150, p[3].toInt(), 4000));
		const QScreen *scr = QGuiApplication::primaryScreen();
		const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);
		if (r.isNull() || !avail.intersects(r))
			r = QRect(avail.right() - 400, avail.top() + 120, 380, 560);
		setGeometry(r);
	}

	void saveRect()
	{
		QSettings s = openSettings();
		const QRect r = geometry();
		s.setValue(QStringLiteral("overlayRect"), QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()));
		s.sync();
	}

	QVBoxLayout *layout_ = nullptr;
	DragLayer *drag_ = nullptr;
	QLabel *hint_ = nullptr;
	QWebEngineView *view_ = nullptr;
	QTimer saveTimer_;
	bool locked_ = true;
};

// The chat link, "stay signed in" and "sound" are set in HopCast's Settings > General > Chat (they are kept in Chat.ini).
// This panel only shows the page and notices when those choices change. Reload and text size live here.
class ChatPanel : public QWidget {
public:
	explicit ChatPanel(QWidget *parent) : QWidget(parent), settings_(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("HopCast"), QStringLiteral("Chat"))
	{
		setObjectName(QStringLiteral("hopcastChatPanel"));
		auto *root = new QVBoxLayout(this);
		root->setContentsMargins(6, 6, 6, 6);

		auto *options = new QHBoxLayout;
		reload_ = new QToolButton;
		reload_->setText(QStringLiteral("↻"));
		reload_->setToolTip(QStringLiteral("Reload the chat"));
		options->addWidget(reload_);
		options->addStretch(1);
		auto *smaller = new QToolButton;
		smaller->setText(QStringLiteral("A-"));
		smaller->setToolTip(QStringLiteral("Smaller text"));
		auto *bigger = new QToolButton;
		bigger->setText(QStringLiteral("A+"));
		bigger->setToolTip(QStringLiteral("Bigger text"));
		options->addWidget(smaller);
		options->addWidget(bigger);
		root->addLayout(options);

		note_ = new QLabel;
		note_->setObjectName(QStringLiteral("chatNote"));
		note_->setWordWrap(true);
		note_->setVisible(false);
		root->addWidget(note_);

		overlayNote_ = new QLabel(QStringLiteral("The chat is showing over your game. Turn that off with \"Show over my game\" above, or in the tray menu."));
		overlayNote_->setObjectName(QStringLiteral("chatOverlayNote"));
		overlayNote_->setWordWrap(true);
		overlayNote_->setVisible(false);
		root->addWidget(overlayNote_);

		// Everything below must be set before the first browser object is made (the engine reads it once, at that point).
		// Light on a busy computer: one renderer process, Chromium's low-memory mode, no background network chatter.
		if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
			qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--enable-low-end-device-mode --renderer-process-limit=1 --disable-background-networking --disable-smooth-scrolling");
#ifdef Q_OS_LINUX
		if (!sandboxPossible()) {
			qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
			sandboxNote_ = QStringLiteral("This system doesn't allow the browser's safety sandbox, so the chat runs without it. Only load chat links you trust.");
		}
#endif

		view_ = new QWebEngineView;
		view_->setObjectName(QStringLiteral("chatView"));
		root_ = root;
		root->addWidget(view_, 1);

		zoom_ = qBound(0.5, settings_.value(QStringLiteral("zoom"), 1.0).toDouble(), 3.0);

		connect(reload_, &QToolButton::clicked, this, [this]() { view_->reload(); });
		connect(smaller, &QToolButton::clicked, this, [this]() { setZoom(zoom_ - 0.1); });
		connect(bigger, &QToolButton::clicked, this, [this]() { setZoom(zoom_ + 0.1); });

		// Settings writes the file from the main program; look at it twice a second while this panel is open.
		watch_.setInterval(500);
		connect(&watch_, &QTimer::timeout, this, [this]() { refresh(); });
		watch_.start();

		// Nothing showing (dock closed, overlay off): freeze the page so it uses no CPU, and let the browser drop its memory
		// after a few minutes. Opening the dock or the overlay brings it back (reloading if it was dropped).
		discard_.setSingleShot(true);
		discard_.setInterval(180000);
		connect(&discard_, &QTimer::timeout, this, [this]() {
			if (!isVisible() && !overlayOn_ && view_->page())
				view_->page()->setLifecycleState(QWebEnginePage::LifecycleState::Discarded);
		});
		self_ = this;

		signedIn_ = readSignedIn();
		sound_ = readSound();
		makePage();
		refresh(true);
	}

	~ChatPanel() override
	{
		if (self_ == this)
			self_ = nullptr;
		if (overlayOn_)
			setOverlay(false);
		delete overlay_;
	}

	static ChatPanel *instance() { return self_; }
	bool overlayOn() const { return overlayOn_; }
	bool overlayMove() const { return overlay_ && overlayOn_ && !overlay_->locked(); }

	// Put the chat over the game (or take it back into the dock). The page is moved, not copied.
	void setOverlay(bool on)
	{
		if (on == overlayOn_)
			return;
		if (on) {
			if (!overlay_)
				overlay_ = new OverlayWindow;
			root_->removeWidget(view_);
			overlay_->adopt(view_);
			overlayOn_ = true;
			overlayNote_->setVisible(true);
			overlay_->present();
			overlay_->setHint(lastNote_);
		} else {
			overlay_->hide();
			overlay_->release();
			overlayOn_ = false;
			root_->addWidget(view_, 1);
			view_->show();
			overlayNote_->setVisible(false);
		}
		applyOpacity(true);
		updateLifecycle();
	}

	void setOverlayMove(bool on)
	{
		if (overlay_ && overlayOn_)
			overlay_->setLocked(!on);
	}

protected:
	void showEvent(QShowEvent *ev) override
	{
		QWidget::showEvent(ev);
		QTimer::singleShot(0, this, [this]() { updateLifecycle(); });
	}
	void hideEvent(QHideEvent *ev) override
	{
		QWidget::hideEvent(ev);
		QTimer::singleShot(0, this, [this]() { updateLifecycle(); }); // after the page itself has been hidden
	}

private:
	void updateLifecycle()
	{
		QWebEnginePage *p = view_->page();
		if (!p)
			return;
		if (isVisible() || overlayOn_) {
			discard_.stop();
			watch_.setInterval(500);
			p->setLifecycleState(QWebEnginePage::LifecycleState::Active);
		} else {
			watch_.setInterval(3000);
			p->setLifecycleState(QWebEnginePage::LifecycleState::Frozen);
			discard_.start();
		}
	}

	// How see-through the chat is over the game (20-100 %, set from the tray menu); full strength inside the dock.
	void applyOpacity(bool force = false)
	{
		const int pct = overlayOn_ ? qBound(20, settings_.value(QStringLiteral("overlayOpacity"), 85).toInt(), 100) : 100;
		if (!force && pct == opacity_)
			return;
		opacity_ = pct;
		if (view_->page())
			view_->page()->runJavaScript(QStringLiteral("document.documentElement.style.opacity = '%1';").arg(pct / 100.0, 0, 'f', 2));
	}

	bool readSignedIn() { return settings_.value(QStringLiteral("signedIn"), false).toBool(); }
	bool readSound() { return settings_.value(QStringLiteral("sound"), false).toBool(); }

	// Looks at Chat.ini again and applies whatever changed in Settings.
	void refresh(bool first = false)
	{
		settings_.sync();
		const bool signedIn = readSignedIn();
		const bool sound = readSound();
		const QString typed = settings_.value(QStringLiteral("url")).toString();
		applyOpacity();

		bool reloadNeeded = first;
		if (signedIn != signedIn_) {
			signedIn_ = signedIn;
			makePage();
			reloadNeeded = true;
		}
		if (sound != sound_) {
			sound_ = sound;
			if (view_->page())
				view_->page()->setAudioMuted(!sound_);
		}
		if (typed != url_) {
			url_ = typed;
			reloadNeeded = true;
		}
		if (!reloadNeeded)
			return;

		if (url_.trimmed().isEmpty()) {
			view_->setUrl(QUrl(QStringLiteral("about:blank")));
			showNote(QStringLiteral("No chat link yet. Open HopCast's Settings, go to General > Chat, and paste your chat link."));
			return;
		}
		const QUrl link = cleanLink(url_);
		if (!link.isValid()) {
			showNote(QStringLiteral("That doesn't look like a web link. In Settings > General > Chat it should look like https://..."));
			return;
		}
		showNote(QString());
		view_->load(link);
	}

	void showNote(const QString &text)
	{
		lastNote_ = text;
		if (overlay_ && overlayOn_)
			overlay_->setHint(text);
		const QString all = text.isEmpty() ? sandboxNote_ : (sandboxNote_.isEmpty() ? text : text + QLatin1Char('\n') + sandboxNote_);
		note_->setText(all);
		note_->setVisible(!all.isEmpty());
	}

	// A fresh page each time the "stay signed in" choice changes: kept cookies live in HopCast's own folder, otherwise
	// the page has no storage at all and everything is forgotten when HopCast closes.
	void makePage()
	{
		QWebEngineProfile *profile = nullptr;
		if (signedIn_) {
			profile = new QWebEngineProfile(QStringLiteral("HopCastChat"));
			profile->setPersistentStoragePath(dataDir());
			profile->setCachePath(dataDir() + QStringLiteral("/cache"));
			profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
		} else {
			profile = new QWebEngineProfile; // off the record: nothing is written to disk
		}
		connect(profile, &QWebEngineProfile::downloadRequested, profile, [](QWebEngineDownloadRequest *request) { request->cancel(); });

		auto *page = new ChatPage(profile, this);
		QWebEngineSettings *s = page->settings();
		s->setAttribute(QWebEngineSettings::PluginsEnabled, false);
		s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
		s->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
		s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
		s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
		s->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
		page->setAudioMuted(!sound_);
		page->setZoomFactor(zoom_);

		connect(page, &QWebEnginePage::loadFinished, page, [this](bool) { applyOpacity(true); });
		if (overlayOn_)
			page->setBackgroundColor(Qt::transparent);
		profile->setParent(page); // a profile has to outlive its page's last use, so it goes away together with it
		QWebEnginePage *old = view_->page();
		view_->setPage(page);
		if (old && old->parent() == this)
			old->deleteLater();
	}

	void setZoom(double z)
	{
		zoom_ = qBound(0.5, z, 3.0);
		settings_.setValue(QStringLiteral("zoom"), zoom_);
		settings_.sync();
		if (view_->page())
			view_->page()->setZoomFactor(zoom_);
	}

	QSettings settings_;
	QTimer watch_;
	QTimer discard_;
	QVBoxLayout *root_ = nullptr;
	QLabel *overlayNote_ = nullptr;
	OverlayWindow *overlay_ = nullptr;
	bool overlayOn_ = false;
	int opacity_ = 100;
	static inline ChatPanel *self_ = nullptr;
	QToolButton *reload_ = nullptr;
	QLabel *note_ = nullptr;
	QWebEngineView *view_ = nullptr;
	QString url_;
	QString sandboxNote_;
	QString lastNote_;
	bool signedIn_ = false;
	bool sound_ = false;
	double zoom_ = 1.0;
};

} // namespace

// The one entry point the app looks up. Returns the panel's widget (owned by `parent`).
HOPCAST_CHAT_EXPORT QWidget *hopcast_chat_create(QWidget *parent)
{
	return new ChatPanel(parent);
}

// The chat over a game (see OverlayWindow). 1 = on / in move mode, 0 = off; the return value is the state afterwards.
HOPCAST_CHAT_EXPORT int hopcast_chat_overlay_abi()
{
	return 1;
}

HOPCAST_CHAT_EXPORT int hopcast_chat_overlay_set(int on)
{
	ChatPanel *p = ChatPanel::instance();
	if (!p)
		return 0;
	p->setOverlay(on != 0);
	return p->overlayOn() ? 1 : 0;
}

HOPCAST_CHAT_EXPORT int hopcast_chat_overlay_move(int on)
{
	ChatPanel *p = ChatPanel::instance();
	if (!p)
		return 0;
	p->setOverlayMove(on != 0);
	return p->overlayMove() ? 1 : 0;
}

// bit 0: the overlay is on, bit 1: it is in move / resize mode
HOPCAST_CHAT_EXPORT int hopcast_chat_overlay_state()
{
	ChatPanel *p = ChatPanel::instance();
	return p ? (p->overlayOn() ? 1 : 0) | (p->overlayMove() ? 2 : 0) : 0;
}

HOPCAST_CHAT_EXPORT int hopcast_chat_abi()
{
	return 1;
}
