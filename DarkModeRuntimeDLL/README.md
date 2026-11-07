# DarkModeRuntimeDLL

Standalone native helper for Dark Mode SKP. It supports Windows and SketchUp 2024 or newer.

## Windows Build

Requirements:
- Visual Studio C++ Build Tools
- CMake 3.20 or newer

Configure and build from this folder:

```powershell
cmake -S . -B out/build -A x64
cmake --build out/build --config Release --target DarkModeRuntimeDLL
```

`./build.ps1` runs the same CMake configure/build flow.

Output:
- CMake build directory: `out/build/`
- Extension runtime: `../DarkModeSKP/DarkModeSKP/main.dll`

## Distribution

Include `CMakeLists.txt`, `build.ps1`, `src/`, and this README in source distributions. Do not include `out/`, `staged/`, or `tools/` in a SketchUp extension package.

## Exported API

- `darkmode_skp_initialize() -> 1 when observers are installed`
- `darkmode_skp_prepare_icon_theme(enabled) -> 1 when the live stock SVG color patch is configured`
- `darkmode_skp_set_dark_mode(enabled) -> 1 on success`
- `darkmode_skp_dark_mode_applied() -> 1 when dark mode is currently applied`
- `darkmode_skp_theme_info(buffer, buffer_len) -> bytes copied`

## Stock icon colors

The optional icon theme scans raw stock SVG payloads already loaded in the SketchUp process. It changes `#0e416c` to `#1b70a8`, `#ff0404` to `#df3f2f`, and `#231F20` to the disabled light gray `#9da9b8`. An SVG whose only hexadecimal color is `#000000` changes that color to `#ffffff`; black-and-white SVGs remain unchanged. The shared `#252A2E` 3D Warehouse and `#363545` chevron colors become `#ffffff`. It applies a separate, uniquely signed `default_env.svg` patch for its dark card, border, and artwork colors. The direct live tray-chevron resource probe is retained in the runtime but intentionally disabled. Every other color remains unchanged. It then clears the Qt pixmap cache and redraws SketchUp windows so it also works after stock icons were cached.

`icon-theme-status.txt` includes `chevron-resource-colors`, which remains `0` while the direct tray-chevron resource probe is disabled, and `default-env-colors` for the targeted environment-card substitutions.

No replacement SVG files, `.rcc` overlay, or SketchUp installation files are used. The changes exist only for the active SketchUp session and are discarded when SketchUp exits.