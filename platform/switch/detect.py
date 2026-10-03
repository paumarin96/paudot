import os
import sys
from pathlib import Path

from methods import print_error


def is_active():
    return True


def get_name():
    return "Nintendo Switch Homebrew"


def can_build():
    # Keep the target discoverable even before dependencies are installed.
    return sys.platform in ("win32", "linux", "darwin")


def get_opts():
    return [
        ("devkitpro", "Native path to the devkitPro installation", os.environ.get("DEVKITPRO", "")),
        ("nxvk_prefix", "Installed NXVK portlib prefix (defaults to devkitPro portlibs/switch)", ""),
        ("nxvk_source", "NXVK source checkout, for its newlib compatibility source", os.environ.get("NXVK_SOURCE", "")),
    ]


def get_tools(env):
    # Do not select the host MSVC toolchain when cross-compiling on Windows.
    return ["gcc", "g++", "gnulink", "ar", "gas"]


def get_flags():
    return {
        "arch": "arm64",
        "target": "template_debug",
        "disable_path_overrides": False,
        "vulkan": True,
        "use_volk": True,
        "rendering_device": True,
        "opengl3": False,
        "forward_plus_renderer": False,
        "forward_mobile_renderer": True,
        "builtin_pcre2_with_jit": False,
        "module_raycast_enabled": False,
        "module_mono_enabled": False,
        "module_upnp_enabled": False,
        "module_enet_enabled": False,
        "module_webrtc_enabled": False,
        "module_websocket_enabled": False,
        "accesskit": False,
        "sdl": False,
    }


def configure(env):
    def fail(message):
        print_error(message)
        raise SystemExit(255)

    if env["target"] == "editor":
        fail("Switch builds export templates only. Build the editor for your host platform.")
    if env["arch"] != "arm64":
        fail("Switch 1 requires arch=arm64.")
    if not all(env[k] for k in ("vulkan", "use_volk", "rendering_device", "forward_mobile_renderer")):
        fail("Switch requires vulkan=yes use_volk=yes rendering_device=yes forward_mobile_renderer=yes.")
    if env["opengl3"] or env["forward_plus_renderer"]:
        fail("This port supports the Vulkan Mobile renderer only.")
    if not env["threads"]:
        fail("Switch requires threads=yes for rendering and audio.")
    if env["disable_path_overrides"]:
        fail("Switch requires disable_path_overrides=no to load romfs:/game.pck through --main-pack.")
    sdk = Path(env["devkitpro"]).expanduser().resolve()
    if not env["devkitpro"] or not (sdk / "devkitA64").is_dir():
        fail("devkitA64 is missing. Install switch-dev and pass devkitpro=<native path>; MSYS /opt paths do not work in Windows Python.")
    compiler = sdk / "devkitA64/bin/aarch64-none-elf-g++"
    if not compiler.is_file() and not compiler.with_suffix(".exe").is_file():
        fail("Missing devkitA64 compiler: " + str(compiler))
    if not (sdk / "libnx/switch.specs").is_file():
        fail("libnx is missing. Install the devkitPro switch-dev package group.")
    portlibs = sdk / "portlibs/switch"
    nxvk = Path(env["nxvk_prefix"]).expanduser().resolve() if env["nxvk_prefix"] else portlibs
    for file in ("lib/libnvk.a", "lib/libnvk_support.a"):
        if not (nxvk / file).is_file():
            fail("Missing NXVK " + str(nxvk / file) + ". Build and install https://github.com/PalindromicBreadLoaf/nxvk (switch branch).")
    for library in ("z", "expat"):
        if not (portlibs / ("lib/lib" + library + ".a")).is_file():
            fail("Missing switch-" + ("zlib" if library == "z" else "libexpat") + " portlib.")
    nxvk_source = Path(env["nxvk_source"]).expanduser()
    if not env["nxvk_source"] or not (nxvk_source / "switch/smoke/nvk_compat.c").is_file():
        fail("Pass nxvk_source=<NXVK source checkout>. The driver needs switch/smoke/nvk_compat.c to fill newlib gaps.")
    env["switch_nxvk_compat"] = str((nxvk_source / "switch/smoke/nvk_compat.c").resolve())
    env["switch_nxvk_compat_headers"] = str((nxvk_source / "switch/docker/cross-include").resolve())
    env.PrependENVPath("PATH", str(sdk / "devkitA64/bin"))
    env.PrependENVPath("PATH", str(sdk / "tools/bin"))
    # switch.specs resolves switch.ld through getenv(DEVKITPRO), not its own path.
    env["ENV"]["DEVKITPRO"] = sdk.as_posix()
    env["ENV"]["DEVKITA64"] = (sdk / "devkitA64").as_posix()
    env["CC"] = str(sdk / "devkitA64/bin/aarch64-none-elf-gcc")
    env["CXX"] = str(compiler)
    env["AR"] = str(sdk / "devkitA64/bin/aarch64-none-elf-ar")
    env["RANLIB"] = str(sdk / "devkitA64/bin/aarch64-none-elf-ranlib")
    env["LINK"] = env["CXX"]
    env["PROGSUFFIX"] = ".elf"
    env["SHLIBSUFFIX"] = ".so"
    env.Append(CPPPATH=["#platform/switch", str(sdk / "libnx/include"), str(portlibs / "include")])
    env.Append(LIBPATH=[str(nxvk / "lib"), str(portlibs / "lib"), str(sdk / "libnx/lib")])
    env.Append(CPPDEFINES=["SWITCH_ENABLED", "UNIX_ENABLED", "UNIX_SOCKET_UNAVAILABLE", "PTHREAD_NO_RENAME", "__SWITCH__", "VULKAN_ENABLED", "VK_USE_PLATFORM_VI_NN", "VOLK_NO_DYNAMIC_LOADER"])
    arch_flags = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE", "-pthread"]
    env.Append(CCFLAGS=arch_flags + ["-ffunction-sections", "-fdata-sections"])
    env.Append(LINKFLAGS=arch_flags + ["-specs=" + str(sdk / "libnx/switch.specs"), "-Wl,--gc-sections", "-Wl,-u,vk_icdGetInstanceProcAddr"])
    # Preserve Godot's libraries, then append the static ICD link group verbatim.
    # Putting -Wl flags in LIBS causes SCons to prefix them with -l.
    env["_SWITCH_ENGINE_LIBFLAGS"] = env["_LIBFLAGS"]
    env["_LIBFLAGS"] = (
        "$_SWITCH_ENGINE_LIBFLAGS -Wl,--whole-archive -lnvk -Wl,--no-whole-archive "
        "-Wl,--start-group -lnvk_support -lz -lexpat -lnx -lpthread -lm -Wl,--end-group"
    )
    env["switch_nxvk_archives"] = [str(nxvk / "lib/libnvk.a"), str(nxvk / "lib/libnvk_support.a")]
    if os.name == "nt":
        env.use_windows_spawn_fix()
