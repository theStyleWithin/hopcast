// HopCast: settings snapshots and self-repair. GPL-2.0-or-later.
#include "HopCastSettingsGuard.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>
#include <QCoreApplication>
#include <algorithm>
#include <functional>
#include <util/base.h>

#include "frontend-api.h"

namespace {

QString g_root;      // ~/.config/PRISMLiveStudio
QString g_notice;    // told to the user once the window is up
const int kKeepAuto = 10;
const int kKeepUpgrade = 3;
const qint64 kMaxFile = 20 * 1024 * 1024;

QString dataDir()
{
	return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/hopcast");
}
QString backupsDir() { return dataDir() + QStringLiteral("/settings-backups"); }
QString keyPath() { return dataDir() + QStringLiteral("/secure-store.key"); }

void ownerOnly(const QString &path, bool dir)
{
	QFile::setPermissions(path, dir ? (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)
					: (QFileDevice::ReadOwner | QFileDevice::WriteOwner));
}

void makeDir(const QString &path)
{
	QDir().mkpath(path);
	ownerOnly(path, true);
}

// What is worth keeping: settings, profiles, scene collections, channels. Not logs, caches, browser data.
bool skipDir(const QString &name)
{
	static const QStringList skip = {QStringLiteral("logs"),       QStringLiteral("crashes"),
					 QStringLiteral("cache"),      QStringLiteral("Cache"),
					 QStringLiteral("GPUCache"),   QStringLiteral("Code Cache"),
					 QStringLiteral("obs-browser"), QStringLiteral("profiler_data")};
	return skip.contains(name);
}

QStringList settingsFiles(const QString &root)
{
	QStringList out;
	const QDir base(root);
	QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString abs = it.next();
		const QString rel = base.relativeFilePath(abs);
		bool skipped = false;
		for (const QString &part : rel.split(QLatin1Char('/')).mid(0, rel.count(QLatin1Char('/'))))
			skipped |= skipDir(part);
		if (!skipped && it.fileInfo().size() <= kMaxFile)
			out << rel;
	}
	out.sort();
	return out;
}

QString signature(const QString &root, const QStringList &files)
{
	QCryptographicHash h(QCryptographicHash::Sha1);
	for (const QString &rel : files) {
		const QFileInfo fi(root + QLatin1Char('/') + rel);
		h.addData(QStringLiteral("%1|%2|%3\n").arg(rel).arg(fi.size()).arg(fi.lastModified().toMSecsSinceEpoch()).toUtf8());
	}
	return QString::fromLatin1(h.result().toHex());
}

bool copyFile(const QString &from, const QString &to)
{
	makeDir(QFileInfo(to).absolutePath());
	QFile::remove(to);
	const bool ok = QFile::copy(from, to);
	if (ok)
		ownerOnly(to, false);
	return ok;
}

// ---- snapshots: <backups>/<name>/config/<relative path>, <backups>/<name>/key, <backups>/<name>/sig
QStringList snapshotNames()
{
	QStringList names = QDir(backupsDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	names.removeAll(QStringLiteral("broken"));
	std::sort(names.begin(), names.end(), std::greater<QString>());
	return names;
}

QString snapshotSig(const QString &name)
{
	QFile f(backupsDir() + QLatin1Char('/') + name + QStringLiteral("/sig"));
	return f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll()).trimmed() : QString();
}

void rotate()
{
	int autos = 0, upgrades = 0;
	for (const QString &name : snapshotNames()) { // newest first
		const bool upgrade = name.contains(QStringLiteral("-before-"));
		int &count = upgrade ? upgrades : autos;
		if (++count > (upgrade ? kKeepUpgrade : kKeepAuto))
			QDir(backupsDir() + QLatin1Char('/') + name).removeRecursively();
	}
}

bool snapshot(const QString &label, bool force = false)
{
	if (g_root.isEmpty() || !QDir(g_root).exists())
		return false;
	const QStringList files = settingsFiles(g_root);
	if (files.isEmpty())
		return false;
	const QString sig = signature(g_root, files) + (QFile::exists(keyPath()) ? QStringLiteral("+k") : QString());
	const QStringList names = snapshotNames();
	if (!force && !names.isEmpty() && snapshotSig(names.first()) == sig)
		return false;

	makeDir(backupsDir());
	const QString name = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QLatin1Char('-') + label;
	const QString dir = backupsDir() + QLatin1Char('/') + name;
	for (const QString &rel : files)
		copyFile(g_root + QLatin1Char('/') + rel, dir + QStringLiteral("/config/") + rel);
	if (QFile::exists(keyPath())) // without it the saved stream keys in the snapshot can't be read
		copyFile(keyPath(), dir + QStringLiteral("/key"));
	QFile s(dir + QStringLiteral("/sig"));
	if (s.open(QIODevice::WriteOnly))
		s.write(sig.toUtf8());
	ownerOnly(dir, true);
	rotate();
	blog(LOG_INFO, "HopCast: settings snapshot %s (%d files)", qUtf8Printable(name), static_cast<int>(files.size()));
	return true;
}

// ---- health: which settings files are missing, empty or unreadable
bool fileHealthy(const QString &path)
{
	const QFileInfo fi(path);
	if (!fi.exists() || fi.size() == 0)
		return false;
	if (!path.endsWith(QStringLiteral(".json")))
		return true;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return false;
	QJsonParseError err;
	QJsonDocument::fromJson(f.readAll(), &err);
	return err.error == QJsonParseError::NoError;
}

