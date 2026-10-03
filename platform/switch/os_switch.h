/**************************************************************************/
/*  os_switch.h                                                           */
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

#pragma once

#include "audio_driver_switch.h"

#include "core/os/os.h"

class OS_Switch : public OS {
	MainLoop *main_loop = nullptr;
	AudioDriverSwitch audio_driver;
	uint64_t ticks_start = 0;
	String executable_path;
	bool entropy_ready = false;

protected:
	void initialize() override;
	void initialize_joypads() override {}
	void finalize() override;
	void finalize_core() override;
	bool _check_internal_feature_support(const String &p_feature) override;
	void set_main_loop(MainLoop *p_main_loop) override { main_loop = p_main_loop; }
	void delete_main_loop() override;

public:
	MainLoop *get_main_loop() const override { return main_loop; }
	Vector<String> get_video_adapter_driver_info() const override { return {}; }
	String get_stdin_string(int64_t p_buffer_size = 1024) override { return String(); }
	PackedByteArray get_stdin_buffer(int64_t p_buffer_size = 1024) override { return {}; }
	Error get_entropy(uint8_t *r_buffer, int p_bytes) override;
	Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool p_read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override { return ERR_UNAVAILABLE; }
	Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override { return ERR_UNAVAILABLE; }
	Error kill(const ProcessID &p_pid) override { return ERR_UNAVAILABLE; }
	bool is_process_running(const ProcessID &p_pid) const override { return false; }
	int get_process_exit_code(const ProcessID &p_pid) const override { return -1; }
	bool has_environment(const String &p_var) const override;
	String get_environment(const String &p_var) const override;
	void set_environment(const String &p_var, const String &p_value) const override;
	void unset_environment(const String &p_var) const override;
	Error set_cwd(const String &p_cwd) override;
	String get_cwd() const override;
	String get_name() const override { return "Switch"; }
	String get_distribution_name() const override { return "Horizon Homebrew"; }
	String get_version() const override;
	String get_executable_path() const override { return executable_path; }
	void set_cmdline(const char *p_execpath, const List<String> &p_args, const List<String> &p_user_args) override;
	String get_user_data_dir() const override;
	String get_cache_path() const override;
	String get_temp_path() const override { return get_cache_path(); }
	int get_processor_count() const override { return 3; }
	DateTime get_datetime(bool p_utc = false) const override;
	TimeZoneInfo get_time_zone_info() const override;
	double get_unix_time() const override;
	void delay_usec(uint32_t p_usec) const override;
	uint64_t get_ticks_usec() const override;
	void run();
	OS_Switch();
};
