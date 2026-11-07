
# DarkModeSKP

DarkModeSKP is a Windows extension for SketchUp that applies a dark theme to Qt and native Windows interface elements. It combines a Ruby extension with a native C++ helper.

<img width="1906" height="1073" alt="Screenshot 2026-11-07 121147" src="https://github.com/user-attachments/assets/fb002f7c-55cb-4e25-b7b5-09ca20ebbd81" />

## Repository layout

- `DarkModeSKP/` - SketchUp Ruby extension, UI, settings, and assets.
- `DarkModeRuntimeDLL/` - Native Windows helper and build configuration.

Generated DLLs, compiler outputs, RBZ packages, captures, dumps, and local analysis workspaces are intentionally excluded from version control.

## Release Contents

The source distribution includes `DarkModeRuntimeDLL/CMakeLists.txt`, `DarkModeRuntimeDLL/build.ps1`, and `DarkModeRuntimeDLL/src/` so users can build `main.dll` themselves on Windows.

The SketchUp extension package contains only:

- `DarkModeSKP/DarkModeSKP.rb`
- `DarkModeSKP/DarkModeSKP/` with the Ruby sources, `icons/`, and `main.dll`

Do not include `DarkModeRuntimeDLL/out/`, `DarkModeRuntimeDLL/staged/`, or `DarkModeRuntimeDLL/tools/` in the extension package.

## Requirements

- Windows
- SketchUp 2024 or newer
- Visual Studio C++ Build Tools
- CMake 3.20 or newer
- PowerShell

On unsupported hosts, the settings dialog shows the compatibility requirement and disables all toggles. The native helper is not initialized.

## Build the native helper

Build the release DLL:

```powershell
Set-Location .\DarkModeRuntimeDLL
cmake -S . -B out/build -A x64
cmake --build out/build --config Release --target DarkModeRuntimeDLL
```

The `build.ps1` helper runs the same CMake build flow. The build copies the untracked `main.dll` to `DarkModeSKP/DarkModeSKP/main.dll`, which is the location used by the Ruby bridge.

## Create an RBZ Package

Build `main.dll` first. Then create the RBZ from the extension folder so its archive root contains both the loader and its matching support folder:

```powershell
Set-Location .\DarkModeSKP
Compress-Archive -Path .\DarkModeSKP.rb, .\DarkModeSKP -DestinationPath .\DarkModeSKP.rbz -Force
```

The resulting `DarkModeSKP.rbz` must contain `DarkModeSKP.rb` and the `DarkModeSKP/` folder at its root. Do not add the native source tree, CMake build directory, staged artifacts, or local tools.

## Install and Use

1. In supported Windows SketchUp 2024 or newer, open `Extensions` > `Extension Manager`.
2. Select `Install Extension` and choose `DarkModeSKP.rbz`.
3. Open `Extensions` > `Dark Mode SKP Settings`, or click the Dark Mode SKP toolbar button.
4. Choose the desired Dark Mode state, then restart SketchUp when the dialog requests it.

On unsupported platforms or SketchUp versions, the settings dialog displays the compatibility requirement and disables its toggles.

## Development notes

The native integration relies on version-specific SketchUp internals. Validate behavior against each supported SketchUp release before distributing a build.
