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

Set `DARKMODE_DEPLOY_PLUGIN_RUNTIME=OFF` when building while SketchUp has the extension DLL loaded. Deployment defaults to `ON`.

## Toolbar Titles

The Qt6 floating-toolbar title override intercepts `CC_TitleBar` drawing for floating `NonCustomizableToolbar` widgets and `CC_ToolButton` only for their identified close controls. Other controls and docked toolbars retain their original rendering. Disabling dark mode clears close-button tracking and restores the original style methods.

SketchUp leaves the title option's window flags at zero and its subcontrols at their constructor defaults. For that path, the override matches `SketchUpProxyStyle`: reserve 16 logical pixels on the right, use weight 700, obtain `SE_DockWidgetTitleBarText` from the active style, elide right using the painter's font metrics, and draw with left/vertical-center alignment. The full title surface, including its perimeter, is flat `#303030`; retaining the original light edge against a dark interior made it look raised.

The close control is SketchUp's existing auto-raised `QToolButton` with its `dlg_close` icon, not a `SC_TitleBarCloseButton`. The native layout uses margins `(1, 1, 2, 1)` and positions this control from its own size hint. The title renderer excludes its actual child-widget rectangle; the close renderer uses Qt's own `CE_ToolButtonLabel` drawing as a transparent mask and recolors that mask. The icon shape, antialiasing, pressed offset, size, hit target, and layout margins remain unchanged. The background stays `#303030` in normal, hover, and pressed states, with no hover effect. Only while enabled and pressed, Qt's `qDrawShadePanel` draws a square, recessed one-pixel bevel without an interior fill. It uses a copied `QStyleOptionComplex`, the native `SC_ToolButton` rectangle, SketchUp's active-subcontrol/auto-raise state handling, and a dark palette. It deliberately does not delegate the bevel to `PE_PanelButtonTool`, whose theme-dependent rounded outline did not match the requested appearance. The glyph remains `#d8dbe2`, or `#707070` when disabled, and the title strip remains flat. The toolbar type, floating state, current parent, and child at the title-close position are checked before intercepting a button. The `toolbar-title-draws` and `toolbar-close-draws` counters are read live when `theme_info` is requested.

## Scene Search Icon

SketchUp 2026 rasterizes `:/dlg_search` from SVG into a cached pixmap-backed `QIcon`. Qt derives its disabled appearance from that icon. Reassigning the same icon does not recolor the cached pixels.

The Qt6 override brackets `CE_PushButton` only for the verified `CPageTabs` search-button member at offset `0x28`, checking its parent vtable and exact button identity. It passes the original style option and painter to the original renderer without replacing the option or changing its icon. A thread-local paint scope identifies the search icon by its shared `QIcon` data pointer. Imports of the size/DPR overload of `QIcon::pixmap` in Qt6Widgets and SketchUp are intercepted, but only returned pixmaps for that exact icon during that paint scope are tinted: `#d8dbe2` when enabled and `#707070` when disabled. Every other request passes through unchanged.

Qt still chooses the native icon mode/state, size, device-pixel ratio, placement, and button chrome. Dark-mode refresh repaints the widget without calling `setIcon`. The 32x32 search pixmap contains a full-height separator at its own left edge, four logical pixels inside the centered 40-pixel-wide button. Tinting made its endpoints conspicuous over the button border. For this icon size, the returned pixmap clears only the two-pixel-high caps in its first two columns, covering the separator and its antialiased edge. All remaining alpha and DPR are preserved, and the cached source is not modified. No button-level clip is added. The magnifying glass, button painting, hit area, scene-count enablement logic, adjacent add-scene control, and tray styling are unchanged.

This override follows dark mode independently of the optional stock-icon resource patch. Disabling dark mode restores the original style method and pixmap import slots; `scene-search-draws` reports live intercepted paints. Discovery retains bounded parent/child traversal for window-show events. The owner vtable RVA and member offset are verified against SketchUp 2026 26.2.243; other executable layouts fail the identity check rather than recoloring unrelated buttons.

