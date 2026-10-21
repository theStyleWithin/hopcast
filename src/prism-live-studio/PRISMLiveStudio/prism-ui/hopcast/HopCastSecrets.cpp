/* HopCast: encrypted storage for secrets - see HopCastSecrets.h.
 * Original code for HopCast, GPL-2.0-or-later.
 */
#include "HopCastSecrets.h"

#include "secure_store.hpp"

#include <libutils-api.h>
#include <frontend-api.h>
#include <liblog.h>
#include <PLSAlertView.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QMap>
#include <QMenu>
#include <QSettings>
#include <QThread>
#include <QTemporaryFile>
#include <QMutex>

#include <map>
#include <mutex>
#include <memory>

namespace hopcast {

namespace {

constexpr auto MODULE = "HopCastSecrets";

QString userFile(const QString &relative)
{
	return pls_get_user_path(QStringLiteral("PRISMLiveStudio/user/") + relative);
}

/* The key is kept apart from the encrypted files: data in ~/.config/PRISMLiveStudio,
 * key in ~/.local/share/hopcast. A copy or dotfile backup of ~/.config alone then
 * holds only locked data. Owner-only (0600) either way; no keyring is used. */
QString keyFilePath()
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/hopcast");
	const QString path = dir + QStringLiteral("/secure-store.key");
	const QString old = userFile(QStringLiteral(".hopcast-store.key")); // earlier HopCast builds
	if (!QFile::exists(path) && QFile::exists(old)) {
		QDir().mkpath(dir);
		QFile::rename(old, path);
	}
	QDir().mkpath(dir);
	QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
	return path;
}

secure::KeyProvider &keys()
{
	return secure::default_key_provider(keyFilePath().toStdString());
}

QString chosenFolder(); // stream-key folder choice, defined below

QMutex g_mutex;
std::map<QString, std::unique_ptr<secure::Store>> g_stores;
bool g_warned = false;

secure::Store &storeFor(const QString &path, const QString &legacy)
{
	auto &store = g_stores[path];
	if (!store) {
		/* The file name is the store's purpose: a file can't be swapped for another one. */
		store = std::make_unique<secure::Store>(path.toStdString(), QFileInfo(path).fileName().toStdString(), keys(), legacy.toStdString());
	}
	return *store;
}

void warnOnce(const QString &message)
{
	if (g_warned)
		return;
	g_warned = true;
	pls_async_call_mt([message]() {
		PLSAlertView::warning(nullptr, QStringLiteral("HopCast"), message);
	});
}

} // namespace

std::optional<QByteArray> secureRead(const QString &path, const QString &legacyPlainPath)
{
	QMutexLocker lock(&g_mutex);
	auto &store = storeFor(path, legacyPlainPath);
	std::string plaintext;
	const auto status = store.load(plaintext);
	switch (status) {
	case secure::LoadStatus::Ok:
		break;
	case secure::LoadStatus::Migrated:
		PLS_INFO(MODULE, "HopCast: encrypted legacy file and removed the plaintext copy: %s", qUtf8Printable(QFileInfo(legacyPlainPath).fileName()));
		break;
	case secure::LoadStatus::KeyMissing:
		PLS_ERROR(MODULE, "HopCast: cannot unlock %s: %s", qUtf8Printable(QFileInfo(path).fileName()), store.last_error().c_str());
		warnOnce(QStringLiteral("HopCast couldn't unlock your saved stream keys because their key file (~/.local/share/hopcast/secure-store.key) is missing or unreadable.\n\n"
					"Your saved channels are kept and won't be overwritten, but they can't be shown or changed. "
					"Restore the key file (for example from a backup) and restart HopCast."));
		return std::nullopt;
	case secure::LoadStatus::Tampered:
		PLS_ERROR(MODULE, "HopCast: %s failed integrity check: %s", qUtf8Printable(QFileInfo(path).fileName()), store.last_error().c_str());
		warnOnce(QStringLiteral("HopCast's encrypted stream-key file was changed outside HopCast or is damaged, so it was not loaded.\n\n"
					"It has been set aside (\"%1.unreadable-…\") and HopCast starts with an empty list. "
					"Please re-enter your stream keys.")
				 .arg(QFileInfo(path).fileName()));
		return std::nullopt;
	case secure::LoadStatus::IoError:
		PLS_ERROR(MODULE, "HopCast: reading %s failed: %s", qUtf8Printable(QFileInfo(path).fileName()), store.last_error().c_str());
		if (plaintext.empty())
			return std::nullopt;
		break;
	}
	if (plaintext.empty())
		return std::nullopt;
	return QByteArray(plaintext.data(), static_cast<qsizetype>(plaintext.size()));
}

