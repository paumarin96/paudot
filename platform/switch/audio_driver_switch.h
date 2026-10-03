/**************************************************************************/
/*  audio_driver_switch.h                                                 */
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

#include "libnx.h"

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "servers/audio/audio_driver.h"

class AudioDriverSwitch : public AudioDriver {
	static constexpr int FRAMES = 1024;
	AudioOutBuffer buffers[2] = {};
	int32_t mix_buffer[FRAMES * 2] = {};
	Mutex mutex;
	Thread thread;
	SafeFlag exiting;
	SafeFlag active;
	bool initialized = false;
	static void thread_main(void *p_data);
	void fill_buffer(AudioOutBuffer *p_buffer);

public:
	const char *get_name() const override { return "libnx"; }
	Error init() override;
	void start() override { active.set(); }
	int get_mix_rate() const override { return 48000; }
	SpeakerMode get_speaker_mode() const override { return SPEAKER_MODE_STEREO; }
	float get_latency() override { return float(FRAMES * 2) / 48000; }
	void lock() override { mutex.lock(); }
	void unlock() override { mutex.unlock(); }
	void finish() override;
	~AudioDriverSwitch() override { finish(); }
};
