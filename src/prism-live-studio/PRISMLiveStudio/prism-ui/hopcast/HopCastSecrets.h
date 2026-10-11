/* HopCast: encrypted storage for secrets (stream keys, platform tokens).
 * Qt wrapper around hopcast/securestore (AES-256-GCM; key in an owner-only key file).
 * Original code for HopCast, GPL-2.0-or-later.
 */
#pragma once

#include <QByteArray>
#include <QString>

class QMenu;
class QWidget;

#include <optional>

namespace hopcast {

/* Read an encrypted file. If it doesn't exist yet and `legacyPlainPath` exists,
 * that plaintext file is encrypted into `path` and securely removed.
 * Returns nullopt if there is nothing to read or it can't be decrypted
 * (the user is told once; see README "Stream keys"). */
std::optional<QByteArray> secureRead(const QString &path, const QString &legacyPlainPath = {});

/* Encrypt and write atomically (0600). Refused if the existing file's key is unavailable. */
bool secureWrite(const QString &path, const QByteArray &data);

/* Files this module manages. */
QString rtmpChannelsPath();       // encrypted Custom RTMP channels (in the folder the user chose)
QString rtmpChannelsLegacyPath(); // plaintext file from HopCast builds before encryption

/* Where the stream-key file lives. The first time HopCast saves a stream key it asks
 * the user to pick a folder (Documents is suggested); the choice is remembered. */
bool ensureStreamKeyFolderChosen(QWidget *parent = nullptr);
/* Lets the user pick another folder and moves the file there (Tools menu). */
void changeStreamKeyFolder(QWidget *parent = nullptr);
/* Adds "Stream Key Storage…" to the given menu. */
void addStreamKeyStorageMenu(QMenu *menu);

} // namespace hopcast
