/* HopCast secure store - crypto core, store logic and file helpers.
 * See secure_store.hpp. Original code for HopCast, GPL-2.0-or-later.
 */
#include "secure_store.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hopcast::secure {

namespace {
constexpr char MAGIC[] = "HCSTORE1";
constexpr size_t MAGIC_SIZE = 8;
constexpr size_t NONCE_SIZE = 12;
constexpr size_t TAG_SIZE = 16;
constexpr size_t HEADER_SIZE = MAGIC_SIZE + 1;

struct CtxDeleter {
	void operator()(EVP_CIPHER_CTX *c) const { EVP_CIPHER_CTX_free(c); }
};
using Ctx = std::unique_ptr<EVP_CIPHER_CTX, CtxDeleter>;

std::string associated_data(KeySource source, const std::string &purpose)
{
	std::string aad(MAGIC, MAGIC_SIZE);
	aad.push_back(static_cast<char>(source));
	aad += '\0';
	aad += purpose;
	return aad;
}

bool valid_source(char c)
{
	return c == static_cast<char>(KeySource::Keyring) || c == static_cast<char>(KeySource::KeyFile);
}
} // namespace

/* ------------------------------------------------------------------ crypto */

std::optional<std::string> random_bytes(size_t count)
{
	std::string out(count, '\0');
	if (RAND_bytes(reinterpret_cast<unsigned char *>(out.data()), static_cast<int>(count)) != 1)
		return std::nullopt;
	return out;
}

std::optional<std::string> seal(const std::string &key, KeySource source, const std::string &purpose, const std::string &plaintext)
{
	if (key.size() != KEY_SIZE)
		return std::nullopt;
	auto nonce = random_bytes(NONCE_SIZE);
	if (!nonce)
		return std::nullopt;

	Ctx ctx(EVP_CIPHER_CTX_new());
	if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
	    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, NONCE_SIZE, nullptr) != 1 ||
	    EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char *>(key.data()),
			       reinterpret_cast<const unsigned char *>(nonce->data())) != 1)
		return std::nullopt;

	int len = 0;
	const std::string aad = associated_data(source, purpose);
	if (EVP_EncryptUpdate(ctx.get(), nullptr, &len, reinterpret_cast<const unsigned char *>(aad.data()), static_cast<int>(aad.size())) != 1)
		return std::nullopt;

	std::string out(MAGIC, MAGIC_SIZE);
	out.push_back(static_cast<char>(source));
	out += *nonce;
	const size_t ctOffset = out.size();
	out.resize(ctOffset + plaintext.size() + TAG_SIZE);
	auto *ct = reinterpret_cast<unsigned char *>(out.data() + ctOffset);
	if (!plaintext.empty() &&
	    EVP_EncryptUpdate(ctx.get(), ct, &len, reinterpret_cast<const unsigned char *>(plaintext.data()), static_cast<int>(plaintext.size())) != 1)
		return std::nullopt;
	int finalLen = 0;
	if (EVP_EncryptFinal_ex(ctx.get(), ct + plaintext.size(), &finalLen) != 1)
		return std::nullopt;
	if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE, ct + plaintext.size()) != 1)
		return std::nullopt;
	return out;
}

std::optional<KeySource> sealed_key_source(const std::string &blob)
{
	if (blob.size() < HEADER_SIZE + NONCE_SIZE + TAG_SIZE || blob.compare(0, MAGIC_SIZE, MAGIC, MAGIC_SIZE) != 0 || !valid_source(blob[MAGIC_SIZE]))
		return std::nullopt;
	return static_cast<KeySource>(blob[MAGIC_SIZE]);
}

std::optional<std::string> open(const std::string &key, const std::string &purpose, const std::string &blob)
{
	auto source = sealed_key_source(blob);
	if (!source || key.size() != KEY_SIZE)
		return std::nullopt;

	const auto *nonce = reinterpret_cast<const unsigned char *>(blob.data() + HEADER_SIZE);
	const size_t ctOffset = HEADER_SIZE + NONCE_SIZE;
	const size_t ctSize = blob.size() - ctOffset - TAG_SIZE;
	const auto *ct = reinterpret_cast<const unsigned char *>(blob.data() + ctOffset);
	std::string tag = blob.substr(blob.size() - TAG_SIZE);

	Ctx ctx(EVP_CIPHER_CTX_new());
	if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
	    EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, NONCE_SIZE, nullptr) != 1 ||
	    EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char *>(key.data()), nonce) != 1)
		return std::nullopt;

	int len = 0;
	const std::string aad = associated_data(*source, purpose);
	if (EVP_DecryptUpdate(ctx.get(), nullptr, &len, reinterpret_cast<const unsigned char *>(aad.data()), static_cast<int>(aad.size())) != 1)
		return std::nullopt;

	std::string plaintext(ctSize, '\0');
	if (ctSize && EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(plaintext.data()), &len, ct, static_cast<int>(ctSize)) != 1)
		return std::nullopt;
	if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag.data()) != 1)
		return std::nullopt;
	int finalLen = 0;
	if (EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(plaintext.data()) + ctSize, &finalLen) != 1) {
		OPENSSL_cleanse(plaintext.data(), plaintext.size());
		return std::nullopt; // wrong key or modified data
	}
	return plaintext;
}

/* ------------------------------------------------------------------ files */

std::optional<std::string> read_file(const std::string &path)
{
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
		return std::nullopt;
	std::string data;
	char buf[8192];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
		data.append(buf, n);
	const bool ok = !std::ferror(f);
	std::fclose(f);
	if (!ok)
		return std::nullopt;
	return data;
}

