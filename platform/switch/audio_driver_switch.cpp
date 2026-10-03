/**************************************************************************/
/*  audio_driver_switch.cpp                                               */
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

#include "audio_driver_switch.h"

#include <cstdlib>
#include <cstring>

Error AudioDriverSwitch::init() {
	if (R_FAILED(audoutInitialize())) {
		return ERR_CANT_OPEN;
	}
	initialized = true;
	if (audoutGetSampleRate() != 48000 || audoutGetChannelCount() != 2 || audoutGetPcmFormat() != PcmFormat_Int16) {
		finish();
		return ERR_UNAVAILABLE;
	}
	for (AudioOutBuffer &buffer : buffers) {
		buffer.buffer = aligned_alloc(0x1000, FRAMES * 2 * sizeof(int16_t));
		if (!buffer.buffer) {
			finish();
			return ERR_OUT_OF_MEMORY;
		}
		buffer.buffer_size = FRAMES * 2 * sizeof(int16_t);
		buffer.data_size = buffer.buffer_size;
		memset(buffer.buffer, 0, buffer.buffer_size);
		armDCacheFlush(buffer.buffer, buffer.buffer_size);
		if (R_FAILED(audoutAppendAudioOutBuffer(&buffer))) {
			finish();
			return FAILED;
		}
	}
	if (R_FAILED(audoutStartAudioOut())) {
		finish();
		return FAILED;
	}
	exiting.clear();
	active.clear();
	if (thread.start(thread_main, this) == Thread::UNASSIGNED_ID) {
		finish();
		return FAILED;
	}
	return OK;
}

void AudioDriverSwitch::fill_buffer(AudioOutBuffer *p_buffer) {
	int16_t *samples = static_cast<int16_t *>(p_buffer->buffer);
	if (active.is_set()) {
		lock();
		audio_server_process(FRAMES, mix_buffer);
		unlock();
		for (int i = 0; i < FRAMES * 2; i++) {
			samples[i] = mix_buffer[i] >> 16;
		}
	} else {
		memset(samples, 0, p_buffer->data_size);
	}
	armDCacheFlush(samples, p_buffer->data_size);
}

void AudioDriverSwitch::thread_main(void *p_data) {
	AudioDriverSwitch *driver = static_cast<AudioDriverSwitch *>(p_data);
	while (!driver->exiting.is_set()) {
		AudioOutBuffer *released = nullptr;
		u32 count = 0;
		// Bound the wait so shutdown always joins the worker promptly.
		Result result = audoutWaitPlayFinish(&released, &count, 100000000);
		if (R_FAILED(result)) {
			continue;
		}
		while (released) {
			AudioOutBuffer *next = released->next;
			driver->fill_buffer(released);
			if (R_FAILED(audoutAppendAudioOutBuffer(released))) {
				driver->exiting.set();
				break;
			}
			released = next;
		}
	}
}

void AudioDriverSwitch::finish() {
	exiting.set();
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
	if (initialized) {
		audoutStopAudioOut();
		audoutExit();
		initialized = false;
	}
	for (AudioOutBuffer &buffer : buffers) {
		free(buffer.buffer);
		buffer = {};
	}
}
