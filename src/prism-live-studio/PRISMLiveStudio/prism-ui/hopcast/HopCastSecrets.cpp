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
		PLSAlertView::warning(nullptr, QStringLiteral("HopperCast"), message);
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
		warnOnce(QStringLiteral("HopperCast couldn't unlock your saved stream keys because their key file (~/.local/share/hopcast/secure-store.key) is missing or unreadable.\n\n"
					"Your saved channels are kept and won't be overwritten, but they can't be shown or changed. "
					"Restore the key file (for example from a backup) and restart HopperCast."));
		return std::nullopt;
	case secure::LoadStatus::Tampered:
		PLS_ERROR(MODULE, "HopCast: %s failed integrity check: %s", qUtf8Printable(QFileInfo(path).fileName()), store.last_error().c_str());
		warnOnce(QStringLiteral("HopperCast's encrypted stream-key file was changed outside HopperCast or is damaged, so it was not loaded.\n\n"
					"It has been set aside (\"%1.unreadable-…\") and HopperCast starts with an empty list. "
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

bool secureWrite(const QString &requestedPath, const QByteArray &data)
{
	QString path = requestedPath;
	if (path == rtmpChannelsPath() && chosenFolder().isEmpty()) {
		ensureStreamKeyFolderChosen(nullptr); // first stream key: ask where to keep the file
		path = rtmpChannelsPath();
	}
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

/* The folder must exist and be writable by us; checked with a real temp file. */
bool folderUsable(const QString &folder, QString &why)
{
	QDir dir(folder);
	if (!dir.exists() && !QDir().mkpath(folder)) {
		why = QStringLiteral("The folder can't be created.");
		return false;
	}
	QTemporaryFile probe(dir.filePath(QStringLiteral(".hopcast-write-test-XXXXXX")));
	if (!probe.open()) {
		why = QStringLiteral("HopperCast can't write to this folder.");
		return false;
	}
	return true;
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

bool onGuiThread()
{
	return qApp && QThread::currentThread() == qApp->thread();
}

/* Shows the folder picker; returns the chosen folder or an empty string. */
QString pickFolder(QWidget *parent, const QString &startFolder)
{
	return QFileDialog::getExistingDirectory(parent, QStringLiteral("Choose where HopperCast keeps your stream keys"), startFolder,
						 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

} // namespace

bool ensureStreamKeyFolderChosen(QWidget *parent)
{
	if (!chosenFolder().isEmpty())
		return true;
	if (!onGuiThread()) {
		bool done = false;
		QMetaObject::invokeMethod(qApp, [&done, parent]() { done = ensureStreamKeyFolderChosen(parent); }, Qt::BlockingQueuedConnection);
		return done;
	}

	const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString suggested = documents.isEmpty() ? QDir::homePath() : documents;
	const auto answer = PLSAlertView::question(
		parent, QStringLiteral("Where should HopperCast keep your stream keys?"),
		QStringLiteral("HopperCast saves your stream keys in one encrypted file (%1). Only HopperCast can open or change it.\n\n"
			       "Choose the folder for it, for example your Documents folder. The key that unlocks it stays separately in "
			       "~/.local/share/hopcast, so the folder you pick only ever holds locked data.\n\n"
			       "You can change this later under Tools > Stream Key Storage…")
			.arg(QString::fromLatin1(FILE_NAME)),
		QMap<PLSAlertView::Button, pls_text_t>{{PLSAlertView::Button::Yes, pls_text_t(QStringLiteral("Choose Folder…"))},
						       {PLSAlertView::Button::No, pls_text_t(QStringLiteral("Use HopperCast's Folder"))}},
		PLSAlertView::Button::Yes);

	QString folder = defaultFolder();
	if (answer == PLSAlertView::Button::Yes) {
		for (;;) {
			const QString picked = pickFolder(parent, suggested);
			if (picked.isEmpty())
				break; // cancelled: fall back to HopCast's folder
			QString why;
			if (folderUsable(picked, why)) {
				folder = picked;
				break;
			}
			PLSAlertView::warning(parent, QStringLiteral("HopperCast"), why + QStringLiteral("\nPlease choose another folder."));
		}
	}
	QString why;
	if (!moveStreamKeyFile(defaultFolder(), folder, why)) {
		PLSAlertView::warning(parent, QStringLiteral("HopperCast"), why);
		folder = defaultFolder();
	}
	setChosenFolder(folder);
	PLS_INFO(MODULE, "HopCast: stream-key file folder set (%s)", folder == defaultFolder() ? "HopCast's folder" : "user-chosen folder");
	return true;
}

void changeStreamKeyFolder(QWidget *parent)
{
	const QString current = chosenFolder().isEmpty() ? defaultFolder() : chosenFolder();
	const QString picked = pickFolder(parent, current);
	if (picked.isEmpty())
		return;
	QString why;
	bool moved = folderUsable(picked, why);
	if (moved) {
		/* Move, forget the old store and switch the setting in one step, so a save
		 * running at the same time can't write to the old location. */
		QMutexLocker lock(&g_mutex);
		moved = moveStreamKeyFile(current, picked, why);
		if (moved) {
			g_stores.erase(QDir(current).filePath(QString::fromLatin1(FILE_NAME))); // reopen at the new place
			setChosenFolder(picked);
		}
	}
	if (!moved) {
		PLSAlertView::warning(parent, QStringLiteral("HopperCast"), why);
		return;
	}
	PLSAlertView::information(parent, QStringLiteral("HopperCast"),
				  QStringLiteral("Your stream keys are now kept in:\n%1").arg(QDir(picked).filePath(QString::fromLatin1(FILE_NAME))));
}

void addStreamKeyStorageMenu(QMenu *menu)
{
	if (!menu)
		return;
	auto action = menu->addAction(QStringLiteral("Stream Key Storage…"));
	QObject::connect(action, &QAction::triggered, menu, [menu]() {
		const QString current = chosenFolder().isEmpty() ? defaultFolder() : chosenFolder();
		const auto answer = PLSAlertView::question(
			menu->window(), QStringLiteral("Stream Key Storage"),
			QStringLiteral("Your encrypted stream-key file is kept in:\n%1\n\nMove it to another folder?").arg(QDir(current).filePath(QString::fromLatin1(FILE_NAME))),
			QMap<PLSAlertView::Button, pls_text_t>{{PLSAlertView::Button::Yes, pls_text_t(QStringLiteral("Choose Folder…"))},
							       {PLSAlertView::Button::Cancel, pls_text_t(QStringLiteral("Close"))}},
			PLSAlertView::Button::Cancel);
		if (answer == PLSAlertView::Button::Yes)
			changeStreamKeyFolder(menu->window());
	});
}

QString rtmpChannelsPath()
{
	const QString folder = chosenFolder();
	return QDir(folder.isEmpty() ? defaultFolder() : folder).filePath(QString::fromLatin1(FILE_NAME));
}

QString rtmpChannelsLegacyPath()
{
	return userFile(QStringLiteral("hopcast-rtmp-channels.json"));
}

} // namespace hopcast
