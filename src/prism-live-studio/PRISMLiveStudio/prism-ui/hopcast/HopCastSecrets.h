/* HopCast: encrypted storage for secrets (stream keys, platform tokens).
 * Qt wrapper around hopcast/securestore (AES-256-GCM; key in an owner-only key file).
 * Original code for HopCast, GPL-2.0-or-later.
 */
#pragma once

#include <QByteArray>
#include <QString>

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
QString rtmpChannelsPath();       // encrypted Custom RTMP channels (in HopCast's own folder)
QString rtmpChannelsLegacyPath(); // plaintext file from HopCast builds before encryption

/* Moves a stream-key file left in a user-chosen folder by earlier builds into HopCast's own
 * folder (once). There is no prompt: keys always live inside the app's own storage. */
void prepareKeyStorage();
} // namespace hopcast
