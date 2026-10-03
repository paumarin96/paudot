# Nintendo Switch 1 homebrew (experimental)

This platform targets Horizon homebrew on the original Tegra X1 Switch using
devkitA64 and libnx. It uses Godot's existing **Mobile** rendering implementation
through a statically linked [NXVK](https://github.com/PalindromicBreadLoaf/nxvk)
Vulkan ICD and a `VK_NN_vi_surface` backed by libnx's `NWindow`.
It does not use Nintendo's proprietary SDK.

**Status:** the editor exporter compiles on Windows, and the platform translation
units have passed a Clang AArch64 syntax check using public libnx headers and
the installed devkitARM newlib/libstdc++ headers as a fallback. This checks API
usage, not devkitA64 ABI compatibility. A complete
devkitA64/NXVK link and execution on hardware have not been verified. Treat this
as an initial port requiring hardware bring-up, not a production export target.

## Dependencies

Follow [devkitPro Getting Started](https://devkitpro.org/wiki/Getting_Started).
Install the `switch-dev` group, `switch-zlib`, and `switch-libexpat` using the
devkitPro package manager. Install Python and SCons 4.4 or newer on the build host.

Build the **switch branch** of NXVK following its
[build instructions](https://github.com/PalindromicBreadLoaf/nxvk/blob/switch/switch/README.md).
Run its Vulkan-only package/install targets to obtain `libnvk.a` and
`libnvk_support.a` in `$DEVKITPRO/portlibs/switch/lib`. Keep its source checkout:
this port also compiles `switch/smoke/nvk_compat.c` for the POSIX functions that
NXVK needs and newlib lacks. NXVK's compatibility headers are scoped to that
translation unit.

The Switch supports Vulkan in licensed software, but **devkitPro/libnx alone
does not provide that driver**. NXVK is an additional experimental dependency.
Its upstream documentation currently requires the
`NVK_I_WANT_A_BROKEN_VULKAN_DRIVER=1` opt-in for Maxwell; this port sets it before
initializing Vulkan. Keep the driver and compatibility source from the same
checkout. Review that dependency's license when distributing linked binaries.

## Build

Build both export templates:

```sh
scons platform=switch target=template_debug devkitpro=/opt/devkitpro nxvk_source=/path/to/nxvk
scons platform=switch target=template_release devkitpro=/opt/devkitpro nxvk_source=/path/to/nxvk
```

For Windows Python use native paths, for example:

```powershell
python -m SCons platform=switch target=template_debug devkitpro=C:/devkitPro nxvk_source=D:/src/nxvk
```

`nxvk_prefix=/path/to/prefix` can select the installed driver archives separately
from the standard devkitPro portlibs. The output files are
`bin/godot.switch.template_debug.arm64.elf` and
`bin/godot.switch.template_release.arm64.elf` for the default build options.
The target requires ARM64, threading, Vulkan/Volk, and the Mobile renderer.
Forward+ and the Compatibility renderer are excluded from Switch builds.

Build the editor for the host platform from this source tree. Platform discovery
automatically registers the **Nintendo Switch Homebrew** export preset, even if
the host has not installed the Switch SDK. No exporter registry edits are needed.

## Export and run

1. Add a Nintendo Switch Homebrew export preset in the rebuilt editor.
2. Set the custom debug/release template paths to the corresponding ELF files.
   Alternatively install them as `switch_debug.arm64.elf` and
   `switch_release.arm64.elf` in the current Godot export template directory.
3. Set `devkitpro/path` to a native devkitPro path containing `tools/bin/elf2nro`
   and `tools/bin/nacptool` (the `.exe` tools on Windows). The `DEVKITPRO`
   environment variable is used if the preset field is empty.
4. Enter the title, author, and version. A blank title uses the project name.
   Optionally provide a 256×256 JPEG icon.
5. Export to an `.nro`. The exporter saves `game.pck` into a temporary RomFS,
   creates NACP metadata, and runs `elf2nro`. The completed NRO contains the
   project; no sidecar PCK is required. Temporary packaging files are cleaned up.
6. Copy the NRO to `sdmc:/switch/<game>/` and launch through hbmenu using **full
   application/title takeover**. Album/applet mode has insufficient memory.

The template mounts RomFS and starts `romfs:/game.pck` with `--rendering-method
mobile --rendering-driver vulkan`. Exported settings also select Mobile/Vulkan.
Project save data and logs use
`sdmc:/switch/godot/userdata/<sanitized project name>/`.

## Implemented scope and limitations

- One fixed 1280×720 fullscreen window, including docked operation (VI scales
  the output). Godot's RenderingDevice manages the Vulkan swapchain.
- Eight controllers through libnx pads, including handheld/Joy-Con input,
  hotplug detection, sticks, digital triggers, and standard Godot button positions.
- Multitouch press, drag, and release events.
- Stereo 48 kHz audio through `audout`, using two aligned PCM buffers and a
  mixing worker with a bounded shutdown wait.
- SD card/packaged resource access, per-project user data, timing, environment
  variables, and CSRNG entropy.
- Networking, remote debugging, one-click deployment, native dynamic
  GDExtensions, C#, keyboard/IME, rumble, and additional windows are unavailable.
  Native extensions must be compiled into the template.
- There is no claim of Vulkan conformance or measured performance for this
  combination. Graphics features and memory use must be tested on hardware.

## Hardware acceptance checks

Build configuration tests can be run with the host Python environment containing
SCons:

```sh
python -m unittest discover -s platform/switch/tests -v
```

These cover invalid build selections, missing driver/compatibility dependencies,
and the actual SCons expansion of the static Vulkan link group.

Before treating a template as usable, build and link both configurations with
the real toolchain, export a minimal 2D project and a Mobile 3D project, and run
them on a Switch 1. Check rendering and presentation, shader compilation,
controller disconnect/reconnect, two simultaneous touches, sustained audio,
save/reload of `user://` data, suspend/resume, clean exit, and a subsequent relaunch.
Repeat in handheld and docked modes. Inspect project logs for driver errors.

This implementation follows Godot's
[custom platform port architecture](https://docs.godotengine.org/en/stable/engine_details/engine_api/custom_platform_ports.html).
