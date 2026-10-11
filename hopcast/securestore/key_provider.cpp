/* HopCast secure store - encryption key provider.
 *
 * Preferred: a random 256-bit key kept in the desktop keyring through the
 * freedesktop Secret Service (libsecret): KWallet (Plasma), GNOME Keyring,
 * KeePassXC, ... The keyring encrypts it with your login password.
 * Fallback (no keyring running): an owner-only (0600) key file.
 *
 * Original code for HopCast, GPL-2.0-or-later.
 */
#include "secure_store.hpp"

#include <openssl/crypto.h>

#include <cerrno>
#include <cstring>
#include <mutex>

#include <sys/stat.h>

#ifdef HOPCAST_HAVE_LIBSECRET
#include <libsecret/secret.h>
#endif

namespace hopcast::secure {

namespace {

constexpr const char *KEYRING_LABEL = "HopCast secure store key (stream keys)";
constexpr const char *KEYRING_PURPOSE = "store-key";

std::string to_hex(const std::string &bytes)
{
	static const char *digits = "0123456789abcdef";
	std::string out;
	out.reserve(bytes.size() * 2);
	for (unsigned char c : bytes) {
		out.push_back(digits[c >> 4]);
		out.push_back(digits[c & 15]);
	}
	return out;
}

std::optional<std::string> from_hex(const std::string &hex)
{
	auto val = [](char c) -> int {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return -1;
	};
	if (hex.size() != KEY_SIZE * 2)
		return std::nullopt;
	std::string out(KEY_SIZE, '\0');
	for (size_t i = 0; i < KEY_SIZE; ++i) {
		int hi = val(hex[2 * i]), lo = val(hex[2 * i + 1]);
		if (hi < 0 || lo < 0)
			return std::nullopt;
		out[i] = static_cast<char>((hi << 4) | lo);
	}
	return out;
}

std::string trim(std::string s)
{
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
		s.pop_back();
	return s;
}

#ifdef HOPCAST_HAVE_LIBSECRET
const SecretSchema *schema()
{
	static const SecretSchema s = [] {
		SecretSchema schema{}; // zero-initialises the reserved fields too
		schema.name = "io.github.thestylewithin.HopCast.SecureStore";
		schema.flags = SECRET_SCHEMA_NONE;
		schema.attributes[0].name = "purpose";
		schema.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
		return schema; // attributes[1] stays {nullptr, ...}: end of list
	}();
	return &s;
}
#endif

class DefaultKeyProvider final : public KeyProvider {
public:
	explicit DefaultKeyProvider(std::string keyFilePath) : m_keyFile(std::move(keyFilePath)) {}

	bool keyring_available() override
	{
#ifdef HOPCAST_HAVE_LIBSECRET
		std::lock_guard lock(m_mutex);
		if (!m_keyringChecked) {
			GError *err = nullptr;
			SecretService *service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &err);
			m_keyringAvailable = service != nullptr;
			if (service)
				g_object_unref(service);
			if (err)
				g_error_free(err);
			m_keyringChecked = true;
		}
		return m_keyringAvailable;
#else
		return false;
#endif
	}

	std::optional<std::string> get(KeySource source, bool create, std::string &error) override
	{
		return source == KeySource::Keyring ? fromKeyring(create, error) : fromKeyFile(create, error);
	}

private:
	std::optional<std::string> fromKeyring(bool create, std::string &error)
	{
#ifdef HOPCAST_HAVE_LIBSECRET
		if (!keyring_available()) {
			error = "no Secret Service keyring is running";
			return std::nullopt;
		}
		GError *err = nullptr;
		gchar *stored = secret_password_lookup_sync(schema(), nullptr, &err, "purpose", KEYRING_PURPOSE, nullptr);
		if (err) {
			error = err->message;
			g_error_free(err);
			return std::nullopt;
		}
		if (stored) {
			auto key = from_hex(trim(stored));
			secret_password_free(stored); // wipes memory
			if (!key)
				error = "keyring entry is malformed";
			return key;
		}
		if (!create) {
			error = "no HopCast key in the keyring";
			return std::nullopt;
		}
		auto key = random_bytes(KEY_SIZE);
		if (!key) {
			error = "random number generator failed";
			return std::nullopt;
		}
		std::string hex = to_hex(*key);
		gboolean ok = secret_password_store_sync(schema(), SECRET_COLLECTION_DEFAULT, KEYRING_LABEL, hex.c_str(), nullptr, &err, "purpose", KEYRING_PURPOSE,
							 nullptr);
		OPENSSL_cleanse(hex.data(), hex.size());
		if (!ok || err) {
			error = err ? err->message : "keyring refused to store the key";
			if (err)
				g_error_free(err);
			OPENSSL_cleanse(key->data(), key->size());
			return std::nullopt;
		}
		return key;
#else
		(void)create;
		error = "built without keyring support";
		return std::nullopt;
#endif
	}

	std::optional<std::string> fromKeyFile(bool create, std::string &error)
	{
		if (auto stored = read_file(m_keyFile); stored) {
			::chmod(m_keyFile.c_str(), S_IRUSR | S_IWUSR); // keep it owner-only
			auto key = from_hex(trim(*stored));
			OPENSSL_cleanse(stored->data(), stored->size());
			if (!key)
				error = "key file is malformed: " + m_keyFile;
			return key;
		} else if (errno != ENOENT) {
			error = "cannot read key file " + m_keyFile + ": " + std::strerror(errno);
			return std::nullopt;
		}
		if (!create) {
			error = "key file not found: " + m_keyFile;
			return std::nullopt;
		}
		auto key = random_bytes(KEY_SIZE);
		if (!key) {
			error = "random number generator failed";
			return std::nullopt;
		}
		std::string hex = to_hex(*key) + "\n";
		bool ok = write_file_private_atomic(m_keyFile, hex, error);
		OPENSSL_cleanse(hex.data(), hex.size());
		if (!ok) {
			OPENSSL_cleanse(key->data(), key->size());
			return std::nullopt;
		}
		return key;
	}

	std::string m_keyFile;
	std::mutex m_mutex;
	bool m_keyringChecked = false;
	bool m_keyringAvailable = false;
};

} // namespace

KeyProvider &default_key_provider(const std::string &key_file_path)
{
	static DefaultKeyProvider provider(key_file_path);
	return provider;
}

} // namespace hopcast::secure
