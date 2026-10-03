/**************************************************************************/
/*  os_switch.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "os_switch.h"

#include "display_server_switch.h"
#include "libnx.h"

#include "core/config/project_settings.h"
#include "core/io/ip.h"
#include "core/os/main_loop.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/unix/thread_posix.h"
#include "main/main.h"

#include <unistd.h>

#include <cstdlib>
#include <ctime>

// Networking is intentionally unavailable until a Horizon socket backend is provided.
class IP_Switch : public IP {
	static IP *create_switch() { return memnew(IP_Switch); }

public:
	static void make_default() { _create = create_switch; }
	void _resolve_hostname(List<IPAddress> &r_addresses, const String &p_hostname, Type p_type) const override {}
	void get_local_interfaces(HashMap<String, Interface_Info> *r_interfaces) const override {}
};

OS_Switch::OS_Switch() {
	ticks_start = armGetSystemTick();
	init_thread_posix();
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);
	IP_Switch::make_default();
	entropy_ready = R_SUCCEEDED(csrngInitialize());
	AudioDriverManager::add_driver(&audio_driver);
	DisplayServerSwitch::register_switch();
}

void OS_Switch::initialize() {
	AudioDriverManager::initialize(0);
}

void OS_Switch::finalize() {
	delete_main_loop();
	audio_driver.finish();
}

void OS_Switch::finalize_core() {
	if (entropy_ready) {
		csrngExit();
		entropy_ready = false;
	}
}

void OS_Switch::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
		main_loop = nullptr;
	}
}

bool OS_Switch::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "switch" || p_feature == "mobile" || p_feature == "arm64" || p_feature == "texture_compression_s3tc" || p_feature == "texture_compression_bptc" || p_feature == "texture_compression_astc";
}

Error OS_Switch::get_entropy(uint8_t *r_buffer, int p_bytes) {
	ERR_FAIL_COND_V(p_bytes < 0, ERR_INVALID_PARAMETER);
	return entropy_ready && R_SUCCEEDED(csrngGetRandomBytes(r_buffer, p_bytes)) ? OK : FAILED;
}

bool OS_Switch::has_environment(const String &p_var) const {
	return getenv(p_var.utf8().get_data()) != nullptr;
}
String OS_Switch::get_environment(const String &p_var) const {
	const char *value = getenv(p_var.utf8().get_data());
	return value ? String::utf8(value) : String();
}
void OS_Switch::set_environment(const String &p_var, const String &p_value) const {
	setenv(p_var.utf8().get_data(), p_value.utf8().get_data(), 1);
}
void OS_Switch::unset_environment(const String &p_var) const {
	unsetenv(p_var.utf8().get_data());
}
Error OS_Switch::set_cwd(const String &p_cwd) {
	return chdir(p_cwd.utf8().get_data()) == 0 ? OK : ERR_CANT_OPEN;
}
String OS_Switch::get_cwd() const {
	char buffer[4096];
	return getcwd(buffer, sizeof(buffer)) ? String::utf8(buffer) : String();
}
String OS_Switch::get_version() const {
	u32 version = hosversionGet();
	return vformat("%d.%d.%d", HOSVER_MAJOR(version), HOSVER_MINOR(version), HOSVER_MICRO(version));
}
void OS_Switch::set_cmdline(const char *p_execpath, const List<String> &p_args, const List<String> &p_user_args) {
	executable_path = String::utf8(p_execpath);
	OS::set_cmdline(p_execpath, p_args, p_user_args);
}
String OS_Switch::get_user_data_dir() const {
	String name = GLOBAL_GET("application/config/name");
	return String("sdmc:/switch/godot/userdata").path_join(name.validate_filename());
}
String OS_Switch::get_cache_path() const {
	return get_user_data_dir().path_join("cache");
}

OS::DateTime OS_Switch::get_datetime(bool p_utc) const {
	time_t timestamp = time(nullptr);
	struct tm value = {};
	if (p_utc) {
		gmtime_r(&timestamp, &value);
	} else {
		localtime_r(&timestamp, &value);
	}
	DateTime result = {};
	result.year = 1900 + value.tm_year;
	result.month = Month(value.tm_mon + 1);
	result.day = value.tm_mday;
	result.weekday = Weekday(value.tm_wday);
	result.hour = value.tm_hour;
	result.minute = value.tm_min;
	result.second = value.tm_sec;
	result.dst = value.tm_isdst > 0;
	return result;
}
OS::TimeZoneInfo OS_Switch::get_time_zone_info() const {
	time_t timestamp = time(nullptr);
	struct tm value = {};
	localtime_r(&timestamp, &value);
	char name[64] = {};
	char offset[16] = {};
	strftime(name, sizeof(name), "%Z", &value);
	strftime(offset, sizeof(offset), "%z", &value);
	int bias = atoi(offset);
	return { (bias / 100) * 60 + bias % 100, String::utf8(name) };
}
void OS_Switch::delay_usec(uint32_t p_usec) const {
	svcSleepThread(uint64_t(p_usec) * 1000);
}
double OS_Switch::get_unix_time() const {
	return time(nullptr);
}
uint64_t OS_Switch::get_ticks_usec() const {
	return armTicksToNs(armGetSystemTick() - ticks_start) / 1000;
}

void OS_Switch::run() {
	ERR_FAIL_NULL(main_loop);
	main_loop->initialize();
	while (appletMainLoop()) {
		DisplayServer::get_singleton()->process_events();
		if (Main::iteration()) {
			break;
		}
	}
	main_loop->finalize();
}
