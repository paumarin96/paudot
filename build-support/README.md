# Building this fork with Visual Studio

Prerequisites: Python 3.9 or newer and Visual Studio with **Desktop development
with C++**, the MSVC x64/x86 tools, and a Windows SDK. This checkout was configured
with Python 3.11, Visual Studio Community 2026, and Windows SDK 10.0.26100.0.

From PowerShell in the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-support\windows.ps1
```

This generates `godot.sln` and opens a new Visual Studio instance with the local
SCons environment on PATH. After generation, you can also open `godot.sln`
directly; a local properties file supplies the SCons path to build commands.
Select **editor** and **x64**, then **Build > Build Solution** (Ctrl+Shift+B).
Set the Godot project as the startup project if necessary. F5 starts the editor;
its debug arguments can be set to `--editor --path "D:\path\to\your\game"`.

The build uses MSVC, debug symbols, and debug optimization (`dev_build=yes`).
The executable is `bin\godot.windows.editor.dev.x86_64.exe`. Generated project
files and the Python environment are local artifacts and are not committed.
Only the Windows x64 editor configuration is prepared; other configurations
in Godot's generated solution need their own SCons generation commands.

Other commands:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-support\windows.ps1 Generate
powershell -ExecutionPolicy Bypass -File .\build-support\windows.ps1 Build -Jobs 8
powershell -ExecutionPolicy Bypass -File .\build-support\windows.ps1 Clean
```

Eight parallel jobs is the default. Lower `-Jobs` if memory runs low; generation
stores this setting in the Visual Studio build command. Run Generate again after
adding or removing source files, or changing build options.

For an optimized release editor without debug symbols:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-support\windows.ps1 Build -Configuration Release
```

This uses `dev_build=no production=yes debug_symbols=no optimize=speed lto=full`
with MSVC's `/O2` optimization and whole-program link-time optimization. The
output is `bin\godot.windows.editor.x86_64.release.exe`; the dev executable
remains available. To run the shader GPU checks against this build:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-support\shaders\test.ps1 -Configuration Release
```

Use `windows.ps1 Generate -Configuration Release` to generate the separate
`godot-release.sln` solution, or `windows.ps1 Open -Configuration Release` to
generate and open it. Select `editor | x64` in that solution for release builds.

Vulkan and OpenGL are enabled. Direct3D 12, ANGLE, and AccessKit are disabled to
avoid requiring their separate SDKs. To enable them, install the dependencies
using the corresponding scripts in `misc/scripts` and adjust `$buildArgs` in
`windows.ps1`, then regenerate the solution. .NET support is not enabled.

Godot uses SCons for compilation; the Visual Studio solution invokes it rather
than compiling through ordinary MSBuild C++ projects. See the official guide:
https://docs.godotengine.org/en/latest/engine_details/development/compiling/compiling_for_windows.html
