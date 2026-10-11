/* HopCast secure store: encrypted, tamper-evident storage for secrets such as
 * stream keys.
 *
 *   - Data is encrypted with AES-256-GCM (OpenSSL). GCM authenticates the whole
 *     file, so any edit made outside HopCast makes it fail to open instead of
 *     silently loading changed data.
 *   - The 256-bit key is random and lives in the desktop keyring (Secret Service:
 *     KWallet, GNOME Keyring, KeePassXC, ...). If no keyring is available, it falls
 *     back to an owner-only key file; that protects against other users and
 *     against copying the data file alone, but not against someone who can read
 *     your whole home directory.
 *   - Files are written atomically with mode 0600.
 *
 * File format (all binary):
 *   "HCSTORE1" (8) | key source (1: 'K' keyring, 'F' key file) | nonce (12) |
 *   ciphertext (n) | GCM tag (16)
 * The magic, key source byte and the store's purpose string are authenticated
 * as associated data.
 *
 * Original code for HopCast, GPL-2.0-or-later.
 */
#pragma once

#include <optional>
#include <string>

namespace hopcast::secure {

enum class KeySource : char { Keyring = 'K', KeyFile = 'F' };

/* ---- crypto core (no I/O) ---- */
constexpr size_t KEY_SIZE = 32;
std::optional<std::string> random_bytes(size_t count);
/* Returns the complete file contents (header + nonce + ciphertext + tag). */
std::optional<std::string> seal(const std::string &key, KeySource source, const std::string &purpose, const std::string &plaintext);
/* Header of a sealed blob, if it looks like one. */
std::optional<KeySource> sealed_key_source(const std::string &blob);
/* Plaintext, or nullopt if the blob is malformed, the key is wrong or the data was modified. */
std::optional<std::string> open(const std::string &key, const std::string &purpose, const std::string &blob);

/* ---- key management ---- */
struct KeyResult {
	std::string key;
	KeySource source;
};
/* Key provider used by Store; replaceable for tests. */
struct KeyProvider {
	virtual ~KeyProvider() = default;
	/* Look up the key for `source`; create it if missing and `create` is set. */
	virtual std::optional<std::string> get(KeySource source, bool create, std::string &error) = 0;
	/* Whether a keyring is reachable right now. */
	virtual bool keyring_available() = 0;
};
/* Desktop keyring (libsecret) with key-file fallback at `key_file_path`. */
KeyProvider &default_key_provider(const std::string &key_file_path);

/* ---- store ---- */
enum class LoadStatus {
	Ok,          // decrypted (or no file yet: empty plaintext)
	Migrated,    // a legacy plaintext file was found, encrypted and removed
	KeyMissing,  // file exists but its key is unavailable (e.g. keyring locked/absent); file untouched, saving disabled
	Tampered,    // file failed authentication; moved aside to <path>.unreadable-<time>
	IoError,
};

class Store {
public:
	Store(std::string path, std::string purpose, KeyProvider &keys, std::string legacy_plaintext_path = {});

	/* Reads and decrypts. On Ok/Migrated, `plaintext` holds the data. */
	LoadStatus load(std::string &plaintext);
	/* Encrypts and writes atomically (0600). Refused while the key is missing. */
	bool save(const std::string &plaintext, std::string &error);

	KeySource key_source() const { return m_source; }
	bool writable() const { return m_writable; }
	const std::string &last_error() const { return m_error; }

private:
	std::string m_path, m_purpose, m_legacy;
	KeyProvider &m_keys;
	KeySource m_source = KeySource::Keyring;
	bool m_sourceKnown = false;
	bool m_writable = true;
	std::string m_error;
};

/* ---- file helpers ---- */
std::optional<std::string> read_file(const std::string &path);
bool write_file_private_atomic(const std::string &path, const std::string &data, std::string &error);
/* Overwrite with zeros, then unlink (best effort; SSDs/CoW filesystems may keep old blocks). */
void scrub_and_remove(const std::string &path);

} // namespace hopcast::secure