bool write_file_private_atomic(const std::string &path, const std::string &data, std::string &error)
{
	const std::string dir = path.substr(0, path.find_last_of('/'));
	std::string tmp = path + ".tmp-XXXXXX";
	int fd = mkstemp(tmp.data()); // created with mode 0600
	if (fd < 0) {
		error = "cannot create temporary file in " + dir + ": " + std::strerror(errno);
		return false;
	}
	fchmod(fd, S_IRUSR | S_IWUSR);
	size_t off = 0;
	while (off < data.size()) {
		ssize_t w = ::write(fd, data.data() + off, data.size() - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			error = std::string("write failed: ") + std::strerror(errno);
			::close(fd);
			::unlink(tmp.c_str());
			return false;
		}
		off += static_cast<size_t>(w);
	}
	if (::fsync(fd) != 0 || ::close(fd) != 0) {
		error = std::string("fsync/close failed: ") + std::strerror(errno);
		::unlink(tmp.c_str());
		return false;
	}
	if (::rename(tmp.c_str(), path.c_str()) != 0) {
		error = std::string("rename failed: ") + std::strerror(errno);
		::unlink(tmp.c_str());
		return false;
	}
	if (int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY); dfd >= 0) {
		::fsync(dfd);
		::close(dfd);
	}
	return true;
}

void scrub_and_remove(const std::string &path)
{
	struct stat st {};
	if (::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
		if (int fd = ::open(path.c_str(), O_WRONLY); fd >= 0) {
			std::string zeros(4096, '\0');
			for (off_t left = st.st_size; left > 0;) {
				ssize_t w = ::write(fd, zeros.data(), static_cast<size_t>(std::min<off_t>(left, static_cast<off_t>(zeros.size()))));
				if (w <= 0)
					break;
				left -= w;
			}
			::fsync(fd);
			::close(fd);
		}
	}
	::unlink(path.c_str());
}

/* ------------------------------------------------------------------ store */

Store::Store(std::string path, std::string purpose, KeyProvider &keys, std::string legacy_plaintext_path)
	: m_path(std::move(path)), m_purpose(std::move(purpose)), m_legacy(std::move(legacy_plaintext_path)), m_keys(keys)
{
}

LoadStatus Store::load(std::string &plaintext)
{
	plaintext.clear();
	m_error.clear();

	if (auto blob = read_file(m_path); blob) {
		auto source = sealed_key_source(*blob);
		auto moveAside = [this](const std::string &why) {
			char stamp[32];
			std::time_t now = std::time(nullptr);
			std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
			const std::string aside = m_path + ".unreadable-" + stamp;
			std::rename(m_path.c_str(), aside.c_str());
			m_error = why + "; moved to " + aside;
			m_sourceKnown = false;
			m_writable = true;
		};
		if (!source) {
			moveAside("not a HopCast secure store file");
			return LoadStatus::Tampered;
		}
		m_source = *source;
		m_sourceKnown = true;

		std::string keyError;
		auto key = m_keys.get(m_source, false, keyError);
		if (!key) {
			m_writable = false; // never overwrite data we cannot read
			m_error = (m_source == KeySource::Keyring ? "the desktop keyring is unavailable or locked: " : "the key file is missing: ") + keyError;
			return LoadStatus::KeyMissing;
		}
		auto opened = open(*key, m_purpose, *blob);
		OPENSSL_cleanse(key->data(), key->size());
		if (!opened) {
			moveAside("authentication failed (file was modified, corrupted, or encrypted with another key)");
			return LoadStatus::Tampered;
		}
		plaintext = std::move(*opened);
		m_writable = true;
		return LoadStatus::Ok;
	} else if (errno != ENOENT) {
		m_error = std::string("cannot read ") + m_path + ": " + std::strerror(errno);
		m_writable = false;
		return LoadStatus::IoError;
	}

	/* No encrypted file yet: migrate a legacy plaintext file if there is one. */
	if (!m_legacy.empty()) {
		if (auto legacy = read_file(m_legacy); legacy) {
			std::string saveError;
			if (save(*legacy, saveError)) {
				scrub_and_remove(m_legacy);
				plaintext = std::move(*legacy);
				return LoadStatus::Migrated;
			}
			m_error = "could not encrypt legacy file: " + saveError;
			plaintext = std::move(*legacy);
			return LoadStatus::IoError;
		}
	}
	return LoadStatus::Ok;
}

bool Store::save(const std::string &plaintext, std::string &error)
{
	if (!m_writable) {
		error = "store is read-only because its key is unavailable (" + m_error + ")";
		return false;
	}
	KeySource source = m_sourceKnown ? m_source : (m_keys.keyring_available() ? KeySource::Keyring : KeySource::KeyFile);
	std::string keyError;
	auto key = m_keys.get(source, true, keyError);
	if (!key && !m_sourceKnown && source == KeySource::Keyring) {
		source = KeySource::KeyFile; // keyring vanished between the check and the call
		key = m_keys.get(source, true, keyError);
	}
	if (!key) {
		error = "no encryption key: " + keyError;
		return false;
	}
	auto blob = seal(*key, source, m_purpose, plaintext);
	OPENSSL_cleanse(key->data(), key->size());
	if (!blob) {
		error = "encryption failed";
		return false;
	}
	if (!write_file_private_atomic(m_path, *blob, error))
		return false;
	m_source = source;
	m_sourceKnown = true;
	return true;
}

} // namespace hopcast::secure
