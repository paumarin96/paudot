/**************************************************************************/
/*  godot_switch.cpp                                                      */
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

#include "libnx.h"
#include "os_switch.h"

#include "main/main.h"
#include "core/io/logger.h"
#include "core/string/print_string.h"

#include <cstdio>
#include <cstdlib>

extern "C" {
// Let libnx use the loader's applet type, so restricted launches can be rejected.
// A zero heap limit uses all memory available to the application.
size_t __nx_heap_size = 0;
}

int main(int argc, char **argv) {
	if (appletGetAppletType() != AppletType_Application && appletGetAppletType() != AppletType_SystemApplication) {
		consoleInit(nullptr);
		printf("Godot requires full application memory. Launch hbmenu using title takeover.\nPress + to exit.\n");
		padConfigureInput(1, HidNpadStyleSet_NpadStandard);
		PadState pad;
		padInitializeDefault(&pad);
		while (appletMainLoop()) {
			padUpdate(&pad);
			if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
				break;
			}
			consoleUpdate(nullptr);
		}
		consoleExit(nullptr);
		return 1;
	}
	Result result = romfsInit();
	if (R_FAILED(result)) {
		fprintf(stderr, "Cannot mount project RomFS: 0x%x\n", result);
		return 1;
	}
	int exit_code = 1;
	{
		OS_Switch os;
		// Load the PCK explicitly; the NRO's executable path is on the SD card.
		char pack_flag[] = "--main-pack";
		char pack_path[] = "romfs:/game.pck";
		char renderer_flag[] = "--rendering-method";
		char renderer[] = "mobile";
		char driver_flag[] = "--rendering-driver";
		char driver[] = "vulkan";
		char log_flag[] = "--log-file";
		char log_path[] = "user://logs/godot.log";
		char *args[] = { pack_flag, pack_path, renderer_flag, renderer, driver_flag, driver, log_flag, log_path };
		print_line("Switch startup: beginning engine setup.");
		Error err = Main::setup(argc > 0 ? argv[0] : "godot.nro", 8, args);
		Logger::set_flush_stdout_on_print(true);
		print_line(vformat("Switch startup: engine setup returned %d.", int(err)));
		if (err == OK) {
			print_line("Switch startup: loading the main scene.");
			int start_result = Main::start();
			print_line(vformat("Switch startup: main scene startup returned %d.", start_result));
			if (start_result == EXIT_SUCCESS) {
				os.run();
			} else {
				os.set_exit_code(EXIT_FAILURE);
			}
			exit_code = os.get_exit_code();
			Main::cleanup();
			print_line("Switch runtime: engine cleanup completed.");
		}
	}
	romfsExit();
	return exit_code;
}