QStringList problems(const QString &root)
{
	QStringList bad;
	const QDir base(root);
	const QString global = root + QStringLiteral("/global.ini");
	if (QFile::exists(global) ? !fileHealthy(global) : (base.exists(QStringLiteral("basic")) || base.exists(QStringLiteral("user"))))
		bad << QStringLiteral("global.ini");

	// Each profile's basic.ini and each scene collection (JSON that must still parse).
	QDirIterator it(root + QStringLiteral("/basic"), QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString abs = it.next();
		const QString rel = base.relativeFilePath(abs);
		const bool profileIni = rel.contains(QStringLiteral("/profiles/")) && abs.endsWith(QStringLiteral("/basic.ini"));
		const bool scene = rel.contains(QStringLiteral("/scenes/")) && abs.endsWith(QStringLiteral(".json"));
		if ((profileIni || scene) && !fileHealthy(abs))
			bad << rel;
	}
	return bad;
}

// Put one file back from the newest snapshot that has a healthy copy; the damaged one is kept aside.
bool repairFile(const QString &rel)
{
	for (const QString &name : snapshotNames()) {
		const QString src = backupsDir() + QLatin1Char('/') + name + QStringLiteral("/config/") + rel;
		if (!fileHealthy(src))
			continue;
		const QString cur = g_root + QLatin1Char('/') + rel;
		if (QFile::exists(cur))
			copyFile(cur, backupsDir() + QStringLiteral("/broken/") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QLatin1Char('/') + rel);
		if (copyFile(src, cur)) {
			blog(LOG_WARNING, "HopCast: restored %s from snapshot %s", qUtf8Printable(rel), qUtf8Printable(name));
			return true;
		}
	}
	return false;
}

// Everything from one snapshot (used for "restore" and when the whole settings folder is gone).
bool restoreAll(const QString &name)
{
	const QString dir = backupsDir() + QLatin1Char('/') + name;
	const QString cfg = dir + QStringLiteral("/config");
	if (!QDir(cfg).exists())
		return false;
	QDirIterator it(cfg, QDir::Files, QDirIterator::Subdirectories);
	const QDir base(cfg);
	int n = 0;
	while (it.hasNext()) {
		const QString abs = it.next();
		n += copyFile(abs, g_root + QLatin1Char('/') + base.relativeFilePath(abs));
	}
	if (QFile::exists(dir + QStringLiteral("/key")) && !QFile::exists(keyPath())) {
		makeDir(dataDir());
		copyFile(dir + QStringLiteral("/key"), keyPath());
	}
	blog(LOG_WARNING, "HopCast: restored %d settings files from snapshot %s", n, qUtf8Printable(name));
	return n > 0;
}

QString versionFile() { return dataDir() + QStringLiteral("/last-version"); }

} // namespace

namespace hopcast {

void settingsGuardBeforeStartup(const QString &configRoot, const QString &version)
{
	g_root = configRoot;
	makeDir(dataDir());

	// 1. A restore the user asked for in the menu.
	const QString request = backupsDir() + QStringLiteral("/restore-request");
	QFile req(request);
	if (req.open(QIODevice::ReadOnly)) {
		const QString name = QString::fromUtf8(req.readAll()).trimmed();
		req.close();
		QFile::remove(request);
		if (QDir(backupsDir() + QLatin1Char('/') + name).exists()) {
			snapshot(QStringLiteral("before-restore"), true);
			if (restoreAll(name))
				g_notice = QStringLiteral("Your settings were restored from the backup of %1.").arg(name.left(15));
		}
	}

	// 2. Nothing there at all but we have backups (the folder was wiped): bring everything back.
	const bool empty = !QDir(g_root).exists() || settingsFiles(g_root).isEmpty();
	if (empty && !snapshotNames().isEmpty()) {
		if (restoreAll(snapshotNames().first()))
			g_notice = QStringLiteral("HopCast found its settings folder empty and restored your settings from the latest backup.");
		return;
	}

	// 3. Damaged files: put back just those.
	QStringList fixed;
	for (const QString &rel : problems(g_root))
		if (repairFile(rel))
			fixed << rel;
	if (!fixed.isEmpty())
		g_notice = QStringLiteral("HopCast repaired %1 damaged settings file(s) from a backup.").arg(fixed.size());

	// 4. First run of a different version: keep a "before" snapshot, whatever the new version does next.
	QFile last(versionFile());
	const QString was = last.open(QIODevice::ReadOnly) ? QString::fromUtf8(last.readAll()).trimmed() : QString();
	if (was != version)
		snapshot(QStringLiteral("before-") + version, true);
}

void settingsGuardAfterStartup(const QString &version)
{
	QFile v(versionFile());
	if (v.open(QIODevice::WriteOnly | QIODevice::Truncate))
		v.write(version.toUtf8());
	if (!g_notice.isEmpty()) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, g_notice);
		g_notice.clear();
	}
	snapshot(QStringLiteral("auto"));
	auto *timer = new QTimer(qApp);
	QObject::connect(timer, &QTimer::timeout, qApp, [] { snapshot(QStringLiteral("auto")); });
	timer->start(15 * 60 * 1000);
	QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [] { snapshot(QStringLiteral("exit")); });
}

bool settingsBackupNow()
{
	return snapshot(QStringLiteral("manual"));
}

QStringList settingsBackupNames()
{
	return snapshotNames();
}

bool settingsScheduleRestore(const QString &name)
{
	if (!QDir(backupsDir() + QLatin1Char('/') + name).exists())
		return false;
	QFile f(backupsDir() + QStringLiteral("/restore-request"));
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(name.toUtf8());
	ownerOnly(f.fileName(), false);
	return true;
}

} // namespace hopcast
