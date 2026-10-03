/**************************************************************************/
/*  export_plugin.cpp                                                     */
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

#include "export_plugin.h"

#include "logo_svg.gen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/os/shared_object.h"
#include "editor/themes/editor_scale.h"

#include "modules/svg/image_loader_svg.h"

EditorExportPlatformSwitch::EditorExportPlatformSwitch() {
#ifdef MODULE_SVG_ENABLED
	Ref<Image> image;
	image.instantiate();
	ImageLoaderSVG::create_image_from_string(image, _switch_logo_svg, EDSCALE, true, HashMap<Color, Color>());
	logo = ImageTexture::create_from_image(image);
#endif
}

void EditorExportPlatformSwitch::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.elf"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.elf"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "devkitpro/path", PROPERTY_HINT_GLOBAL_DIR), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/title"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/author"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/version"), "1.0.0"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.jpg,*.jpeg"), ""));
}

void EditorExportPlatformSwitch::get_platform_features(List<String> *r_features) const {
	r_features->push_back("switch");
	r_features->push_back("mobile");
}

void EditorExportPlatformSwitch::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back("arm64");
	r_features->push_back("s3tc");
	r_features->push_back("bptc");
}

String EditorExportPlatformSwitch::template_path(const Ref<EditorExportPreset> &p_preset, bool p_debug) const {
	String path = p_preset->get(p_debug ? "custom_template/debug" : "custom_template/release");
	return path.is_empty() ? find_export_template(p_debug ? "switch_debug.arm64.elf" : "switch_release.arm64.elf") : path;
}

String EditorExportPlatformSwitch::tool_path(const Ref<EditorExportPreset> &p_preset, const String &p_tool) const {
	String root = p_preset->get("devkitpro/path");
	if (root.is_empty()) {
		root = OS::get_singleton()->get_environment("DEVKITPRO");
	}
	String path = root.path_join("tools/bin").path_join(p_tool);
#ifdef WINDOWS_ENABLED
	path += ".exe";
#endif
	return path;
}

bool EditorExportPlatformSwitch::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	r_missing_templates = !FileAccess::exists(template_path(p_preset, p_debug));
	if (r_missing_templates) {
		r_error += "Missing Switch ARM64 ELF export template. Set custom_template/debug or custom_template/release.\n";
	}
	bool valid = !r_missing_templates;
	if (valid) {
		Ref<FileAccess> file = FileAccess::open(template_path(p_preset, p_debug), FileAccess::READ);
		uint8_t header[20] = {};
		if (file.is_null() || file->get_buffer(header, sizeof(header)) != sizeof(header) ||
				header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F' ||
				header[4] != 2 || header[5] != 1 || header[18] != 0xb7 || header[19] != 0) {
			r_error += "The Switch template must be a little-endian ARM64 ELF, not an NRO or host executable.\n";
			valid = false;
		}
	}
	for (const String &tool : { String("elf2nro"), String("nacptool") }) {
		if (!FileAccess::exists(tool_path(p_preset, tool))) {
			r_error += "Missing devkitPro tool: " + tool_path(p_preset, tool) + "\n";
			valid = false;
		}
	}
	return valid;
}

bool EditorExportPlatformSwitch::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String title = p_preset->get("application/title");
	if (title.is_empty()) {
		title = GLOBAL_GET("application/config/name");
	}
	String author = p_preset->get("application/author");
	String version = p_preset->get("application/version");
	if (title.is_empty() || title.utf8().length() > 511 || author.utf8().length() > 255 || version.is_empty() || version.utf8().length() > 15) {
		r_error += "NACP requires a title (at most 511 UTF-8 bytes), author (at most 255 bytes), and version (1–15 bytes).\n";
		return false;
	}
	String icon = p_preset->get("application/icon");
	if (!icon.is_empty()) {
		Ref<Image> image;
		image.instantiate();
		if ((icon.get_extension().to_lower() != "jpg" && icon.get_extension().to_lower() != "jpeg") || image->load(icon) != OK || image->get_size() != Vector2i(256, 256)) {
			r_error += "The homebrew icon must be a 256×256 JPEG.\n";
			return false;
		}
	}
	return true;
}

HashMap<String, Variant> EditorExportPlatformSwitch::get_custom_project_settings(const Ref<EditorExportPreset> &p_preset) const {
	HashMap<String, Variant> settings;
	settings.insert("rendering/renderer/rendering_method", "mobile");
	settings.insert("rendering/renderer/rendering_method.mobile", "mobile");
	settings.insert("rendering/rendering_device/driver", "vulkan");
	settings.insert("rendering/rendering_device/driver.mobile", "vulkan");
	return settings;
}

Error EditorExportPlatformSwitch::run_tool(const String &p_tool, const List<String> &p_args) {
	String output;
	int exit_code = -1;
	Error err = OS::get_singleton()->execute(p_tool, p_args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "Switch packaging", p_tool + " failed:\n" + output);
		return err != OK ? err : FAILED;
	}
	return OK;
}

Error EditorExportPlatformSwitch::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<DebugFlags> p_flags, bool p_notify) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags, p_notify);
	String validation;
	bool missing = false;
	if (!has_valid_export_configuration(p_preset, validation, missing, p_debug) || !has_valid_project_configuration(p_preset, validation)) {
		add_message(EXPORT_MESSAGE_ERROR, "Switch export", validation);
		return ERR_UNCONFIGURED;
	}
	if (p_flags != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "Switch export", "Remote debugging and one-click deployment are unavailable in this port.");
		return ERR_UNAVAILABLE;
	}
	Error err;
	Ref<DirAccess> temporary = DirAccess::create_temp("godot-switch", false, &err);
	ERR_FAIL_COND_V(temporary.is_null(), err);
	String root = temporary->get_current_dir();
	String romfs = root.path_join("romfs");
	err = DirAccess::make_dir_recursive_absolute(romfs);
	ERR_FAIL_COND_V(err != OK, err);
	Vector<SharedObject> shared_objects;
	err = save_pack(p_preset, p_debug, romfs.path_join("game.pck"), &shared_objects);
	ERR_FAIL_COND_V(err != OK, err);
	if (!shared_objects.is_empty()) {
		add_message(EXPORT_MESSAGE_ERROR, "Switch export", "Dynamic GDExtensions are unavailable. Compile native extensions into the export template.");
		return ERR_UNAVAILABLE;
	}
	String title = p_preset->get("application/title");
	if (title.is_empty()) {
		title = GLOBAL_GET("application/config/name");
	}
	String nacp = root.path_join("game.nacp");
	List<String> nacp_args = { "--create", title, p_preset->get("application/author"), p_preset->get("application/version"), nacp };
	err = run_tool(tool_path(p_preset, "nacptool"), nacp_args);
	ERR_FAIL_COND_V(err != OK, err);
	String nro = root.path_join("game.nro");
	List<String> nro_args = { template_path(p_preset, p_debug), nro, "--nacp=" + nacp, "--romfsdir=" + romfs };
	String icon = p_preset->get("application/icon");
	if (!icon.is_empty()) {
		nro_args.push_back("--icon=" + ProjectSettings::get_singleton()->globalize_path(icon));
	}
	err = run_tool(tool_path(p_preset, "elf2nro"), nro_args);
	ERR_FAIL_COND_V(err != OK, err);
	return DirAccess::copy_absolute(nro, p_path);
}
