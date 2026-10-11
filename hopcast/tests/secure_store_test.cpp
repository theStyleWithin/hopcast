/* Tests for hopcast/securestore. Build and run: hopcast/scripts/test-securestore.sh
 * Set HOPCAST_TEST_KEYRING=1 to require that the real desktop keyring is used.
 * Original code for HopCast, GPL-2.0-or-later.
 */
#include "../securestore/secure_store.hpp"

#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace hopcast::secure;

static int g_failures = 0;
#define CHECK(cond)                                                              \
	do {                                                                     \
		if (cond) {                                                      \
			std::printf("  ok   %s\n", #cond);                           \
		} else {                                                         \
			std::printf("  FAIL %s  (%s:%d)\n", #cond, __FILE__, __LINE__); \
			++g_failures;                                            \
		}                                                                \
	} while (0)

struct FakeKeys : KeyProvider {
	std::map<char, std::string> keys;
	bool keyring = true;
	bool locked = false; // simulates a locked/absent keyring for existing keys
	std::optional<std::string> get(KeySource s, bool create, std::string &error) override
	{
		if (locked) {
			error = "locked";
			return std::nullopt;
		}
		auto it = keys.find(static_cast<char>(s));
		if (it != keys.end())
			return it->second;
		if (!create) {
			error = "missing";
			return std::nullopt;
		}
		return keys[static_cast<char>(s)] = *random_bytes(KEY_SIZE);
	}
	bool keyring_available() override { return keyring; }
};

static bool contains(const std::string &hay, const std::string &needle)
{
	return hay.find(needle) != std::string::npos;
}

static mode_t file_mode(const std::string &path)
{
	struct stat st {};
	return ::stat(path.c_str(), &st) == 0 ? (st.st_mode & 0777) : 0;
}

static int count_prefix(const std::string &dir, const std::string &prefix)
{
	int n = 0;
	if (DIR *d = opendir(dir.c_str())) {
		while (dirent *e = readdir(d))
			if (std::string(e->d_name).rfind(prefix, 0) == 0)
				++n;
		closedir(d);
	}
	return n;
}

int main()
{
	char tmpl[] = "/tmp/hopcast-securestore-XXXXXX";
	const std::string dir = mkdtemp(tmpl);
	const std::string secretKey = "live_123456789_SuperSecretStreamKey";
	const std::string json = "[{\"customUserDataSeq\":\"1\",\"customData\":{\"streamKey\":\"" + secretKey + "\"}}]";

	std::puts("crypto core");
	auto key = *random_bytes(KEY_SIZE), otherKey = *random_bytes(KEY_SIZE);
	auto blob = seal(key, KeySource::Keyring, "rtmp-channels", json);
	CHECK(blob.has_value());
	CHECK(!contains(*blob, secretKey));
	CHECK(open(key, "rtmp-channels", *blob) == json);
	CHECK(!open(otherKey, "rtmp-channels", *blob));
	CHECK(!open(key, "other-purpose", *blob));
	for (size_t pos : {size_t(3), size_t(8), size_t(15), blob->size() / 2, blob->size() - 1}) {
		std::string t = *blob;
		t[pos] ^= 0x01;
		CHECK(!open(key, "rtmp-channels", t));
	}
	CHECK(!open(key, "rtmp-channels", blob->substr(0, blob->size() - 1)));
	auto blob2 = seal(key, KeySource::Keyring, "rtmp-channels", json);
	CHECK(*blob != *blob2); // fresh nonce every time
	CHECK(open(key, "p", *seal(key, KeySource::KeyFile, "p", "")) == std::string()); // empty data round-trips

	std::puts("store: save/load, permissions");
	FakeKeys fake;
	const std::string path = dir + "/channels.enc";
	{
		Store s(path, "rtmp-channels", fake);
		std::string pt, err;
		CHECK(s.load(pt) == LoadStatus::Ok && pt.empty());
		CHECK(s.save(json, err));
		CHECK(s.key_source() == KeySource::Keyring);
		CHECK(file_mode(path) == 0600);
		CHECK(!contains(*read_file(path), secretKey));
	}
	{
		Store s(path, "rtmp-channels", fake);
		std::string pt;
		CHECK(s.load(pt) == LoadStatus::Ok && pt == json);
	}

	std::puts("store: outside edit is detected and the file kept aside");
	{
		auto data = *read_file(path);
		data[data.size() - 5] ^= 0x20;
		std::string err;
		write_file_private_atomic(path, data, err);
		Store s(path, "rtmp-channels", fake);
		std::string pt;
		CHECK(s.load(pt) == LoadStatus::Tampered);
		CHECK(pt.empty());
		CHECK(access(path.c_str(), F_OK) != 0);
		CHECK(count_prefix(dir, "channels.enc.unreadable-") == 1);
		CHECK(s.save(json, err)); // starts fresh
	}

	std::puts("store: key unavailable -> read-only, file untouched");
	{
		auto before = *read_file(path);
		fake.locked = true;
		Store s(path, "rtmp-channels", fake);
		std::string pt, err;
		CHECK(s.load(pt) == LoadStatus::KeyMissing);
		CHECK(!s.writable());
		CHECK(!s.save("[]", err));
		CHECK(*read_file(path) == before);
		fake.locked = false;
	}

	std::puts("store: legacy plaintext migration");
	{
		const std::string legacy = dir + "/legacy.json", enc = dir + "/migrated.enc";
		std::string err;
		write_file_private_atomic(legacy, json, err);
		Store s(enc, "rtmp-channels", fake, legacy);
		std::string pt;
		CHECK(s.load(pt) == LoadStatus::Migrated && pt == json);
		CHECK(access(legacy.c_str(), F_OK) != 0);
		CHECK(!contains(*read_file(enc), secretKey));
	}

	std::puts("store: no keyring -> key file fallback");
	{
		fake.keyring = false;
		const std::string enc = dir + "/fallback.enc";
		Store s(enc, "rtmp-channels", fake);
		std::string pt, err;
		s.load(pt);
		CHECK(s.save(json, err) && s.key_source() == KeySource::KeyFile);
		fake.keyring = true;
		Store again(enc, "rtmp-channels", fake);
		CHECK(again.load(pt) == LoadStatus::Ok && pt == json); // stays on its own key source
	}

	std::puts("default key provider (keyring, or key file fallback)");
	{
		const std::string keyFile = dir + "/store.key", enc = dir + "/real.enc";
		KeyProvider &real = default_key_provider(keyFile);
		const bool keyring = real.keyring_available();
		std::printf("  keyring available: %s\n", keyring ? "yes" : "no");
		if (std::getenv("HOPCAST_TEST_KEYRING"))
			CHECK(keyring);
		Store s(enc, "rtmp-channels", real);
		std::string pt, err;
		s.load(pt);
		CHECK(s.save(json, err));
		if (!err.empty())
			std::printf("  error: %s\n", err.c_str());
		const bool usedKeyring = s.key_source() == KeySource::Keyring;
		std::printf("  key stored in: %s\n", usedKeyring ? "desktop keyring" : "owner-only key file");
		if (std::getenv("HOPCAST_TEST_KEYRING"))
			CHECK(usedKeyring); // a working, unlocked keyring must be used
		if (!keyring)
			CHECK(!usedKeyring);
		if (!usedKeyring) // fallback (no keyring, or it refused to store the key)
			CHECK(file_mode(keyFile) == 0600);
		else
			CHECK(access(keyFile.c_str(), F_OK) != 0); // no key file when the keyring holds the key
		Store again(enc, "rtmp-channels", real);
		CHECK(again.load(pt) == LoadStatus::Ok && pt == json);
	}

	std::string cmd = "rm -rf '" + dir + "'";
	if (std::system(cmd.c_str()) != 0)
		std::puts("  (could not remove temp dir)");
	std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures, g_failures == 1 ? "" : "s");
	return g_failures ? 1 : 0;
}
