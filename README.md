# DarkModeSKP

> **Disclaimer:** DarkModeSKP is an independent project. It is not affiliated with, endorsed by, or sponsored by Trimble Inc. or SketchUp.

DarkModeSKP is an experimental SketchUp extension that attempts to apply a dark theme to Qt interface elements.

<img width="1906" height="1073" alt="image" src="https://github.com/user-attachments/assets/8591d74e-9288-4278-b061-46d6042a083f" />

*The navigation cube in the top-right corner is the [Viewport Advanced SKP](https://extensions.sketchup.com/extension/cd54da6d-9823-4ac7-9135-974f8988d6a9) extension.*

## Repository layout

- `DarkModeSKP/` - SketchUp Ruby extension, UI, settings, and assets.
- `DarkModeRuntimeDLL/` - Native Windows helper and build configuration.

Generated build and package outputs are intentionally excluded from version control.

## Requirements

- Windows
- SketchUp **2024-2026** for installation and use
- Visual Studio C++ Build Tools with the Desktop development with C++ workload
- CMake 3.20 or newer
- PowerShell

## Recommended: build from source

Building from source is the recommended path. It creates `main.dll` from the current source, packages it with the extension, and avoids relying on a prebuilt binary.

1. Open PowerShell at the repository root, where `DarkModeRuntimeDLL/` and `DarkModeSKP/` are sibling folders.
2. Build the release DLL:

   ```powershell
   .\DarkModeRuntimeDLL\build.ps1 -Configuration Release
   ```

   The build creates its output under `DarkModeRuntimeDLL/out/build/` and automatically copies the DLL needed by the extension to `DarkModeSKP/DarkModeSKP/main.dll`. Do not move the DLL after the build.

3. Confirm the extension DLL is in place:

   ```powershell
   Test-Path .\DarkModeSKP\DarkModeSKP\main.dll
   ```

   This command must return `True` before creating the package.

4. Create the extension archive from the extension folder:

   ```powershell
   Set-Location .\DarkModeSKP
   Compress-Archive -Path .\DarkModeSKP.rb, .\DarkModeSKP -DestinationPath ..\DarkModeSKP.rbz -Force
   ```

   The completed package is `DarkModeSKP.rbz` in the repository root. Its archive root must contain `DarkModeSKP.rb` and the `DarkModeSKP/` support folder, including `DarkModeSKP/main.dll`. After you've finished building the `.rbz` package, you can continue with [Installing the extension](#installing-the-extension).

## Download a prebuilt release

Use this alternative when you do not want to build from source. Download the latest `.rbz` package from [GitHub Releases](https://github.com/gordanog/Dark-Mode-SKP-Public/releases), then continue with [Installing the extension](#installing-the-extension).

## Installing the extension

1. In SketchUp, open `Extensions` > `Extension Manager`.
2. Select `Install Extension` and choose the downloaded or manually created `DarkModeSKP.rbz` package.
3. Open `Extensions` > `Dark Mode SKP Settings`, choose the desired state, and restart SketchUp when prompted.

## Use with caution

DarkModeSKP is experimental and modifies SketchUp interface behavior. It may cause visual issues or instability

If SketchUp fails to start or becomes stuck after installation, close it completely. Open `%AppData%\SketchUp\SketchUp 20XX\SketchUp\Plugins` in File Explorer, replacing `20XX` with your installed SketchUp version. Delete only the `DarkModeSKP` folder and the `DarkModeSKP.rb` registration file, then restart SketchUp.
