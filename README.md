
# DarkModeSKP

DarkModeSKP is a Windows extension for SketchUp that applies a dark theme to Qt and native Windows interface elements. It combines a Ruby extension with a native C++ helper.

<img width="1906" height="1073" alt="Screenshot 2026-11-07 121147" src="https://github.com/user-attachments/assets/fb002f7c-55cb-4e25-b7b5-09ca20ebbd81" />

*The navigation cube in the top-right corner is the [Viewport Advanced SKP](https://extensions.sketchup.com/extension/cd54da6d-9823-4ac7-9135-974f8988d6a9) extension.*

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

### Window-opening performance

The native helper publishes an immutable Qt symbol lookup instead of rediscovering modules on every window-show or paint event. Once initialized, callers use it by reference without a mutex or string copies. An early lookup before Qt finishes loading is retried. Late scene-search icon repair checks the shown widget's bounded parent chain and searches only the matching page-tabs subtree. If the version-specific icon cannot be found, unrelated dialogs do not trigger application-wide widget scans. Menu handling ignores child-window show events. There is no production polling timer.

Live measurements on Windows, SketchUp **26.1.256**, on **2026-11-08** compared the original source at `562dab6` with the optimized helper. The development probe times the three native window-show callbacks inside SketchUp. The following values are elapsed time spent inside callbacks over each tested interaction, **not total file-dialog opening time**:

[Recorded samples and measurement limits](DarkModeRuntimeDLL/benchmark-results.json) include callback counts, totals, and maxima. Each interaction below is a single recorded sample, not a latency distribution; treat sub-millisecond values as approximate. The baseline Open test used an empty model, the baseline 3D Export test used an unsaved rectangle, and the optimized checks used an unsaved cube with two scenes.

| Interaction | Original callbacks | Optimized callbacks |
| --- | ---: | ---: |
| Open picker, then Cancel; Default Tray visible (44 events each) | 5,562.5544 ms | 0.0335 ms |
| 3D Export picker (47 / 48 events) | 5,771.2653 ms | 0.0303 ms |
| 100 Qt lookup calls on the UI thread | 384.4803 ms | < 0.001 ms |

The original Open cycle spent 5,518.6321 ms in the scene-search icon hook alone. Its icon status remained zero, so every show event repeated a search across the application's widgets (the installed inventory contained 1,753 widgets). Disabling only that hook reduced the same Open cycle to 49.4022 ms. Keeping targeted icon repair together with the shared lookup was faster and preserves late-window styling. Caching alone would leave the repeated widget walks; polling/throttling would retain unnecessary work and delay styling, so those approaches were not selected.

Open, Save As, 2D Export, 3D Export, Preferences, menus, About SketchUp, the floating Default Tray, and scene search were checked in the real application with the optimized probe build. Dialogs drew promptly and retained dark colors. Export and Save pickers were canceled. Tests used unsaved geometry; no user model was saved or overwritten. These measurements establish the improvement on this SketchUp build; they are not compatibility certification for every supported release.

The normal Release DLL was then rebuilt without probe exports, installed, and checked after a fresh launch without the startup sampler. Open and Preferences retained prompt drawing and dark colors; the floating Default Tray was visible. The probe is separate from the normal CMake target and extension package.

After rebuilding `main.dll`, restart SketchUp to load the replacement. Repeat the dialog/appearance checks on each SketchUp release before distribution.

### Repeating the native timing measurements

`DarkModeRuntimeDLL/performance_probe.cpp` is a development-only translation unit that includes the helper source and adds timing exports. Compile it **instead of** `src/dark_mode_skp.cpp` with the same x64 C++17 Release flags and Windows libraries. Its `DARKMODE_SOURCE_PATH` macro can select an older source revision. Temporarily use that DLL as the installed helper, then restart SketchUp with `-RubyStartup` and the absolute path to `DarkModeRuntimeDLL/performance_probe.rb`.

Supply explicit file paths through `DARKMODE_PERF_OUTPUT` (JSON samples) and `DARKMODE_PERF_CONTROL` (test generation). Put `0:test-name` in the control file for normal hooks, or `1:test-name` to bypass only late icon repair. Changing the generation resets the counters. The sampler runs only in this development startup script. Optional `DARKMODE_PERF_FIXTURE=1` creates an unsaved cube and two scenes, and refuses an existing/nonempty model. Close that fixture without saving when finished, restore a normal helper build, and restart without the startup script. **Never distribute the probe DLL or include these probe sources in an RBZ.**
