/**************************************************************************/
/*  rendering_context_driver_vulkan_switch.cpp                            */
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

#include "rendering_context_driver_vulkan_switch.h"

#include "libnx.h"

#include <cstdlib>

extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance p_instance, const char *p_name);

Error RenderingContextDriverVulkanSwitch::_initialize_loader() {
	// Maxwell support in NVK currently requires this explicit opt-in.
	setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
	volkInitializeCustom(vk_icdGetInstanceProcAddr);
	return OK;
}

const char *RenderingContextDriverVulkanSwitch::_get_platform_surface_extension() const {
	return VK_NN_VI_SURFACE_EXTENSION_NAME;
}

RenderingContextDriver::SurfaceID RenderingContextDriverVulkanSwitch::surface_create(const void *p_platform_data) {
	VkViSurfaceCreateInfoNN info = {};
	info.sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
	info.window = const_cast<void *>(p_platform_data);
	VkSurfaceKHR handle = VK_NULL_HANDLE;
	ERR_FAIL_NULL_V(vkCreateViSurfaceNN, SurfaceID());
	VkResult result = vkCreateViSurfaceNN(instance_get(), &info, get_allocation_callbacks(VK_OBJECT_TYPE_SURFACE_KHR), &handle);
	ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, SurfaceID(), "NXVK could not create a libnx VI surface.");
	Surface *surface = memnew(Surface);
	surface->vk_surface = handle;
	return SurfaceID(surface);
}
