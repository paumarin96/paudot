"""Build configuration tests; these do not substitute for a target build."""

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from SCons.Script import Environment

spec = importlib.util.spec_from_file_location("switch_detect", Path(__file__).parents[1] / "detect.py")
detect = importlib.util.module_from_spec(spec)
spec.loader.exec_module(detect)


class SwitchConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.sdk = self.root / "SDK with spaces"
        self.source = self.root / "NXVK source"
        for name in [
            "devkitA64/bin/aarch64-none-elf-g++",
            "libnx/switch.specs",
            "portlibs/switch/lib/libnvk.a",
            "portlibs/switch/lib/libnvk_support.a",
            "portlibs/switch/lib/libz.a",
            "portlibs/switch/lib/libexpat.a",
        ]:
            file = self.sdk / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.touch()
        compat = self.source / "switch/smoke/nvk_compat.c"
        compat.parent.mkdir(parents=True)
        compat.touch()
        self.env = Environment(tools=detect.get_tools(None))
        self.env.Replace(**detect.get_flags(), threads=True, devkitpro=str(self.sdk), nxvk_prefix="", nxvk_source=str(self.source))
        self.addCleanup(patch.stopall)
        patch.object(type(self.env), "use_windows_spawn_fix", create=True).start()
        self.error = patch.object(detect, "print_error").start()

    def test_link_command_preserves_engine_libraries_and_static_icd_order(self):
        detect.configure(self.env)
        self.env.Prepend(LIBS=["godot_core", "godot_drivers"])
        flags = self.env.subst("$_LIBFLAGS")
        self.assertEqual(
            flags.split(),
            ["-lgodot_core", "-lgodot_drivers", "-Wl,--whole-archive", "-lnvk", "-Wl,--no-whole-archive",
             "-Wl,--start-group", "-lnvk_support", "-lz", "-lexpat", "-lnx", "-lpthread", "-lm", "-Wl,--end-group"],
        )
        self.assertEqual(self.env["PROGSUFFIX"], ".elf")
        self.assertIn("VULKAN_ENABLED", self.env["CPPDEFINES"])
        self.assertIn("VOLK_NO_DYNAMIC_LOADER", self.env["CPPDEFINES"])
        self.assertIn("-pthread", self.env["CCFLAGS"])
        self.assertEqual(self.env["ENV"]["DEVKITPRO"], self.sdk.as_posix())

    def test_missing_driver_fails_before_build(self):
        (self.sdk / "portlibs/switch/lib/libnvk.a").unlink()
        with self.assertRaises(SystemExit):
            detect.configure(self.env)
        self.assertIn("Missing NXVK", self.error.call_args.args[0])

    def test_missing_compat_source_fails_before_link(self):
        self.env["nxvk_source"] = ""
        with self.assertRaises(SystemExit):
            detect.configure(self.env)
        self.assertIn("nxvk_source", self.error.call_args.args[0])

    def test_invalid_target_configuration_is_rejected(self):
        for key, value in [("target", "editor"), ("arch", "x86_64"), ("vulkan", False),
                           ("forward_plus_renderer", True), ("threads", False), ("disable_path_overrides", True)]:
            with self.subTest(key=key):
                old_value = self.env[key]
                self.env[key] = value
                with self.assertRaises(SystemExit):
                    detect.configure(self.env)
                self.env[key] = old_value


if __name__ == "__main__":
    unittest.main()
