// HopCast: desktop screenshots for the system tray menu. GPL-2.0-or-later.
#include "HopCastScreenshot.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QRandomGenerator>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace {

using Done = std::function<void(const QString &, const QString &)>;

QString newScreenshotPath()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	if (dir.isEmpty())
		dir = QDir::homePath();
	dir += QStringLiteral("/HopCast");
	QDir().mkpath(dir);
	return dir + QStringLiteral("/Screenshot ") +
	       QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh-mm-ss")) + QStringLiteral(".png");
}

bool x11Available()
{
	return QGuiApplication::platformName() == QLatin1String("xcb") &&
	       qgetenv("XDG_SESSION_TYPE") != "wayland" && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
}

bool grabAllScreens(const QString &path)
{
	const auto screens = QGuiApplication::screens();
	if (screens.isEmpty())
		return false;

	QRect all;
	qreal ratio = 1.0;
	for (QScreen *s : screens) {
		all = all.united(s->geometry());
		ratio = qMax(ratio, s->devicePixelRatio());
	}

	QImage out(all.size() * ratio, QImage::Format_ARGB32);
	out.setDevicePixelRatio(ratio);
	out.fill(Qt::black);
	QPainter painter(&out);
	bool any = false;
	for (QScreen *s : screens) {
		const QPixmap shot = s->grabWindow(0);
		if (shot.isNull())
			continue;
		painter.drawPixmap(s->geometry().topLeft() - all.topLeft(), shot);
		any = true;
	}
	painter.end();
	return any && out.save(path, "PNG");
}

// Waits for the portal's answer to one Screenshot request, then reports it and deletes itself.
class PortalWaiter : public QObject {
	Q_OBJECT
public:
	PortalWaiter(Done done, QString path) : done(std::move(done)), path(std::move(path)) {}

	void finish(const QString &error)
	{
		if (finished)
			return;
		finished = true;
		if (error.isEmpty())
			done(path, QString());
		else
			done(QString(), error);
		deleteLater();
	}

public slots:
	void onResponse(uint response, const QVariantMap &results)
	{
		if (response != 0) {
			finish(response == 1 ? QStringLiteral("Screenshot cancelled.")
					     : QStringLiteral("The desktop refused to take a screenshot."));
			return;
		}
		const QString source = QUrl(results.value(QStringLiteral("uri")).toString()).toLocalFile();
		if (source.isEmpty() || !QFile::exists(source)) {
			finish(QStringLiteral("The desktop did not return a screenshot file."));
			return;
		}
		if (!QFile::rename(source, path)) {
			if (!QFile::copy(source, path)) {
				finish(QStringLiteral("Couldn't save the screenshot to ") + path);
				return;
			}
			QFile::remove(source);
		}
		finish(QString());
	}

private:
	Done done;
	QString path;
	bool finished = false;
};

void portalScreenshot(const QString &path, Done done)
{
	QDBusConnection bus = QDBusConnection::sessionBus();
	if (!bus.isConnected()) {
		done(QString(), QStringLiteral("No desktop session bus; can't take a screenshot."));
		return;
	}

	const QString sender = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
	const QString token = QStringLiteral("hopcast%1").arg(QRandomGenerator::global()->generate());
	const QString requestPath =
		QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

	auto *waiter = new PortalWaiter(std::move(done), path);
	bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), requestPath,
		    QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), waiter,
		    SLOT(onResponse(uint, QVariantMap)));

	QVariantMap options;
	options.insert(QStringLiteral("handle_token"), token);
	options.insert(QStringLiteral("interactive"), false);

	QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
							   QStringLiteral("/org/freedesktop/portal/desktop"),
							   QStringLiteral("org.freedesktop.portal.Screenshot"),
							   QStringLiteral("Screenshot"));
	call << QString() << options;

	auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), waiter);
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished, waiter, [waiter](QDBusPendingCallWatcher *w) {
		if (w->isError())
			waiter->finish(QStringLiteral("The desktop screenshot service isn't available (%1).")
					       .arg(w->error().message()));
	});

	// Don't wait forever if the user ignores a permission prompt.
	QTimer::singleShot(120000, waiter, [waiter]() { waiter->finish(QStringLiteral("Screenshot timed out.")); });
}

} // namespace

namespace hopcast {

void takeDesktopScreenshot(std::function<void(const QString &path, const QString &error)> done)
{
	const QString path = newScreenshotPath();

	if (x11Available()) {
		// Give the tray menu a moment to disappear so it isn't in the picture.
		QTimer::singleShot(400, qApp, [path, done]() {
			if (grabAllScreens(path))
				done(path, QString());
			else
				portalScreenshot(path, done);
		});
		return;
	}
	portalScreenshot(path, std::move(done));
}

} // namespace hopcast

#include "HopCastScreenshot.moc"