bool secureWrite(const QString &path, const QByteArray &data)
{
	QMutexLocker lock(&g_mutex);
	auto &store = storeFor(path, {});
	std::string error;
	if (!store.save(std::string(data.constData(), static_cast<size_t>(data.size())), error)) {
		PLS_ERROR(MODULE, "HopCast: saving %s failed: %s", qUtf8Printable(QFileInfo(path).fileName()), error.c_str());
		return false;
	}
	PLS_INFO(MODULE, "HopCast: saved %s encrypted (key in %s)", qUtf8Printable(QFileInfo(path).fileName()),
		 store.key_source() == secure::KeySource::Keyring ? "desktop keyring" : "owner-only key file");
	return true;
}

/* ---- stream-key folder choice ------------------------------------------- */

namespace {

constexpr auto FILE_NAME = "hopcast-rtmp-channels.enc";
constexpr auto SETTING_FOLDER = "secureStore/streamKeyFolder";

QSettings &settings()
{
	/* Not secret: just remembers the folder. */
	static QSettings s(userFile(QStringLiteral("hopcast.ini")), QSettings::IniFormat);
	return s;
}

QString defaultFolder()
{
	return QFileInfo(userFile(QString::fromLatin1(FILE_NAME))).absolutePath();
}

/* The folder setting is read from worker threads (saves) and written from the GUI
 * thread; one QSettings object isn't safe to share without a lock. */
QMutex g_settingsMutex;

QString chosenFolder()
{
	QMutexLocker lock(&g_settingsMutex);
	return settings().value(SETTING_FOLDER).toString();
}

void setChosenFolder(const QString &folder)
{
	QMutexLocker lock(&g_settingsMutex);
	settings().setValue(SETTING_FOLDER, folder);
	settings().sync();
}

/* Move the encrypted file (if any) from `from` to `to` folder. */
bool moveStreamKeyFile(const QString &fromFolder, const QString &toFolder, QString &why)
{
	const QString src = QDir(fromFolder).filePath(QString::fromLatin1(FILE_NAME));
	const QString dst = QDir(toFolder).filePath(QString::fromLatin1(FILE_NAME));
	if (QFileInfo(src).absoluteFilePath() == QFileInfo(dst).absoluteFilePath() || !QFile::exists(src))
		return true;
	if (QFile::exists(dst)) {
		why = QStringLiteral("That folder already contains a %1 file. Move or rename it first, so nothing gets overwritten.").arg(QString::fromLatin1(FILE_NAME));
		return false;
	}
	if (!QFile::rename(src, dst)) { // also works across drives (copy + remove)
		why = QStringLiteral("The file couldn't be moved there.");
		return false;
	}
	QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
	return true;
}

} // namespace

/* HopCast keeps the encrypted channel file in its own folder; there is no prompt and no
 * choice. Builds before 0.1.4 let the user pick a folder: if one was picked, the file is
 * moved into HopCast's folder once, and the old setting is dropped. If the move isn't
 * possible, the file stays where it is and keeps working. */
void prepareKeyStorage()
{
	static std::once_flag once;
	std::call_once(once, []() {
		const QString chosen = chosenFolder();
		if (chosen.isEmpty())
			return;
		QString why;
		QMutexLocker lock(&g_mutex);
		if (moveStreamKeyFile(chosen, defaultFolder(), why)) {
			g_stores.erase(QDir(chosen).filePath(QString::fromLatin1(FILE_NAME)));
			setChosenFolder(QString());
			PLS_INFO(MODULE, "HopCast: moved the stream-key file from an earlier chosen folder into HopCast's own folder");
		} else {
			PLS_WARN(MODULE, "HopCast: stream-key file stays in the earlier chosen folder (%s)", qUtf8Printable(why));
		}
	});
}

QString rtmpChannelsPath()
{
	prepareKeyStorage();
	const QString folder = chosenFolder();
	return QDir(folder.isEmpty() ? defaultFolder() : folder).filePath(QString::fromLatin1(FILE_NAME));
}

QString rtmpChannelsLegacyPath()
{
	return userFile(QStringLiteral("hopcast-rtmp-channels.json"));
}

} // namespace hopcast
