// HopCast: the chat panel's web view. GPL-2.0-or-later.
//
// Built as its own small library so HopCast still starts on a computer where the browser engine (Qt WebEngine) can't be
// loaded; the app opens it on demand with QLibrary (see HopCastChatDock.cpp). It shows one page, the chat link you paste,
// and nothing else: no tabs, no downloads, links inside the chat open in your normal browser.
#include <QDesktopServices>
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

		view_ = new QWebEngineView;
		view_->setObjectName(QStringLiteral("chatView"));
		root->addWidget(view_, 1);

		zoom_ = qBound(0.5, settings_.value(QStringLiteral("zoom"), 1.0).toDouble(), 3.0);

#ifdef Q_OS_LINUX
		if (!sandboxPossible()) {
			qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
			sandboxNote_ = QStringLiteral("This system doesn't allow the browser's safety sandbox, so the chat runs without it. Only load chat links you trust.");
		}
#endif

		connect(reload_, &QToolButton::clicked, this, [this]() { view_->reload(); });
		connect(smaller, &QToolButton::clicked, this, [this]() { setZoom(zoom_ - 0.1); });
		connect(bigger, &QToolButton::clicked, this, [this]() { setZoom(zoom_ + 0.1); });

		// Settings writes the file from the main program; look at it twice a second while this panel is open.
		watch_.setInterval(500);
		connect(&watch_, &QTimer::timeout, this, [this]() { refresh(); });
		watch_.start();

		signedIn_ = readSignedIn();
		sound_ = readSound();
		makePage();
		refresh(true);
	}

private:
	bool readSignedIn() { return settings_.value(QStringLiteral("signedIn"), false).toBool(); }
	bool readSound() { return settings_.value(QStringLiteral("sound"), false).toBool(); }

	// Looks at Chat.ini again and applies whatever changed in Settings.
	void refresh(bool first = false)
	{
		settings_.sync();
		const bool signedIn = readSignedIn();
		const bool sound = readSound();
		const QString typed = settings_.value(QStringLiteral("url")).toString();

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
	QToolButton *reload_ = nullptr;
	QLabel *note_ = nullptr;
	QWebEngineView *view_ = nullptr;
	QString url_;
	QString sandboxNote_;
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

HOPCAST_CHAT_EXPORT int hopcast_chat_abi()
{
	return 1;
}
