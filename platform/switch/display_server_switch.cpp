/**************************************************************************/
/*  display_server_switch.cpp                                             */
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

#include "display_server_switch.h"

#include "core/input/input.h"
#include "core/input/input_event.h"
#include "servers/display/native_menu.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"

DisplayServer *DisplayServerSwitch::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	r_error = ERR_UNAVAILABLE;
	ERR_FAIL_COND_V_MSG(p_rendering_driver != "vulkan", nullptr, "Switch requires Vulkan through NXVK.");
	return memnew(DisplayServerSwitch(p_vsync_mode, r_error));
}

void DisplayServerSwitch::register_switch() {
	register_create_function("switch", create_func, get_rendering_drivers_func);
}

DisplayServerSwitch::DisplayServerSwitch(DisplayServerEnums::VSyncMode p_vsync_mode, Error &r_error) {
	r_error = ERR_UNAVAILABLE;
	native_menu = memnew(NativeMenu);
	context = memnew(RenderingContextDriverVulkanSwitch);
	NWindow *window = nwindowGetDefault();
	ERR_FAIL_COND_MSG(R_FAILED(nwindowSetDimensions(window, size.x, size.y)), "Failed to set Switch display dimensions.");
	r_error = context->initialize();
	ERR_FAIL_COND_MSG(r_error != OK, "Failed to initialize NXVK.");
	r_error = context->window_create(DisplayServerEnums::MAIN_WINDOW_ID, window);
	ERR_FAIL_COND_MSG(r_error != OK, "Failed to create the Switch Vulkan window.");
	context->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID, size.x, size.y);
	context->window_set_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID, p_vsync_mode);
	device = memnew(RenderingDevice);
	r_error = device->initialize(context, DisplayServerEnums::MAIN_WINDOW_ID);
	ERR_FAIL_COND_MSG(r_error != OK, "NXVK could not initialize Godot's rendering device.");
	r_error = device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
	ERR_FAIL_COND_MSG(r_error != OK, "Failed to create the Switch swapchain.");
	RendererCompositorRD::make_current();
	padConfigureInput(8, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pads[0]);
	for (int i = 1; i < 8; i++) {
		padInitializeWithMask(&pads[i], BIT(i));
	}
	hidInitializeTouchScreen();
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);
	r_error = OK;
}

DisplayServerSwitch::~DisplayServerSwitch() {
	if (device) {
		memdelete(device);
	}
	if (context) {
		memdelete(context);
	}
	memdelete(native_menu);
}

void DisplayServerSwitch::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	static_cast<DisplayServerSwitch *>(get_singleton())->_dispatch_input_event(p_event);
}

void DisplayServerSwitch::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	if (input_event_callback.is_valid()) {
		input_event_callback.call(p_event);
	}
}

void DisplayServerSwitch::poll_input() {
	static const struct {
		u64 mask;
		JoyButton button;
	} buttons[] = {
		{ HidNpadButton_B, JoyButton::A },
		{ HidNpadButton_A, JoyButton::B },
		{ HidNpadButton_Y, JoyButton::X },
		{ HidNpadButton_X, JoyButton::Y },
		{ HidNpadButton_L, JoyButton::LEFT_SHOULDER },
		{ HidNpadButton_R, JoyButton::RIGHT_SHOULDER },
		{ HidNpadButton_Minus, JoyButton::BACK },
		{ HidNpadButton_Plus, JoyButton::START },
		{ HidNpadButton_StickL, JoyButton::LEFT_STICK },
		{ HidNpadButton_StickR, JoyButton::RIGHT_STICK },
		{ HidNpadButton_Up, JoyButton::DPAD_UP },
		{ HidNpadButton_Down, JoyButton::DPAD_DOWN },
		{ HidNpadButton_Left, JoyButton::DPAD_LEFT },
		{ HidNpadButton_Right, JoyButton::DPAD_RIGHT },
	};
	Input *input = Input::get_singleton();
	for (int i = 0; i < 8; i++) {
		padUpdate(&pads[i]);
		bool is_connected = padIsConnected(&pads[i]);
		if (connected[i] != is_connected) {
			connected[i] = is_connected;
			input->joy_connection_changed(i, is_connected, "Nintendo Switch Controller");
		}
		if (!is_connected) {
			continue;
		}
		u64 changed = padGetButtonsDown(&pads[i]) | padGetButtonsUp(&pads[i]);
		u64 held = padGetButtons(&pads[i]);
		for (const auto &button : buttons) {
			if (changed & button.mask) {
				input->joy_button(i, button.button, held & button.mask);
			}
		}
		HidAnalogStickState left = padGetStickPos(&pads[i], 0);
		HidAnalogStickState right = padGetStickPos(&pads[i], 1);
		input->joy_axis(i, JoyAxis::LEFT_X, CLAMP(left.x / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(i, JoyAxis::LEFT_Y, CLAMP(-left.y / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(i, JoyAxis::RIGHT_X, CLAMP(right.x / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(i, JoyAxis::RIGHT_Y, CLAMP(-right.y / 32767.0f, -1.0f, 1.0f));
		input->joy_axis(i, JoyAxis::TRIGGER_LEFT, (held & HidNpadButton_ZL) ? 1.0f : 0.0f);
		input->joy_axis(i, JoyAxis::TRIGGER_RIGHT, (held & HidNpadButton_ZR) ? 1.0f : 0.0f);
	}
	HidTouchScreenState state = {};
	if (hidGetTouchScreenStates(&state, 1) == 0) {
		return;
	}
	HashMap<int, Vector2> current;
	for (int i = 0; i < state.count; i++) {
		const HidTouchState &touch = state.touches[i];
		int id = touch.finger_id;
		Vector2 position(touch.x, touch.y);
		current.insert(id, position);
		if (!touches.has(id)) {
			Ref<InputEventScreenTouch> event;
			event.instantiate();
			event->set_index(id);
			event->set_position(position);
			event->set_pressed(true);
			input->parse_input_event(event);
		} else if (touches[id] != position) {
			Ref<InputEventScreenDrag> event;
			event.instantiate();
			event->set_index(id);
			event->set_position(position);
			event->set_relative(position - touches[id]);
			event->set_relative_screen_position(position - touches[id]);
			input->parse_input_event(event);
		}
	}
	for (const KeyValue<int, Vector2> &touch : touches) {
		if (!current.has(touch.key)) {
			Ref<InputEventScreenTouch> event;
			event.instantiate();
			event->set_index(touch.key);
			event->set_position(touch.value);
			event->set_pressed(false);
			input->parse_input_event(event);
		}
	}
	touches = current;
}

void DisplayServerSwitch::process_events() {
	poll_input();
	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerSwitch::window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND(p_window != DisplayServerEnums::MAIN_WINDOW_ID);
	context->window_set_vsync_mode(p_window, p_vsync_mode);
}

DisplayServerEnums::VSyncMode DisplayServerSwitch::window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V(p_window != DisplayServerEnums::MAIN_WINDOW_ID, DisplayServerEnums::VSYNC_ENABLED);
	return context->window_get_vsync_mode(p_window);
}
