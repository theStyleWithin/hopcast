/* HopCast: encrypted storage for secrets - see HopCastSecrets.h.
 * Original code for HopCast, GPL-2.0-or-later.
 */
#include "HopCastSecrets.h"

#include "secure_store.hpp"

#include <libutils-api.h>
#include <frontend-api.h>
#include <liblog.h>
#include <PLSAlertView.h>

#include <QFileInfo>
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

secure::KeyProvider &keys()
{
	return secure::default_key_provider(userFile(QStringLiteral(".hopcast-store.key")).toStdString());
}

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
		warnOnce(QStringLiteral("HopCast couldn't unlock your saved stream keys because the desktop keyring is locked or not running.\n\n"
					"Your saved channels are kept safe but can't be shown or changed until the keyring is available. "
					"Unlock your keyring (KWallet / GNOME Keyring) and restart HopCast."));
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

QString rtmpChannelsPath()
{
	return userFile(QStringLiteral("hopcast-rtmp-channels.enc"));
}

QString rtmpChannelsLegacyPath()
{
	return userFile(QStringLiteral("hopcast-rtmp-channels.json"));
}

} // namespace hopcast
