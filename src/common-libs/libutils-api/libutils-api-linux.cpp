/* HopCast (Linux): POSIX/Qt implementation of the pls_libutil_api_mac helper API.
 *
 * NAVER's libutils-api.cpp implements its non-Windows code paths on top of the
 * helper interface declared in libutils-api-mac.h (implemented with Cocoa in
 * libutils-api-mac.mm). Providing the same interface on Linux lets those code
 * paths compile and run unchanged. The namespace name is kept for that reason;
 * nothing here uses macOS APIs.
 *
 * Layout assumptions (portable / AppImage):
 *   <prefix>/bin/<executable>
 *   <prefix>/share/hopcast/          resources ("data" lives below it)
 *   <prefix>/lib/hopcast/plugins/    PRISM plugins
 * User data lives in $XDG_CONFIG_HOME (~/.config), like OBS on Linux.
 *
 * Original code, GPL-2.0-or-later.
 */
#include "libutils-api.h"
#include "libutils-api-log.h"
#include "libutils-api-mac.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QUrl>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <csignal>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define LINUX_API_MODULE "Process"

namespace {

QString self_exe_path()
{
	return QFileInfo(QStringLiteral("/proc/self/exe")).canonicalFilePath();
}

/* <prefix> = directory above bin/ */
QString install_prefix()
{
	QDir d(QFileInfo(self_exe_path()).absolutePath());
	d.cdUp();
	return d.absolutePath();
}

/* ----- child exit-code registry (filled by reaper threads) ----- */
std::mutex g_exit_mutex;
std::map<pid_t, int> g_exit_codes; /* pid -> decoded exit code, once reaped */

int decode_status(int status)
{
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return WTERMSIG(status);
	return -1;
}

bool exit_code_for(pid_t pid, int &code)
{
	std::lock_guard<std::mutex> lock(g_exit_mutex);
	auto it = g_exit_codes.find(pid);
	if (it == g_exit_codes.end())
		return false;
	code = it->second;
	return true;
}

/* Blocking reaper for one child: records the exit code, prevents zombies. */
void start_reaper(pid_t pid)
{
	std::thread([pid]() {
		int status = 0;
		pid_t r;
		do {
			r = waitpid(pid, &status, 0);
		} while (r == -1 && errno == EINTR);
		if (r == pid) {
			std::lock_guard<std::mutex> lock(g_exit_mutex);
			g_exit_codes[pid] = decode_status(status);
		}
	}).detach();
}

bool pid_alive(pid_t pid)
{
	if (pid <= 0)
		return false;
	if (int c; exit_code_for(pid, c))
		return false;
	if (kill(pid, 0) != 0 && errno != EPERM)
		return false;
	/* A zombie still answers kill(0); treat it as exited. */
	std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
	std::string content((std::istreambuf_iterator<char>(stat)), std::istreambuf_iterator<char>());
	auto rp = content.rfind(')');
	if (rp != std::string::npos && rp + 2 < content.size() && content[rp + 2] == 'Z')
		return false;
	return true;
}

std::string read_os_release_field(const std::string &key)
{
	std::ifstream f("/etc/os-release");
	std::string line;
	while (std::getline(f, line)) {
		if (line.rfind(key + "=", 0) == 0) {
			std::string v = line.substr(key.size() + 1);
			if (v.size() >= 2 && (v.front() == '"' || v.front() == '\''))
				v = v.substr(1, v.size() - 2);
			return v;
		}
	}
	return {};
}

} // namespace

