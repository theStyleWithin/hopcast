/* HopCast (Linux): CPU / GPU / monitor information for libutils-api.
 *
 * Linux counterpart of libutils-gpu-cpu-monitor-info-mac.mm, using /proc, /sys
 * and QGuiApplication::screens(). Used for system-info logging and the
 * performance display. Original code, GPL-2.0-or-later.
 */
#include <cstdint>

#include "libutils-gpu-cpu-monitor-info.h"

#include <QGuiApplication>
#include <QScreen>

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace {

std::string trim(std::string s)
{
	const char *ws = " \t\r\n";
	s.erase(0, s.find_first_not_of(ws));
	s.erase(s.find_last_not_of(ws) + 1);
	return s;
}

std::string read_first_line(const std::string &path)
{
	std::ifstream f(path);
	std::string line;
	std::getline(f, line);
	return trim(line);
}

/* Value of the first "key : value" line in /proc/cpuinfo. */
std::string cpuinfo_value(const std::string &key)
{
	std::ifstream f("/proc/cpuinfo");
	std::string line;
	while (std::getline(f, line)) {
		auto colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		if (trim(line.substr(0, colon)) == key)
			return trim(line.substr(colon + 1));
	}
	return {};
}

/* MemTotal / MemAvailable from /proc/meminfo, in MB. */
void memory_mb(float &free_mb, float &total_mb)
{
	free_mb = total_mb = 0.0f;
	std::ifstream f("/proc/meminfo");
	std::string key;
	long long kb = 0;
	std::string unit;
	while (f >> key >> kb >> unit) {
		if (key == "MemTotal:")
			total_mb = static_cast<float>(kb) / 1024.0f;
		else if (key == "MemAvailable:")
			free_mb = static_cast<float>(kb) / 1024.0f;
	}
}

unsigned long physical_core_count()
{
	std::ifstream f("/proc/cpuinfo");
	std::string line, phys = "0";
	std::set<std::pair<std::string, std::string>> cores;
	while (std::getline(f, line)) {
		auto colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string k = trim(line.substr(0, colon)), v = trim(line.substr(colon + 1));
		if (k == "physical id")
			phys = v;
		else if (k == "core id")
			cores.emplace(phys, v);
	}
	return cores.empty() ? static_cast<unsigned long>(sysconf(_SC_NPROCESSORS_CONF)) : cores.size();
}

/* /sys/class/drm/cardN (not cardN-HDMI-A-1 connectors). */
std::vector<std::string> drm_cards()
{
	std::vector<std::string> cards;
	if (DIR *d = opendir("/sys/class/drm")) {
		while (dirent *e = readdir(d)) {
			std::string n = e->d_name;
			if (n.rfind("card", 0) == 0 && n.find('-') == std::string::npos)
				cards.push_back("/sys/class/drm/" + n + "/device");
		}
		closedir(d);
	}
	std::sort(cards.begin(), cards.end());
	return cards;
}

unsigned int read_hex(const std::string &path)
{
	std::string s = read_first_line(path);
	return s.empty() ? 0u : static_cast<unsigned int>(std::stoul(s, nullptr, 16));
}

const char *vendor_name(unsigned int id)
{
	switch (id) {
	case 0x10de:
		return "NVIDIA";
	case 0x1002:
		return "AMD";
	case 0x8086:
		return "Intel";
	default:
		return "GPU";
	}
}

} // namespace

std::string pls_get_cpu_info()
{
	std::string name = cpuinfo_value("model name");
	if (name.empty())
		name = "Unknown CPU";
	return name + " " + std::to_string(sysconf(_SC_NPROCESSORS_ONLN)) + "-Core Processor";
}

std::string pls_get_gpu_info()
{
	std::string result;
	for (const auto &dev : drm_cards()) {
		unsigned int vendor = read_hex(dev + "/vendor");
		unsigned int device = read_hex(dev + "/device");
		if (!vendor)
			continue;
		char buf[64];
		std::snprintf(buf, sizeof buf, "%s [%04x:%04x]", vendor_name(vendor), vendor, device);
		if (!result.empty())
			result += ", ";
		result += buf;
	}
	return result;
}

double pls_get_gpu_device_usage()
{
	/* amdgpu exposes a busy percentage; other drivers don't via sysfs. */
	for (const auto &dev : drm_cards()) {
		std::string busy = read_first_line(dev + "/gpu_busy_percent");
		if (!busy.empty())
			return std::stod(busy);
	}
	return 0.0;
}

bool pls_get_basic_graphics_info(pls_gpu_basic_info &gpu_info)
{
	gpu_info.machine_model_name = read_first_line("/sys/class/dmi/id/product_name");
	gpu_info.machine_model_version = read_first_line("/sys/class/dmi/id/product_version");
	gpu_info.gpus.clear();
	for (const auto &dev : drm_cards()) {
		unsigned int vendor = read_hex(dev + "/vendor");
		if (!vendor)
			continue;
		gpu_info.gpus.push_back({vendor, read_hex(dev + "/device")});
	}
	return !gpu_info.gpus.empty();
}

bool pls_get_cpu_info(pls_cpu_info &info)
{
	info.name = cpuinfo_value("model name");
	std::string mhz = cpuinfo_value("cpu MHz");
	info.hz = mhz.empty() ? 0 : static_cast<int64_t>(std::stod(mhz) * 1000000.0);
	long ticks = sysconf(_SC_CLK_TCK);
	info.tick = ticks > 0 ? 1000 / ticks : 0;
	memory_mb(info.free_mem, info.total_mem);
	info.logical_cores = static_cast<unsigned long>(sysconf(_SC_NPROCESSORS_ONLN));
	info.physical_cores = physical_core_count();
	return true;
}

bool pls_get_monitors_info(std::vector<pls_monitor_info> &infos)
{
	infos.clear();
	if (!qGuiApp)
		return false;

	uint32_t id = 0;
	for (QScreen *screen : QGuiApplication::screens()) {
		pls_monitor_info info;
		QRect g = screen->geometry();
		info.monitor_id = id++;
		info.bounds = {g.x(), g.y(), g.width(), g.height()};
		info.dip_to_pixel_scale = static_cast<float>(screen->devicePixelRatio());
		const QString n = screen->name();
		info.is_builtin = n.startsWith("eDP") || n.startsWith("LVDS") || n.startsWith("DSI");
		infos.push_back(info);
	}
	return !infos.empty();
}
