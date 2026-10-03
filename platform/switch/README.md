# Nintendo Switch 1 homebrew (experimental)

This platform targets Horizon homebrew on the original Tegra X1 Switch using
devkitA64 and libnx. It uses Godot's existing **Mobile** rendering implementation
through a statically linked [NXVK](https://github.com/PalindromicBreadLoaf/nxvk)
Vulkan ICD and a `VK_NN_vi_surface` backed by libnx's `NWindow`.
It does not use Nintendo's proprietary SDK.

**Status:** both debug and release export templates compile and link with Windows
devkitA64 16.1.0 and the installed NXVK archives. The rebuilt Windows editor has
exported a minimal project to both debug and release NROs with embedded project
data. Execution on Switch hardware has not been verified. Treat this as an
initial port requiring hardware bring-up, not a production export target.

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

### NXVK on Windows with Docker Desktop

Run these commands from the Godot checkout with Docker Desktop's Linux engine
running. Keep LF line endings in the NXVK checkout:

```powershell
git clone --config core.autocrlf=false --depth 1 --branch switch https://github.com/PalindromicBreadLoaf/nxvk.git bin/nxvk
$nxvkSource = (Resolve-Path bin/nxvk).Path
$nxvkPortlibs = 'C:/devkitPro/portlibs/switch'
docker build -t godot-nxvk bin/nxvk/switch/docker
docker run --rm -v "${nxvkSource}:/work" -w /work godot-nxvk make CONTAINER= package
docker run --rm -v "${nxvkSource}:/work" -v "${nxvkPortlibs}:/host-portlibs" -w /work godot-nxvk make CONTAINER= PORTLIB=/host-portlibs install
$pcPath = "$nxvkPortlibs/lib/pkgconfig/nxvk.pc"
$pc = [System.IO.File]::ReadAllText($pcPath).Replace('prefix=/opt/devkitpro/portlibs/switch', "prefix=$nxvkPortlibs")
[System.IO.File]::WriteAllText($pcPath, $pc)
```

The host mount on the install command places the archives, headers, and license
in the Windows SDK. An install into the container's own SDK disappears when
that container exits.

NXVK commit `556cc2ec00d38b955b30e9011cc717b65d3f663c` was built and installed
at `C:/devkitPro/portlibs/switch`. Its smoke app was compiled, linked against
the installed archives, and packaged using Windows devkitA64 16.1.0. The test
NRO is `bin/nxvk/switch/smoke/out/windows/nvk_smoke.nro`. Godot's Switch Vulkan
context also compiled with that toolchain. Both complete Godot templates now
link with the same SDK. These checks do not verify execution on hardware.

## Build

Build both export templates:

```sh
scons platform=switch target=template_debug devkitpro=/opt/devkitpro nxvk_source=/path/to/nxvk
scons platform=switch target=template_release devkitpro=/opt/devkitpro nxvk_source=/path/to/nxvk
```

For Windows Python use native paths, for example:

```powershell
python -m SCons platform=switch target=template_debug devkitpro=C:/devkitPro nxvk_source=bin/nxvk
```

`nxvk_prefix=/path/to/prefix` can select the installed driver archives separately
from the standard devkitPro portlibs. The output files are
`bin/godot.switch.template_debug.arm64.elf` and
`bin/godot.switch.template_release.arm64.elf` for the default build options.
The target requires ARM64, threading, Vulkan/Volk, and the Mobile renderer.
It also requires `disable_path_overrides=no` (selected by default) to load the
embedded `romfs:/game.pck` with `--main-pack`.
Forward+ and the Compatibility renderer are excluded from Switch builds.

Build the editor for the host platform from this source tree. Platform discovery
automatically registers the **Nintendo Switch Homebrew** export preset, even if
the host has not installed the Switch SDK. No exporter registry edits are needed.

This checkout's Windows build helper can build the optimized editor:

```powershell
powershell -ExecutionPolicy Bypass -File build-support/windows.ps1 Build -Configuration Release -Jobs 8
```

The editor output is `bin/godot.windows.editor.x86_64.release.exe`. A sample
export preset selecting the two locally built templates is in
`bin/switch-export-smoke/export_presets.cfg`; its exported test programs are
`switch-smoke-debug.nro` and `switch-smoke-release.nro` in that directory.

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

The template forces `--log-file user://logs/godot.log`, including in release
exports. It is replaced on each launch, and output is flushed after each
message once engine setup completes. Godot errors and script `print()` output
are included. A second logger writes from before engine setup to
`sdmc:/switch/godot/userdata/switch-startup.log` and flushes every message;
the previous launch is kept as `switch-startup.previous.log`. This shared
bootstrap log also contains runtime messages (up to 4095 bytes per message)
and startup/exit checkpoints. If the SD card cannot be written, Horizon debug
output remains available. These logs do not replace Atmosphere's crash reports
and cannot capture messages that the process never emits.

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
- ENet, WebRTC, WebSocket, and UPnP modules are disabled by default on Switch.
  Cryptography remains enabled using Godot's transport callbacks and Horizon's
  monotonic clock.
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
Godot also sends startup messages to Horizon's debug output service, which
emulators can include in their guest logs. Early audio initialization must be
left to `Main::setup2`, after project settings have been created.
The entry point must compare `Main::start()` against `EXIT_SUCCESS` (zero);
treating it as a boolean skips the game loop and exits after the splash screen
on a successful startup.

An Eden v0.2.1 test reached Godot's Vulkan Mobile initialization with NXVK
`556cc2ec00d38b955b30e9011cc717b65d3f663c`, but the emulator then crashed with
a host access violation (`0xc0000005`). The independent NXVK `nvk_smoke.nro`
reproduced the same host fault at `eden.exe+0x78f91d`, without Godot. Switching
Eden's host renderer from Vulkan to OpenGL did not resolve the Godot run.
Scene rendering and presentation have therefore not been validated in Eden;
these builds still require hardware acceptance testing.

This implementation follows Godot's
[custom platform port architecture](https://docs.godotengine.org/en/stable/engine_details/engine_api/custom_platform_ports.html).