Read-only inspection in light and dark modes found the search button at 40x32 logical pixels with zero content margins, inside a 32-pixel-tall `CPageTabs`. Both modes retain a Fusion-backed `SketchUpProxyStyle`; dark mode adds an inherited stylesheet wrapper. Earlier button-edge clips missed the separator because it belongs to the inset icon, not the button's outer edge. A bounded live before/after capture verified that trimming the pixmap exposes the native button border at both ends while retaining the central line. The user also confirmed the improved appearance. The original `QTabWidget::pane` styling remains restored: background `#242424` and a one-pixel `#3a3a3a` border. No container restyling is needed for the icon correction.

## Component Folder Backings

The browser paints three stacked rectangles behind folder thumbnails using hard-coded `#e6e6e6`, `#f0f0f0`, and `#ffffff` fills. These are separate from the folder resource image, so the existing image-background override does not affect them. In dark mode, they become `#242424`, `#282828`, and `#2b2b2b`. Rectangle geometry, folder artwork, labels, and component-preview images are unchanged.

Folder outlines use softer `#5a5a5a` edges in dark mode. A separate `QPainter::drawRects` import hook validates and recognizes only the three folder draw calls, at return RVAs `0x3e74d1`, `0x3e757f`, and `0x3e762c`. It copies the native `#a0a0a0` pen, changes only its color, and saves/restores painter state around the draw. Cosmetic line width, stacking, unrelated component borders, and hover/selection painting remain native; nonmatching pen colors pass through.

The override intercepts SketchUp's RGB `QColor` constructor import only after validating all three indirect calls and their constructor target. It applies only within the existing browser paint scope, at return RVAs `0x3e7492`, `0x3e7540`, and `0x3e75ed`, with the expected opaque native colors. These addresses are verified against SketchUp 2026 26.2.243. Other calls pass through, including unrelated white artwork. Disabling restores the import.

## Components Filter Icons

The Components browser caches the magnifier as a 24x24 pixmap inside `ContentBrowserSearchBar`. A SketchUp-only import hook for the exact three-argument `QPainter::drawPixmap` overload recognizes that member by its offset (`0x28`), owner vtable (`0x1c799f8`), and stored 24x24 size. It tints a copy during dark-mode painting, leaving the cached source, destination rectangle, alpha, and device-pixel ratio intact.

The navigation dropdown is a flat 16x24 `QPushButton` with the native 8x8 `:/drop_arrow` icon. Its identity is checked against the `ContentBrowserPage` UI pointer at `0x148`, navigation member at UI offset `0x28`, and adjacent verified search-bar type at `0x30`. Its painting reuses the scoped push-button icon tint without changing the original style option, icon, geometry, or menu behavior. Other browser buttons are not selected by these checks. These layouts are verified against SketchUp 2026 26.2.243.

Both icons use `#d8dbe2` when enabled and `#707070` when disabled. Theme changes repaint existing controls; disabling dark mode restores native import/style hooks and cached icon colors. This operates independently of the optional stock-icon resource patch.

## Search SketchUp

The command-search popup is the native Qt `OmnibarDialog`, not a web view or the scene-search dialog. SketchUp 2026 26.2.243 uses primary vtable RVA `0x1c61d58`, inherited `QFrame` painting, a `QLineEdit`, and a `QTreeView` with `OmnibarItemDelegate`. The delegate reads result text and category backgrounds from its option palette; its fixed four-pixel selection accent remains native.

A color-only stylesheet is attached only after the exact dialog-vtable check. It covers the frame, grip/no-results labels, input, and results, without setting margins, padding, fonts, border widths, or dimensions. Result hover and selection use the shared popup-list highlight `#6f94ce`; input text selection retains `#4772b3`. Existing dialogs are refreshed on theme changes, and newly opened dialogs are handled by the window-show observer. Disabling removes only the added rules and preserves the original stylesheet. Reading that stylesheet uses the installed Qt MSVC member-return ABI (`this`, then output buffer) and does not sanitize or truncate it for logging.

Labels retain transparent backgrounds so the full-width grip does not paint over the native frame's inset side borders.

## Distribution

Include `CMakeLists.txt`, `build.ps1`, `src/`, and this README in source distributions. Do not include `out/`, `staged/`, `tools/`, or `tests/` in a SketchUp extension package.

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