namespace pls_libutil_api_mac {

/* ----- paths ----- */
QString pls_get_app_executable_dir()
{
	return QFileInfo(self_exe_path()).absolutePath();
}
QString pls_get_app_resource_dir()
{
	return install_prefix() + QStringLiteral("/share/hopcast");
}
QString pls_get_app_plugin_dir()
{
	return install_prefix() + QStringLiteral("/lib/hopcast/plugins");
}
QString pls_get_app_macos_dir()
{
	return self_exe_path();
}
QString pls_get_bundle_dir()
{
	return install_prefix();
}
QString pls_get_app_content_dir()
{
	return install_prefix();
}
QString pls_get_dll_dir(const QString &pluginName)
{
	/* Same layout as Windows: data/prism-plugins/<plugin> */
	return pls_get_app_resource_dir() + QStringLiteral("/data/prism-plugins/") + pluginName;
}
QString pls_get_mac_app_data_dir()
{
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}
QString pls_get_app_pn()
{
	return QFileInfo(self_exe_path()).fileName();
}

/* No app-bundle registry on Linux. */
bool pls_is_install_app(const QString &, QString &appPath)
{
	appPath.clear();
	return false;
}
QString pls_get_install_app(const QString &)
{
	return {};
}
QString pls_get_app_version_by_identifier(const char *)
{
	return {};
}
QString pls_get_app_version_by_app_path(const QString &)
{
	return {};
}
bool pls_check_mac_app_is_existed(const wchar_t *)
{
	return false;
}

/* ----- system info ----- */
std::string pls_get_cpu_name()
{
	std::ifstream f("/proc/cpuinfo");
	std::string line;
	while (std::getline(f, line)) {
		if (line.rfind("model name", 0) == 0) {
			auto c = line.find(':');
			if (c != std::string::npos) {
				std::string v = line.substr(c + 1);
				v.erase(0, v.find_first_not_of(" \t"));
				return v;
			}
		}
	}
	return {};
}
pls_mac_ver_t pls_get_mac_systerm_ver()
{
	/* Kernel version: major.minor.patch, build = distro pretty name */
	pls_mac_ver_t v{0, 0, 0, {}};
	utsname u{};
	if (uname(&u) == 0)
		std::sscanf(u.release, "%d.%d.%d", &v.major, &v.minor, &v.patch);
	v.buildNum = read_os_release_field("PRETTY_NAME");
	return v;
}
std::string pls_get_system_version()
{
	std::string name = read_os_release_field("PRETTY_NAME");
	utsname u{};
	if (uname(&u) == 0)
		name += std::string(name.empty() ? "" : " ") + "(Linux " + u.release + ")";
	return name.empty() ? std::string("Linux") : name;
}
std::string pls_get_machine_hardware_name()
{
	utsname u{};
	return uname(&u) == 0 ? std::string(u.machine) : std::string();
}
QString pls_get_system_identifier()
{
	return QLocale::system().name(); /* e.g. en_US (macOS returns the locale id too) */
}
QString pls_get_current_system_language_id()
{
	return QLocale::territoryToCode(QLocale::system().territory()); /* country code, as on macOS */
}
QString pls_get_system_language()
{
	const QStringList langs = QLocale::system().uiLanguages();
	return langs.isEmpty() ? QStringLiteral("en-US") : langs.first();
}
void pls_get_current_datetime(pls_mac_datetime_t &dt)
{
	using namespace std::chrono;
	auto now = system_clock::now();
	std::time_t t = system_clock::to_time_t(now);
	std::tm lt{};
	localtime_r(&t, &lt);
	dt.year = lt.tm_year + 1900;
	dt.month = lt.tm_mon + 1;
	dt.day = lt.tm_mday;
	dt.hour = lt.tm_hour;
	dt.minute = lt.tm_min;
	dt.second = lt.tm_sec;
	dt.milliseconds = static_cast<int>(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
	dt.timezone = static_cast<int>(lt.tm_gmtoff); /* seconds east of UTC, as on macOS */
}

/* ----- strings (wchar_t is UTF-32 on Linux) ----- */
std::wstring pls_utf8_to_unicode(const char *utf8)
{
	return utf8 ? QString::fromUtf8(utf8).toStdWString() : std::wstring();
}
std::string pls_unicode_to_utf8(const wchar_t *unicode)
{
	return unicode ? QString::fromWCharArray(unicode).toStdString() : std::string();
}

/* ----- files ----- */
bool pls_file_is_existed(const QString &filePath)
{
	return QFileInfo::exists(filePath);
}
bool pls_copy_file(const QString &fileName, const QString &newName, bool overwrite, int &errorCode)
{
	errorCode = 0;
	if (overwrite && QFileInfo::exists(newName) && !QFile::remove(newName)) {
		errorCode = EACCES;
		return false;
	}
	QFile src(fileName);
	if (!src.copy(newName)) {
		errorCode = static_cast<int>(src.error());
		return false;
	}
	return true;
}
bool unZip(const QString &dstDirPath, const QString &srcFilePath)
{
	if (!QFileInfo::exists(srcFilePath) || !QFileInfo(dstDirPath).isDir()) {
		PLS_WARN(LINUX_API_MODULE, "unzip failed: missing %s", QFileInfo::exists(srcFilePath) ? "destination folder" : "zip file");
		return false;
	}
	QZipReader reader(srcFilePath);
	if (!reader.isReadable() || reader.status() != QZipReader::NoError) {
		PLS_WARN(LINUX_API_MODULE, "unzip failed: unreadable archive (status %d)", static_cast<int>(reader.status()));
		return false;
	}
	/* Reject entries escaping the destination (zip-slip). */
	const QString root = QDir(dstDirPath).canonicalPath() + QLatin1Char('/');
	for (const auto &fi : reader.fileInfoList()) {
		QString target = QDir::cleanPath(QDir(dstDirPath).absoluteFilePath(fi.filePath));
		if (!(target + QLatin1Char('/')).startsWith(root) && target != QDir(dstDirPath).canonicalPath()) {
			PLS_WARN(LINUX_API_MODULE, "unzip refused: entry outside destination (%s)", "path traversal");
			return false;
		}
	}
	bool ok = reader.extractAll(dstDirPath);
	reader.close();
	return ok;
}
bool zip(const QString &destZipPath, const QString &sourceDirName, const QString &sourceFolderPath)
{
	/* Same contract as the macOS version: cd sourceFolderPath; zip -r dest sourceDirName */
	QDir base(sourceFolderPath);
	QFileInfo src(base.filePath(sourceDirName));
	if (!src.exists()) {
		PLS_WARN(LINUX_API_MODULE, "zip failed: source missing (%s)", "see sourceFolderPath");
		return false;
	}
	QZipWriter writer(destZipPath);
	if (writer.status() != QZipWriter::NoError)
		return false;
	writer.setCompressionPolicy(QZipWriter::AutoCompress);
	auto addFile = [&](const QString &abs) {
		QFile f(abs);
		if (f.open(QIODevice::ReadOnly))
			writer.addFile(base.relativeFilePath(abs), &f);
	};
	if (src.isDir()) {
		writer.addDirectory(sourceDirName);
		QDirIterator it(src.absoluteFilePath(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString p = it.next();
			if (QFileInfo(p).isDir())
				writer.addDirectory(base.relativeFilePath(p));
			else
				addFile(p);
		}
	} else {
		addFile(src.absoluteFilePath());
	}
	writer.close();
	return writer.status() == QZipWriter::NoError;
}
bool pls_removeZipFile(const QString &dzipFile)
{
	return QFile::remove(dzipFile);
}

/* ----- processes ----- */
uint32_t pls_current_process_id()
{
	return static_cast<uint32_t>(getpid());
}
QStringList pls_cmdlines()
{
	QStringList list;
	std::ifstream f("/proc/self/cmdline", std::ios::binary);
	std::string arg;
	while (std::getline(f, arg, '\0'))
		list.append(QString::fromStdString(arg));
	return list;
}
bool pls_is_process_running(unsigned int pid)
{
	return pid_alive(static_cast<pid_t>(pid));
}
bool pls_is_process_running(const char *process_name, int &process_id)
{
	process_id = 0;
	if (!process_name || !*process_name)
		return false;
	DIR *d = opendir("/proc");
	if (!d)
		return false;
	bool found = false;
	while (dirent *e = readdir(d)) {
		char *end = nullptr;
		long pid = std::strtol(e->d_name, &end, 10);
		if (!end || *end != '\0' || pid <= 0)
			continue;
		std::ifstream comm(std::string("/proc/") + e->d_name + "/comm");
		std::string name;
		std::getline(comm, name);
		if (name == process_name && pid_alive(static_cast<pid_t>(pid))) {
			process_id = static_cast<int>(pid);
			found = true;
			break;
		}
	}
	closedir(d);
	return found;
}
MacHandle pls_process_create(const QString &program, const QStringList &arguments, const QString &work_dir)
{
	auto *handle = new MacProcessInfo;
	handle->pid = 0;

	std::string prog = program.toStdString();
	std::vector<std::string> args;
	args.push_back(prog);
	for (const auto &a : arguments)
		args.push_back(a.toStdString());
	std::vector<char *> argv;
	for (auto &a : args)
		argv.push_back(a.data());
	argv.push_back(nullptr);

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	std::string wd = work_dir.toStdString();
	if (!wd.empty())
		posix_spawn_file_actions_addchdir_np(&actions, wd.c_str());

	pid_t pid = 0;
	int rc = posix_spawn(&pid, prog.c_str(), &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	if (rc != 0) {
		PLS_WARN(LINUX_API_MODULE, "posix_spawn failed for %s: %s", prog.c_str(), strerror(rc));
		return handle; /* pid 0 -> caller treats as failure */
	}
	handle->pid = static_cast<uint32_t>(pid);
	start_reaper(pid);
	PLS_INFO(LINUX_API_MODULE, "process created, pid %d, program %s", pid, prog.c_str());
	return handle;
}
MacHandle pls_process_create(uint32_t process_id)
{
	auto *handle = new MacProcessInfo;
	handle->pid = process_id; /* not our child: exit code is unknown (-1) */
	return handle;
}
bool pls_process_destroy(MacHandle handle)
{
	/* macOS semantics: terminate the process. SIGTERM, then SIGKILL after 3 s. */
	if (!handle || !pid_alive(static_cast<pid_t>(handle->pid)))
		return false;
	pid_t pid = static_cast<pid_t>(handle->pid);
	if (kill(pid, SIGTERM) != 0)
		return false;
	for (int i = 0; i < 30 && pid_alive(pid); ++i)
		QThread::msleep(100);
	if (pid_alive(pid))
		kill(pid, SIGKILL);
	return true;
}
bool pls_process_force_terminte(uint32_t process_id, int &exit_code)
{
	pid_t pid = static_cast<pid_t>(process_id);
	if (!pid_alive(pid))
		return false;
	if (kill(pid, SIGKILL) != 0)
		return false;
	for (int i = 0; i < 50 && pid_alive(pid); ++i)
		QThread::msleep(20);
	int code = SIGKILL;
	exit_code_for(pid, code);
	exit_code = code;
	return true;
}
bool pls_process_force_terminte(int &exit_code)
{
	return pls_process_force_terminte(static_cast<uint32_t>(getpid()), exit_code);
}
int pls_process_wait(MacHandle handle, int timeout)
{
	if (!handle)
		return -1;
	pid_t pid = static_cast<pid_t>(handle->pid);
	auto start = std::chrono::steady_clock::now();
	while (pid_alive(pid)) {
		if (timeout == 0)
			return 0;
		if (timeout > 0 &&
		    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() >= timeout)
			return 0;
		QThread::msleep(50);
	}
	return 1;
}
uint32_t pls_process_id(MacHandle handle)
{
	return handle ? handle->pid : 0;
}
bool pls_process_exit_code(MacHandle handle, uint32_t *exit_code)
{
	if (!handle || pid_alive(static_cast<pid_t>(handle->pid)))
		return false;
	int code = handle->exitCode;
	exit_code_for(static_cast<pid_t>(handle->pid), code);
	handle->exitCode = code;
	*exit_code = static_cast<uint32_t>(code);
	return true;
}
/* Exit codes are collected by the reaper thread started in pls_process_create(). */
void addObserverProcessExit(MacHandle) {}
void removeObserverProcessExit(MacHandle) {}

void pls_mac_create_process_with_not_inherit(const QString &program, const QStringList &arguments, void *receiver, PLSMacProcessCallback callback)
{
	MacHandle h = pls_libutil_api_mac::pls_process_create(program, arguments, QString());
	bool ok = h && h->pid != 0;
	int pid = h ? static_cast<int>(h->pid) : 0;
	delete h;
	if (callback)
		callback(receiver, ok, pid);
}
void pls_create_and_observe_process(const QString &program, const QStringList &arguments, void *user_data,
				    void (*callback)(void *user_data, bool created, bool processed))
{
	MacHandle h = pls_libutil_api_mac::pls_process_create(program, arguments, QString());
	bool ok = h && h->pid != 0;
	delete h;
	if (callback)
		callback(user_data, ok, ok);
}
bool pls_restart_mac_app(const QStringList &arguments)
{
	MacHandle h = pls_libutil_api_mac::pls_process_create(self_exe_path(), arguments, QString());
	bool ok = h && h->pid != 0;
	delete h;
	return ok;
}

/* ----- desktop integration ----- */
bool pls_open_url_mac(const QString &url)
{
	return QDesktopServices::openUrl(QUrl(url));
}
bool pls_is_mouse_pressed_by_mac(Qt::MouseButton button)
{
	return qGuiApp && (QGuiApplication::mouseButtons() & button);
}
void bring_mac_window_to_front(WId) {} /* handled by Qt raise()/activateWindow() */
void pls_activate_prism_as_active_app() {}
bool pls_activiate_app()
{
	return false;
}
bool pls_activiate_mac_app_except_self()
{
	return false;
}
bool pls_activiate_app_pid(int)
{
	return false;
}
bool pls_is_app_running(const char *)
{
	return false;
}
bool pls_launch_app(const char *, const char *)
{
	return false;
}
void pls_application_show_dock_icon(bool) {}
bool pls_get_is_app_quitting_by_dock()
{
	return false;
}

/* NAVER request signing is not used by HopCast; same pass-through as macOS. */
QUrl build_mac_hmac_url(const QUrl &url, const QByteArray &)
{
	return url;
}

/* ----- macOS app-bundle self-update: not used on Linux (AppImage) ----- */
bool install_mac_package(const QString &, const QString &, const std::string &, const std::string &, const char *)
{
	return false;
}
QString pls_get_existed_downloaded_mac_app(const QString &, const QString &, bool)
{
	return {};
}
bool pls_remove_all_downloaded_mac_app_small_equal_version(const QString &, const QString &)
{
	return false;
}

/* ----- PRISM Lens: no Linux build ----- */
void pls_set_current_lens(int) {}
bool pls_is_lens_has_run()
{
	return false;
}
bool pls_lens_needs_reboot()
{
	return false;
}

} // namespace pls_libutil_api_mac

/* HopCast: PRISM Lens camera-window queries (implemented in libutils-api-mac.mm on
 * macOS). There is no Lens on Linux. */
LIBUTILSAPI_API LENS_WINDOWS_STATUS pls_is_cam_window_visible()
{
	return LENS_WINDOWS_STATUS::NO_SHOW;
}
LIBUTILSAPI_API bool pls_active_cam_window()
{
	return false;
}
