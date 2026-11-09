#include "../src/dark_mode_skp.cpp"

#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template<typename Function>
Function qt_export(HMODULE module, const char* name) {
  FARPROC address = GetProcAddress(module, name);
  if (!address) {
    throw std::runtime_error(std::string("Missing Qt export: ") + name);
  }
  return reinterpret_cast<Function>(address);
}

ToolbarTitleDrawFn g_test_original_draw = nullptr;
unsigned int g_test_sub_controls = 0;
unsigned int g_test_title_flags = 0;
int g_test_draw_calls = 0;
const void* g_test_image = nullptr;
int g_test_image_width = 0;
int g_test_image_height = 0;
unsigned int(__cdecl* g_test_pixel)(const void*, int, int) = nullptr;
std::vector<unsigned int> g_test_original_pixels;
void* g_test_real_style = nullptr;
void* g_test_close_button = nullptr;
HMODULE g_test_widgets = nullptr;
HMODULE g_test_gui = nullptr;
int g_test_text_rect[4] = {};
int g_test_geometry_calls = 0;
int g_test_text_calls = 0;
size_t g_test_text_length = 0;

void* __cdecl record_dock_rect(const void*, void* result, int element, const void* option, const void* widget) {
  const auto* dock = static_cast<const Qt6DockTitleOptionView*>(option);
  require(element == 30 && dock->type == 9, "Native dock-title geometry was not requested");
  require(dock->rect[0] == 8 && dock->rect[1] == 4 && dock->rect[2] == 215 && dock->rect[3] == 27,
    "Native 16-pixel close-button reservation changed");
  void** table = *static_cast<void***>(g_test_real_style);
  reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, const void*)>(table[23])(
    g_test_real_style, result, element, option, widget);
  std::memcpy(g_test_text_rect, result, sizeof(g_test_text_rect));
  ++g_test_geometry_calls;
  return result;
}

void __cdecl record_dock_text(const void*, void* painter, const void* rect, int flags, const void* palette,
                             bool enabled, const void* text, int role) {
  require(std::memcmp(rect, g_test_text_rect, sizeof(g_test_text_rect)) == 0,
    "Text did not use the native style's exact inset");
  require(flags == 0x81 && role == 0, "Native title alignment or text role changed");
  const void* font = reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.painter_font)(painter);
  require(qt_export<int(__cdecl*)(const void*)>(g_test_gui, "?weight@QFont@@QEBA?AW4Weight@1@XZ")(font) == 700,
    "Native bold title weight changed");
  if (g_test_text_length > 25) {
    require(static_cast<const long long*>(text)[2] < static_cast<long long>(g_test_text_length),
      "Long title was not elided to its native text area");
  }
  void** table = *static_cast<void***>(g_test_real_style);
  reinterpret_cast<void(__cdecl*)(const void*, void*, const void*, int, const void*, bool, const void*, int)>(table[18])(
    g_test_real_style, painter, rect, flags, palette, enabled, text, role);
  ++g_test_text_calls;
}

void __cdecl record_original_draw(const void* style, int control, const void* option,
                                  void* painter, const void* widget) {
  const auto* title = static_cast<const Qt6TitleBarOptionView*>(option);
  g_test_sub_controls = title->sub_controls;
  g_test_title_flags = title->title_bar_flags;
  ++g_test_draw_calls;
  g_test_original_draw(g_test_real_style, control, option, painter, widget);
  if (g_test_close_button) {
    alignas(16) unsigned char region[64] = {};
    qt_export<void(__cdecl*)(void*)>(g_test_gui, "??0QRegion@@QEAA@XZ")(region);
    const int position[2] = {214, 6};
    qt_export<void(__cdecl*)(void*, void*, const void*, const void*, unsigned int)>(g_test_widgets,
      "?render@QWidget@@QEAAXPEAVQPainter@@AEBVQPoint@@AEBVQRegion@@V?$QFlags@W4RenderFlag@QWidget@@@@@Z")(
        g_test_close_button, painter, position, region, 3);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.region_destructor)(region);
  }
  g_test_original_pixels.clear();
  for (int row = 0; row < g_test_image_height; ++row) {
    for (int column = 0; column < g_test_image_width; ++column) {
      g_test_original_pixels.push_back(g_test_pixel(g_test_image, column, row));
    }
  }
}

void check_render(const QtLookup& lookup, HMODULE widgets, HMODULE gui, void* widget,
                  bool active, bool enabled, int scale, const char* text, bool close_button,
                  bool sketchup_defaults = false) {
  void* style = qwidget_style(lookup, widget);
  require(style != nullptr, "No widget style");
  void** vtable = *static_cast<void***>(style);
  g_test_original_draw = reinterpret_cast<ToolbarTitleDrawFn>(vtable[24]);
  g_test_real_style = style;
  g_test_geometry_calls = 0;
  g_test_text_calls = 0;
  g_test_text_length = std::strlen(text);
  void* observed_vtable[27];
  std::memcpy(observed_vtable, vtable, sizeof(observed_vtable));
  observed_vtable[23] = reinterpret_cast<void*>(&record_dock_rect);
  observed_vtable[18] = reinterpret_cast<void*>(&record_dock_text);
  void** observed_style = observed_vtable;

  alignas(16) unsigned char option_storage[256];
  std::memset(option_storage, 0xa5, sizeof(option_storage));
  qt_export<void(__cdecl*)(void*)>(widgets, "??0QStyleOptionTitleBar@@QEAA@XZ")(option_storage);
  auto* title = reinterpret_cast<Qt6TitleBarOptionView*>(option_storage);
  require(title->type == 0xf0005 && title->version == 1, "Title option type ABI mismatch");
  require(title->sub_controls == 0xffffffff, "Title subcontrol offset ABI mismatch");
  for (size_t offset = sizeof(Qt6TitleBarOptionView); offset < sizeof(option_storage); ++offset) {
    require(option_storage[offset] == 0xa5, "Title option exceeds verified ABI size");
  }
  qt_export<void(__cdecl*)(void*, const void*)>(widgets,
    "?initFrom@QStyleOption@@QEAAXPEBVQWidget@@@Z")(option_storage, widget);
  title->rect[0] = 8;
  title->rect[1] = 4;
  title->rect[2] = 231;
  title->rect[3] = 27;
  title->state = (enabled ? 1 : 0) | (active ? 0x10000 : 0);
  if (!sketchup_defaults) {
    title->title_bar_state = active ? 0x10000 : 0;
    title->title_bar_flags = 0x3000;
    title->sub_controls = 0x100 | (close_button ? 0x8 : 0);
  }
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(title->text);
  reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(
    title->text, text, static_cast<long long>(std::strlen(text)));
  unsigned char original_option[sizeof(Qt6TitleBarOptionView)];
  std::memcpy(original_option, title, sizeof(original_option));

  alignas(16) unsigned char image[256] = {};
  alignas(16) unsigned char painter[256] = {};
  qt_export<void(__cdecl*)(void*, int, int, int)>(gui, "??0QImage@@QEAA@HHW4Format@0@@Z")(
    image, 240 * scale, 32 * scale, 5);
  qt_export<void(__cdecl*)(void*, double)>(gui, "?setDevicePixelRatio@QImage@@QEAAXN@Z")(image, scale);
  qt_export<void(__cdecl*)(void*, unsigned int)>(gui, "?fill@QImage@@QEAAXI@Z")(image, 0xffff00ff);
  qt_export<void(__cdecl*)(void*, void*)>(gui, "??0QPainter@@QEAA@PEAVQPaintDevice@@@Z")(painter, image);
  require(qt_export<bool(__cdecl*)(const void*)>(gui, "?isActive@QPainter@@QEBA_NXZ")(painter),
    "QImage painter is inactive");

  g_test_draw_calls = 0;
  g_test_image = image;
  g_test_image_width = 240 * scale;
  g_test_image_height = 32 * scale;
  g_test_pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(gui, "?pixel@QImage@@QEBAIHH@Z");
  const LONG draw_count = g_toolbar_title_draw_count;
  require(draw_non_customizable_toolbar_title(lookup, sketchup_defaults ? &observed_style : style,
    title, painter, widget, record_original_draw),
    "Title renderer rejected a valid option");
  if (sketchup_defaults) {
    require(g_test_geometry_calls == 1 && g_test_text_calls == 1, "Native dock-title text path was bypassed");
  }
  require(g_toolbar_title_draw_count == draw_count + 1, "Shared hook incorrectly themed an ordinary QWidget");
  require(g_test_draw_calls == 1, "Original button renderer was not called exactly once");
  require(g_test_sub_controls == title->sub_controls, "Original title/button flags changed");
  require(g_test_title_flags == title->title_bar_flags, "Window flags changed");
  require(std::memcmp(original_option, title, sizeof(original_option)) == 0, "Original option mutated");
  require(!qt_export<bool(__cdecl*)(const void*)>(gui, "?hasClipping@QPainter@@QEBA_NXZ")(painter),
    "Painter clipping was not restored");
  qt_export<void(__cdecl*)(void*)>(gui, "??1QPainter@@QEAA@XZ")(painter);

  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(gui, "?pixel@QImage@@QEBAIHH@Z");
  int button_rect[4] = {0, 0, -1, -1};
  if (close_button) {
    reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, int, const void*)>(vtable[26])(
      style, button_rect, 5, title, 8, widget);
    require(button_rect[2] >= button_rect[0] && button_rect[3] >= button_rect[1], "Close-button geometry is empty");
  }
  size_t dark_pixels = 0;
  size_t light_pixels = 0;
  for (int row = 0; row < 32 * scale; ++row) {
    for (int column = 0; column < 240 * scale; ++column) {
      const unsigned int color = pixel(image, column, row);
      const bool inside = column >= 8 * scale && column < 232 * scale && row >= 4 * scale && row < 28 * scale;
      if (!inside) {
        require(color == 0xffff00ff, "Title drawing escaped its rectangle");
      } else {
        const bool native_frame = sketchup_defaults &&
          (column < 9 * scale || column >= 230 * scale || row < 6 * scale || row >= 26 * scale);
        if (native_frame) {
          require(color == 0xff303030, "Title perimeter retained a light or beveled edge");
        }
        if (g_test_close_button && column >= 214 * scale && column < 230 * scale &&
            row >= 6 * scale && row < 26 * scale) {
          require(color == g_test_original_pixels[static_cast<size_t>(row * g_test_image_width + column)],
            "Native child close-button pixels changed");
          continue;
        }
        if (close_button && column >= button_rect[0] * scale && column < (button_rect[2] + 1) * scale &&
            row >= button_rect[1] * scale && row < (button_rect[3] + 1) * scale) {
          if (color != g_test_original_pixels[static_cast<size_t>(row * g_test_image_width + column)]) {
            std::fprintf(stderr, "button mismatch at %d,%d rect=%d,%d,%d,%d actual=%08x original=%08x\n",
              column, row, button_rect[0], button_rect[1], button_rect[2], button_rect[3], color,
              g_test_original_pixels[static_cast<size_t>(row * g_test_image_width + column)]);
          }
          require(color == g_test_original_pixels[static_cast<size_t>(row * g_test_image_width + column)],
            "Original caption-button pixels were overwritten");
        }
        dark_pixels += color == 0xff303030;
        light_pixels += ((color >> 16) & 255) > 150 && ((color >> 8) & 255) > 150 && (color & 255) > 150;
        require(color != 0xffff00ff, "Title strip contains an unpainted pixel");
      }
    }
  }
  require(dark_pixels > static_cast<size_t>(224 * 24 * scale * scale / 2), "Title strip is not predominantly dark");
  if (*text) {
    require(light_pixels > 0, "Light title text was not rendered");
  } else if (!close_button) {
    const int title_area = 224 * 24 - (g_test_close_button ? 16 * 20 : 0);
    require(dark_pixels == static_cast<size_t>(title_area * scale * scale), "Empty title interior is not uniformly dark");
  }
  qt_export<void(__cdecl*)(void*)>(gui, "??1QImage@@UEAA@XZ")(image);
  qt_export<void(__cdecl*)(void*)>(widgets, "??1QStyleOptionTitleBar@@QEAA@XZ")(title);
  std::printf("PASS active=%d enabled=%d scale=%d close=%d defaults=%d text=%s dark=%zu light=%zu\n",
    active, enabled, scale, close_button, sketchup_defaults, *text ? "present" : "empty", dark_pixels, light_pixels);
}

bool g_test_bright_close_panel = false;
void* g_test_close_native_style = nullptr;

void __cdecl draw_bright_close_panel(const void*, int, const void*, void*, const void*) {
  require(false, "Close-button bevel fell back to the rounded theme primitive");
}

void __cdecl forward_close_label(const void*, int control, const void* option, void* painter, const void* widget) {
  reinterpret_cast<ToolbarTitleDrawFn>((*static_cast<void***>(g_test_close_native_style))[22])(
    g_test_close_native_style, control, option, painter, widget);
}

void __cdecl forward_close_control(const void*, int control, const void* option, void* painter, const void* widget) {
  reinterpret_cast<ToolbarTitleDrawFn>((*static_cast<void***>(g_test_close_native_style))[24])(
    g_test_close_native_style, control, option, painter, widget);
}

void* __cdecl forward_close_subrect(const void*, void* result, int control, const void* option,
                                   unsigned int subcontrol, const void* widget) {
  return reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, unsigned int, const void*)>(
    (*static_cast<void***>(g_test_close_native_style))[26])(
      g_test_close_native_style, result, control, option, subcontrol, widget);
}

void check_close_render(const QtLookup& lookup, void* widget, unsigned int state, double scale) {
  void* style = qwidget_style(lookup, widget);
  void** table = *static_cast<void***>(style);
  void* forwarded_table[27] = {};
  void** forwarded_style = forwarded_table;
  if (g_test_bright_close_panel) {
    g_test_close_native_style = style;
    std::copy(table, table + 27, forwarded_table);
    forwarded_table[21] = reinterpret_cast<void*>(&draw_bright_close_panel);
    forwarded_table[22] = reinterpret_cast<void*>(&forward_close_label);
    forwarded_table[24] = reinterpret_cast<void*>(&forward_close_control);
    forwarded_table[26] = reinterpret_cast<void*>(&forward_close_subrect);
    style = &forwarded_style;
    table = forwarded_table;
  }
  alignas(16) unsigned char option[256] = {};
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??0QStyleOptionToolButton@@QEAA@XZ")(option);
  qt_export<void(__cdecl*)(const void*, void*)>(g_test_widgets,
    "?initStyleOption@QToolButton@@MEBAXPEAVQStyleOptionToolButton@@@Z")(widget, option);
  auto* header = reinterpret_cast<Qt6TitleBarOptionView*>(option);
  require(header->type == 0xf0003 && header->version == 1, "Unexpected tool-button option ABI");
  header->state = state;
  header->sub_controls = 1;
  header->active_sub_controls = (state & 4) ? 1 : 0;
  const int rect[4] = {4, 4, 19, 23};
  std::memcpy(header->rect, rect, sizeof(rect));
  unsigned char original_option[sizeof(option)];
  std::memcpy(original_option, option, sizeof(option));
  alignas(16) unsigned char native_image[256] = {};
  alignas(16) unsigned char dark_image[256] = {};
  alignas(16) unsigned char panel_image[256] = {};
  const int image_width = static_cast<int>(24 * scale);
  const int image_height = static_cast<int>(28 * scale);
  for (void* image : {static_cast<void*>(native_image), static_cast<void*>(dark_image), static_cast<void*>(panel_image)}) {
    reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, image_width, image_height, image == panel_image ? 5 : 6);
    reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(image, scale);
    reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, image == native_image ? 0 : 0xffff00ff);
  }
  alignas(16) unsigned char native_painter[256] = {};
  alignas(16) unsigned char dark_painter[256] = {};
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(native_painter, native_image);
  reinterpret_cast<ToolbarTitleDrawFn>(table[22])(style, 22, option, native_painter, widget);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(native_painter);
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(dark_painter, dark_image);
  const LONG before = g_toolbar_close_draw_count;
  require(draw_non_customizable_toolbar_close(lookup, style, option, dark_painter, widget), "Close-button renderer rejected its native option");
  require(g_toolbar_close_draw_count == before + 1, "Close drawing counter is incorrect");
  require(std::memcmp(original_option, option, sizeof(option)) == 0, "Close-button option, icon, or geometry changed");
  require(!qt_export<bool(__cdecl*)(const void*)>(g_test_gui, "?hasClipping@QPainter@@QEBA_NXZ")(dark_painter),
    "Close-button renderer did not restore clipping");
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(dark_painter);
  const bool enabled = (state & 1) != 0;
  constexpr int background = 48;
  reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(panel_image, 0);
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(native_painter, panel_image);
  if (enabled && (state & 4)) {
    alignas(16) unsigned char panel_option[256] = {};
    qt_export<void(__cdecl*)(void*, const void*)>(g_test_widgets, "??0QStyleOptionComplex@@QEAA@AEBV0@@Z")(panel_option, option);
    auto* panel = reinterpret_cast<Qt6TitleBarOptionView*>(panel_option);
    require(panel->type == 0xf0000, "Unexpected complex panel-option ABI");
    panel->state = state;
    if ((state & 0x1000) && !(state & 0x2000)) {
      panel->state &= ~2u;
    }
    reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, unsigned int, const void*)>(table[26])(
      style, panel->rect, 4, option, 1, widget);
    for (const PaletteRoleColor& role : {
      PaletteRoleColor{1, 48, 48, 48}, PaletteRoleColor{2, 74, 74, 74},
      PaletteRoleColor{3, 56, 56, 56}, PaletteRoleColor{4, 24, 24, 24},
      PaletteRoleColor{5, 32, 32, 32}, PaletteRoleColor{10, 48, 48, 48},
      PaletteRoleColor{11, 16, 16, 16}, PaletteRoleColor{12, 74, 74, 74}
    }) {
      set_qpalette_color_group_role_color(lookup, panel->palette, 5, role);
    }
    qt_export<void(__cdecl*)(void*, const void*, const void*, bool, int, const void*)>(g_test_widgets,
      "?qDrawShadePanel@@YAXPEAVQPainter@@AEBVQRect@@AEBVQPalette@@_NHPEBVQBrush@@@Z")(
        native_painter, panel->rect, panel->palette, true, 1, nullptr);
    qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QStyleOptionComplex@@QEAA@XZ")(panel_option);
  }
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(native_painter);
  const int foreground[3] = {enabled ? 216 : 112, enabled ? 219 : 112, enabled ? 226 : 112};
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  size_t glyph_pixels = 0;
  size_t shaded_panel_pixels = 0;
  int frame_left = image_width;
  int frame_top = image_height;
  int frame_right = -1;
  int frame_bottom = -1;
  for (int row = 0; row < image_height; ++row) {
    for (int column = 0; column < image_width; ++column) {
      const unsigned int actual = pixel(dark_image, column, row);
      if (column < 4 * scale || column >= 20 * scale || row < 4 * scale || row >= 24 * scale) {
        require(actual == 0xffff00ff, "Close-button painting escaped its rectangle");
        continue;
      }
      const unsigned int alpha = pixel(native_image, column, row) >> 24;
      const unsigned int panel = pixel(panel_image, column, row);
      const unsigned int panel_alpha = panel >> 24;
      if (panel_alpha != 0) {
        frame_left = std::min(frame_left, column);
        frame_top = std::min(frame_top, row);
        frame_right = std::max(frame_right, column);
        frame_bottom = std::max(frame_bottom, row);
      }
      const int panel_shade = panel & 255;
      const int panel_channel = (panel_shade * panel_alpha + background * (255 - panel_alpha) + 127) / 255;
      shaded_panel_pixels += panel_channel != background;
      glyph_pixels += alpha != 0;
      require((actual >> 24) == 255, "Close-button background is not opaque");
      if (alpha == 0 && column >= 8 * scale && column < 16 * scale && row >= 8 * scale && row < 20 * scale) {
        require(actual == 0xff303030, "Close-button interior changed on hover or hold");
      }
      for (int channel = 0; channel < 3; ++channel) {
        require(panel_channel <= 128, "Native close-button pressed effect brought back a light-mode panel");
        const int expected = (foreground[channel] * alpha + panel_channel * (255 - alpha) + 127) / 255;
        const int difference = static_cast<int>((actual >> (16 - channel * 8)) & 255) - expected;
        require(difference >= -2 && difference <= 2, "Close glyph shape, placement, antialiasing, or state color changed");
      }
    }
  }
  require(glyph_pixels > 0, "Native close glyph is blank");
  if (enabled && (state & 4) && (state & 0x2000)) {
    require(shaded_panel_pixels > 0, "Pressed close-button panel lost its native border and shading");
    for (int corner_row : {frame_top, frame_bottom}) {
      for (int corner_column : {frame_left, frame_right}) {
        require(pixel(dark_image, corner_column, corner_row) != 0xff303030,
          "Pressed close-button frame has rounded or missing corners");
      }
    }
  } else if (!enabled || !(state & 4)) {
    require(shaded_panel_pixels == 0, "Non-pressed close-button background changed");
  }
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(dark_painter, dark_image);
  const LONG before_passthrough = g_toolbar_close_draw_count;
  reinterpret_cast<ToolbarTitleDrawFn>(table[24])(style, 4, option, dark_painter, widget);
  require(g_toolbar_close_draw_count == before_passthrough, "Shared hook themed an unrelated tool button");
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(dark_painter);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(native_image);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(dark_image);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(panel_image);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QStyleOptionToolButton@@QEAA@XZ")(option);
  std::printf("PASS close state=%x scale=%.2f native-glyph-pixels=%zu\n", state, scale, glyph_pixels);
}

ToolbarTitleDrawFn g_test_scene_original = nullptr;
const void* g_test_scene_button = nullptr;
void* g_test_scene_mask_painter = nullptr;
bool g_test_scene_tint = false;
bool g_test_scene_rendering = false;
int g_test_scene_label_calls = 0;
const void* g_test_scene_native_option = nullptr;

void __cdecl draw_unchanged_scene_option(const void* style, int control, const void* option,
                                       void* painter, const void* widget) {
  require(option == g_test_scene_native_option, "Scene icon tint replaced the native full-button style option");
  g_test_scene_original(style, control, option, painter, widget);
}

void __cdecl record_scene_search_control(const void* style, int control, const void* option,
                                        void* painter, const void* widget) {
  if (control == 0 && widget == g_test_scene_button && !g_test_scene_rendering) {
    ++g_test_scene_label_calls;
    g_test_scene_rendering = true;
    if (g_test_scene_tint) {
      g_test_scene_native_option = option;
      require(draw_scene_search_button(resolve_qt_lookup(), style, option, painter, widget, &draw_unchanged_scene_option, control),
        "Full button paint rejected the scene-search pixmap tint");
    } else {
      alignas(16) unsigned char label_option[256] = {};
      qt_export<void(__cdecl*)(void*, const void*)>(g_test_widgets, "??0QStyleOptionButton@@QEAA@AEBV0@@Z")(label_option, option);
      reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, const void*)>(
        (*reinterpret_cast<void***>(const_cast<void*>(style)))[23])(
          style, reinterpret_cast<Qt6PushButtonOptionView*>(label_option)->rect, 0, option, widget);
      g_test_scene_original(style, 2, label_option, g_test_scene_mask_painter, widget);
      qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QStyleOptionButton@@QEAA@XZ")(label_option);
      g_test_scene_original(style, control, option, painter, widget);
    }
    g_test_scene_rendering = false;
    return;
  }
  g_test_scene_original(style, control, option, painter, widget);
}

void check_scene_search_routing(const QtLookup& lookup, void* widget, unsigned int state, double scale) {
  void* style = qwidget_style(lookup, widget);
  void** slot = *static_cast<void***>(style) + 22;
  g_test_scene_original = reinterpret_cast<ToolbarTitleDrawFn>(*slot);
  g_test_scene_button = widget;
  alignas(16) unsigned char option[256] = {};
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??0QStyleOptionButton@@QEAA@XZ")(option);
  qt_export<void(__cdecl*)(const void*, void*)>(g_test_widgets,
    "?initStyleOption@QPushButton@@MEBAXPEAVQStyleOptionButton@@@Z")(widget, option);
  reinterpret_cast<Qt6TitleBarOptionView*>(option)->state = state;
  alignas(16) unsigned char images[3][256] = {};
  alignas(16) unsigned char painter[256] = {};
  alignas(16) unsigned char mask_painter[256] = {};
  const int width = static_cast<int>(40 * scale);
  const int height = static_cast<int>(32 * scale);
  for (int index = 0; index < 3; ++index) {
    reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(images[index], width, height, 6);
    reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(images[index], scale);
    reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(images[index], index == 2 ? 0 : 0xffc23d56);
  }
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(mask_painter, images[2]);
  g_test_scene_mask_painter = mask_painter;
  DWORD previous_protection = 0;
  require(VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &previous_protection) != 0, "Cannot install scene paint observer");
  *slot = reinterpret_cast<void*>(&record_scene_search_control);
  DWORD ignored_protection = 0;
  VirtualProtect(slot, sizeof(void*), previous_protection, &ignored_protection);
  for (int index = 0; index < 2; ++index) {
    g_test_scene_tint = index != 0;
    g_test_scene_label_calls = 0;
    reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, images[index]);
    reinterpret_cast<ToolbarTitleDrawFn>(*slot)(style, 0, option, painter, widget);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
    require(g_test_scene_label_calls == 1, "Full QPushButton paint bypassed or repeated the hooked control stage");
  }
  require(VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &previous_protection) != 0, "Cannot restore scene paint observer");
  *slot = reinterpret_cast<void*>(g_test_scene_original);
  VirtualProtect(slot, sizeof(void*), previous_protection, &ignored_protection);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(mask_painter);
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  size_t changed_pixels = 0;
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const unsigned int native = pixel(images[0], column, row);
      const unsigned int dark = pixel(images[1], column, row);
      const unsigned int alpha = pixel(images[2], column, row) >> 24;
      if (alpha == 0) {
        require(native == dark, "Scene-search full paint changed the native border or background");
      } else {
        changed_pixels += native != dark;
        if (alpha == 255) {
          require(dark == ((state & 1) ? 0xffd8dbe2 : 0xff707070), "Full scene-search paint used the wrong state color");
        }
      }
    }
  }
  require(changed_pixels > 0, "Full scene-search paint did not recolor the icon");
  require(g_scene_search_pixmap_context == nullptr, "Scene-search pixmap tint escaped its paint scope");
  reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(images[2], 0xffc23d56);
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, images[2]);
  g_test_scene_original(style, 0, option, painter, widget);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      require(pixel(images[2], column, row) == pixel(images[0], column, row),
        "Scene-search tint changed the cached native icon or an unscoped paint");
    }
  }
  for (void* image : {static_cast<void*>(images[0]), static_cast<void*>(images[1]), static_cast<void*>(images[2])}) {
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  }
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QStyleOptionButton@@QEAA@XZ")(option);
  std::printf("PASS full scene-search state=%x scale=%.2f recolored-pixels=%zu\n", state, scale, changed_pixels);
}

void check_omnibar_colors(const QtLookup& lookup) {
  alignas(16) unsigned char frame[1024] = {};
  alignas(16) unsigned char input[1024] = {};
  alignas(16) unsigned char results[1024] = {};
  alignas(16) unsigned char label[1024] = {};
  alignas(16) unsigned char stylesheet[64] = {};
  alignas(16) unsigned char image[256] = {};
  alignas(16) unsigned char painter[256] = {};
  alignas(16) unsigned char region[64] = {};
  const auto set_geometry = qt_export<void(__cdecl*)(void*, int, int, int, int)>(
    g_test_widgets, "?setGeometry@QWidget@@QEAAXHHHH@Z");
  const auto set_visible = qt_export<void(__cdecl*)(void*, bool)>(
    g_test_widgets, "?setVisible@QWidget@@UEAAX_N@Z");
  const auto contents_rect = qt_export<void*(__cdecl*)(const void*, void*)>(
    g_test_widgets, "?contentsRect@QWidget@@QEBA?AVQRect@@XZ");
  const auto size_hint = qt_export<void*(__cdecl*)(const void*, void*)>(
    g_test_widgets, "?sizeHint@QLineEdit@@UEBA?AVQSize@@XZ");
  const auto palette_color = qt_export<const void*(__cdecl*)(const void*, int, int)>(
    g_test_gui, "?color@QPalette@@QEBAAEBVQColor@@W4ColorGroup@1@W4ColorRole@1@@Z");
  const auto rgba = qt_export<unsigned int(__cdecl*)(const void*)>(g_test_gui, "?rgba@QColor@@QEBAIXZ");
  const auto color = [&](void* widget, int role) {
    const void* palette = reinterpret_cast<const void*(__cdecl*)(const void*)>(lookup.widget_palette_proc)(widget);
    return rgba(palette_color(palette, 0, role));
  };
  qt_export<void(__cdecl*)(void*, void*, unsigned int)>(g_test_widgets,
    "??0QFrame@@QEAA@PEAVQWidget@@V?$QFlags@W4WindowType@Qt@@@@@Z")(frame, nullptr, 0);
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "??0QLineEdit@@QEAA@PEAVQWidget@@@Z")(input, frame);
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "??0QTreeView@@QEAA@PEAVQWidget@@@Z")(results, frame);
  qt_export<void(__cdecl*)(void*, void*, unsigned int)>(g_test_widgets,
    "??0QLabel@@QEAA@PEAVQWidget@@V?$QFlags@W4WindowType@Qt@@@@@Z")(label, frame, 0);
  const char* native_stylesheet =
    "QFrame { background-color: #ffffff; border: 1px solid #cccccc; border-radius: 6px; }\n"
    "QLabel { background-color: transparent; border: none; border-radius: 0; }\n"
    "QLineEdit { padding: 4px; border: 1px solid #cccccc; }\n";
  reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(
    stylesheet, native_stylesheet, std::strlen(native_stylesheet));
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(frame, stylesheet);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(stylesheet);
  set_geometry(frame, 0, 0, 494, 226);
  set_geometry(input, 13, 21, 468, 32);
  set_geometry(results, 6, 53, 482, 160);
  set_geometry(label, 6, 13, 482, 8);
  qt_export<void(__cdecl*)(void*, int, bool)>(g_test_widgets,
    "?setAttribute@QWidget@@QEAAXW4WidgetAttribute@Qt@@_N@Z")(frame, 103, true);
  set_visible(frame, true);
  qt_export<void(__cdecl*)(void*)>(g_test_gui, "??0QRegion@@QEAA@XZ")(region);
  reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, 494, 226, 6);
  int original_contents[4][4] = {};
  int original_geometry[4][4] = {};
  int original_hint[2] = {};
  const std::vector<void*> widgets = {frame, input, results, label};
  for (size_t index = 0; index < widgets.size(); ++index) {
    contents_rect(widgets[index], original_contents[index]);
    std::memcpy(original_geometry[index], reinterpret_cast<const void*(__cdecl*)(const void*)>(
      g_toolbar_title_api.widget_geometry)(widgets[index]), sizeof(original_geometry[index]));
  }
  size_hint(input, original_hint);
  require(!apply_omnibar_dialog_style(lookup, frame, true), "Ordinary QFrame matched OmnibarDialog");
  require(!apply_omnibar_dialog_style(lookup, nullptr, true), "Null widget matched OmnibarDialog");
  const unsigned int original_text = color(input, 6);
  unsigned int original_corner = 0;
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  const int position[2] = {};
  for (int phase = 0; phase < 5; ++phase) {
    const bool dark = (phase % 2) != 0;
    require(apply_omnibar_dialog_stylesheet(lookup, frame, dark), "Cannot apply Omnibar color stylesheet");
    std::string applied;
    require(read_omnibar_stylesheet(lookup, frame, applied), "Cannot read Omnibar stylesheet");
    require(apply_omnibar_dialog_stylesheet(lookup, frame, dark), "Repeated Omnibar styling failed");
    std::string repeated;
    require(read_omnibar_stylesheet(lookup, frame, repeated), "Cannot read repeated Omnibar stylesheet");
    require(repeated == applied,
      "Repeated Omnibar styling duplicated the rules");
    reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, 0);
    reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
    qt_export<void(__cdecl*)(void*, void*, const void*, const void*, unsigned int)>(g_test_widgets,
      "?render@QWidget@@QEAAXPEAVQPainter@@AEBVQPoint@@AEBVQRegion@@V?$QFlags@W4RenderFlag@QWidget@@@@@Z")(
        frame, painter, position, region, 2);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
    if (phase == 0) {
      original_corner = pixel(image, 0, 0);
    }
    require(pixel(image, 0, 0) == original_corner, "Omnibar colors changed the rounded frame corner");
    require(pixel(image, 250, 8) == (dark ? 0xff2b2b2b : 0xffffffff), "Omnibar frame uses the wrong background");
    require(color(input, 6) == (dark ? 0xffe6e8ee : original_text), "Omnibar input text color did not switch");
    if (dark) {
      require(color(input, 9) == 0xff242424, "Omnibar input base is not dark");
      require(color(input, 12) == 0xff4772b3, "Omnibar result highlight changed the input selection color");
      require(color(results, 9) == 0xff242424, "Omnibar result base is not dark");
      require(color(results, 6) == 0xffe6e8ee, "Omnibar delegate text palette is not light");
      require(color(results, 12) == 0xff6f94ce, "Omnibar selection does not match the dark popup-list highlight");
      reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, 0xff2b2b2b);
      const auto set_pixel = qt_export<void(__cdecl*)(void*, int, int, unsigned int)>(
        g_test_gui, "?setPixel@QImage@@QEAAXHHI@Z");
      for (int row = 13; row < 21; ++row) {
        set_pixel(image, 6, row, 0xff3a3a3a);
        set_pixel(image, 487, row, 0xff3a3a3a);
      }
      const int grip_position[2] = {6, 13};
      reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
      qt_export<void(__cdecl*)(void*, void*, const void*, const void*, unsigned int)>(g_test_widgets,
        "?render@QWidget@@QEAAXPEAVQPainter@@AEBVQPoint@@AEBVQRegion@@V?$QFlags@W4RenderFlag@QWidget@@@@@Z")(
          label, painter, grip_position, region, 2);
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
      for (int row = 13; row < 21; ++row) {
        require(pixel(image, 6, row) == 0xff3a3a3a && pixel(image, 487, row) == 0xff3a3a3a,
          "Omnibar grip background erased the inset frame border");
      }
    } else {
      require(applied == native_stylesheet, "Disabling Omnibar styling did not restore the original stylesheet");
    }
    for (size_t index = 0; index < widgets.size(); ++index) {
      int current_contents[4] = {};
      contents_rect(widgets[index], current_contents);
      require(std::memcmp(current_contents, original_contents[index], sizeof(current_contents)) == 0,
        "Omnibar styling changed native content insets");
      require(std::memcmp(original_geometry[index], reinterpret_cast<const void*(__cdecl*)(const void*)>(
        g_toolbar_title_api.widget_geometry)(widgets[index]), sizeof(original_geometry[index])) == 0,
        "Omnibar styling changed widget geometry");
    }
    int current_hint[2] = {};
    size_hint(input, current_hint);
    require(std::memcmp(current_hint, original_hint, sizeof(current_hint)) == 0,
      "Omnibar styling changed input padding or preferred size");
  }
  qt_export<void(__cdecl*)(void*)>(g_test_gui, "??1QRegion@@QEAA@XZ")(region);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QLabel@@UEAA@XZ")(label);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QTreeView@@UEAA@XZ")(results);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QLineEdit@@UEAA@XZ")(input);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QFrame@@UEAA@XZ")(frame);
  std::puts("PASS Omnibar colors, native geometry, owner filtering, repeated application and restoration");
}

void check_scene_container_layout(const QtLookup& lookup) {
  alignas(16) unsigned char container[1024] = {};
  alignas(16) unsigned char page[1024] = {};
  alignas(16) unsigned char title[64] = {};
  HMODULE core = GetModuleHandleW(L"Qt6Core.dll");
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "??0QTabWidget@@QEAA@PEAVQWidget@@@Z")(container, nullptr);
  qt_export<void(__cdecl*)(void*, void*, unsigned int)>(g_test_widgets, "??0QWidget@@QEAA@PEAV0@V?$QFlags@W4WindowType@Qt@@@@@Z")(page, container, 0);
  qt_export<void(__cdecl*)(void*)>(core, "??0QString@@QEAA@XZ")(title);
  qt_export<int(__cdecl*)(void*, void*, const void*)>(g_test_widgets,
    "?addTab@QTabWidget@@QEAAHPEAVQWidget@@AEBVQString@@@Z")(container, page, title);
  qt_export<void(__cdecl*)(void*)>(core, "??1QString@@QEAA@XZ")(title);
  qt_export<void(__cdecl*)(void*, bool)>(g_test_widgets, "?setTabBarAutoHide@QTabWidget@@QEAAX_N@Z")(container, true);
  qt_export<void(__cdecl*)(void*, int, int, int, int)>(g_test_widgets, "?setGeometry@QWidget@@QEAAXHHHH@Z")(container, 0, 0, 128, 96);
  qt_export<void(__cdecl*)(void*, int, bool)>(g_test_widgets, "?setAttribute@QWidget@@QEAAXW4WidgetAttribute@Qt@@_N@Z")(container, 103, true);
  qt_export<void(__cdecl*)(void*, bool)>(g_test_widgets, "?setVisible@QWidget@@UEAAX_N@Z")(container, true);
  alignas(16) unsigned char image[256] = {};
  alignas(16) unsigned char painter[256] = {};
  alignas(16) unsigned char region[64] = {};
  reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, 128, 96, 6);
  qt_export<void(__cdecl*)(void*)>(g_test_gui, "??0QRegion@@QEAA@XZ")(region);
  int original_rect[4] = {};
  const int position[2] = {0, 0};
  for (int phase = 0; phase < 3; ++phase) {
    if (phase == 1) {
      require(apply_qt_color_scheme(lookup, true), "Cannot enable Qt dark color scheme for container test");
      require(apply_application_palette(lookup, true), "Cannot apply production dark palette for container test");
    }
    if (phase == 2) {
      alignas(16) unsigned char stylesheet[64] = {};
      const char* text = separator_splitter_dark_qt_stylesheet();
      reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(stylesheet, text, std::strlen(text));
      reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(container, stylesheet);
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(stylesheet);
    }
    qt_export<void(__cdecl*)(void*, unsigned int)>(g_test_gui, "?fill@QImage@@QEAAXI@Z")(image, 0);
    reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
    qt_export<void(__cdecl*)(void*, void*, const void*, const void*, unsigned int)>(g_test_widgets,
      "?render@QWidget@@QEAAXPEAVQPainter@@AEBVQPoint@@AEBVQRegion@@V?$QFlags@W4RenderFlag@QWidget@@@@@Z")(
        container, painter, position, region, 3);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
    const auto* rect = static_cast<const int*>(reinterpret_cast<const void*(__cdecl*)(const void*)>(
      g_toolbar_title_api.widget_geometry)(qwidget_parent_widget(lookup, page)));
    std::printf("scene-container phase=%d page-stack=(%d,%d)-(%d,%d)\n", phase, rect[0], rect[1], rect[2], rect[3]);
    require(rect[2] - rect[0] + 1 >= 120 && rect[3] - rect[1] + 1 >= 88,
      "Scene-container fixture layout was not activated");
    const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
    std::printf("scene-container phase=%d left-edge=%08x top-left=%08x\n", phase, pixel(image, 0, 16), pixel(image, 0, 0));
    if (phase == 2) {
      for (int row = 0; row < 96; ++row) {
        require(pixel(image, 0, row) == 0xff3a3a3a,
          "Tray pane lost its original dark outline color");
      }
      const int dark_pane_rect[4] = {1, 1, 126, 94};
      require(std::memcmp(dark_pane_rect, rect, sizeof(dark_pane_rect)) == 0,
        "Tray pane no longer uses its original one-pixel dark border");
    }
    if (phase == 0) {
      std::memcpy(original_rect, rect, sizeof(original_rect));
    } else if (phase == 1) {
      require(std::memcmp(original_rect, rect, sizeof(original_rect)) == 0,
        "Dark mode changed the scene-container native page inset or border geometry");
    }
  }
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.region_destructor)(region);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QWidget@@UEAA@XZ")(page);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QTabWidget@@UEAA@XZ")(container);
  std::puts("PASS native container geometry and original dark tray outline");
}

void __cdecl draw_oversized_search_edge(const void*, int control, const void* option, void* painter, const void*) {
  require(control == 0, "Scene-search clipping test did not use the full button paint path");
  const auto* button = static_cast<const Qt6PushButtonOptionView*>(option);
  const int edge[4] = {button->rect[0], button->rect[1] - 3, button->rect[0], button->rect[3] + 3};
  alignas(16) unsigned char color[16] = {};
  reinterpret_cast<QtColorRgbCtorFn>(resolve_qt_lookup().qcolor_rgb_ctor_proc)(color, 0, 255, 255, 255);
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, edge, color);
}

void check_scene_search_render(const QtLookup& lookup, void* widget, unsigned int state, double scale) {
  void* style = qwidget_style(lookup, widget);
  const auto original_draw = reinterpret_cast<ToolbarTitleDrawFn>((*static_cast<void***>(style))[22]);
  alignas(16) unsigned char option[256] = {};
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??0QStyleOptionButton@@QEAA@XZ")(option);
  qt_export<void(__cdecl*)(const void*, void*)>(g_test_widgets,
    "?initStyleOption@QPushButton@@MEBAXPEAVQStyleOptionButton@@@Z")(widget, option);
  auto* header = reinterpret_cast<Qt6TitleBarOptionView*>(option);
  require(header->type == 2 && header->version == 1, "Unexpected push-button option ABI");
  header->state = state;
  const int rect[4] = {4, 4, 43, 35};
  std::memcpy(header->rect, rect, sizeof(rect));
  unsigned char original_option[sizeof(option)];
  std::memcpy(original_option, option, sizeof(option));
  alignas(16) unsigned char native_image[256] = {};
  alignas(16) unsigned char dark_image[256] = {};
  alignas(16) unsigned char painter[256] = {};
  const int image_width = static_cast<int>(48 * scale);
  const int image_height = static_cast<int>(40 * scale);
  for (void* image : {static_cast<void*>(native_image), static_cast<void*>(dark_image)}) {
    reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, image_width, image_height, 6);
    reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(image, scale);
    reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, image == native_image ? 0 : 0xff303030);
  }
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, native_image);
  original_draw(style, 2, option, painter, widget);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, dark_image);
  require(draw_scene_search_button(lookup, style, option, painter, widget, original_draw), "Scene-search icon renderer rejected its option");
  require(std::memcmp(original_option, option, sizeof(option)) == 0, "Scene-search option, icon, or geometry changed");
  require(!qt_export<bool(__cdecl*)(const void*)>(g_test_gui, "?hasClipping@QPainter@@QEBA_NXZ")(painter),
    "Scene-search renderer did not restore clipping");
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
  const bool enabled = (state & 1) != 0;
  const int foreground[3] = {enabled ? 216 : 112, enabled ? 219 : 112, enabled ? 226 : 112};
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  size_t glyph_pixels = 0;
  for (int row = 0; row < image_height; ++row) {
    for (int column = 0; column < image_width; ++column) {
      const unsigned int actual = pixel(dark_image, column, row);
      const bool inside = column >= 4 * scale && column < 44 * scale && row >= 4 * scale && row < 36 * scale;
      const unsigned int alpha = inside ? pixel(native_image, column, row) >> 24 : 0;
      glyph_pixels += alpha != 0;
      require((actual >> 24) == 255, "Scene-search renderer changed background opacity");
      for (int channel = 0; channel < 3; ++channel) {
        const int expected = (foreground[channel] * alpha + 48 * (255 - alpha) + 127) / 255;
        const int difference = static_cast<int>((actual >> (16 - channel * 8)) & 255) - expected;
        require(difference >= -2 && difference <= 2, "Scene-search glyph, state color, or surrounding pixels changed");
      }
    }
  }
  require(glyph_pixels > 0, "Scene-search test icon is blank");
  for (bool caller_clip : {false, true}) {
    reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(dark_image, 0xffff00ff);
    reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, dark_image);
    const int caller_rect[4] = {0, 8, 47, 31};
    if (caller_clip) {
      reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, caller_rect, 1);
    }
    require(draw_scene_search_button(lookup, style, option, painter, widget, &draw_oversized_search_edge, 0),
      "Scene-search edge clipping rejected its native option");
    require(qt_export<bool(__cdecl*)(const void*)>(g_test_gui, "?hasClipping@QPainter@@QEBA_NXZ")(painter) == caller_clip,
      "Scene-search edge clipping changed the caller's clip state");
    if (caller_clip) {
      alignas(16) unsigned char marker_color[16] = {};
      reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(marker_color, 0, 255, 0, 255);
      const int marker_rect[4] = {0, 0, 3, 39};
      reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, marker_rect, marker_color);
    }
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
    const int edge_left = static_cast<int>(4 * scale);
    const int edge_right = static_cast<int>(5 * scale + 0.5);
    const int edge_top = static_cast<int>((caller_clip ? 8 : 1) * scale + 0.5);
    const int edge_bottom = static_cast<int>((caller_clip ? 32 : 39) * scale + 0.5);
    for (int row = 0; row < image_height; ++row) {
      for (int column = 0; column < image_width; ++column) {
        unsigned int expected = 0xffff00ff;
        if (column >= edge_left && column < edge_right && row >= edge_top && row < edge_bottom) {
          expected = 0xff00ffff;
        }
        if (caller_clip && column < 4 * scale && row >= 8 * scale && row < 32 * scale) {
          expected = 0xff00ff00;
        }
        require(pixel(dark_image, column, row) == expected,
          "Scene-search icon trim changed the native button paint extent or caller's existing clip");
      }
    }
    require(std::memcmp(original_option, option, sizeof(option)) == 0, "Scene-search clipping changed button geometry");
  }
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(native_image);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(dark_image);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QStyleOptionButton@@QEAA@XZ")(option);
  std::printf("PASS scene-search state=%x scale=%.2f glyph-pixels=%zu\n", state, scale, glyph_pixels);
}

void check_content_browser_folder_colors(const QtLookup& lookup) {
  require(resolve_content_browser_folder_outline_api(lookup), "Folder outline API is unavailable");
  std::vector<unsigned char> executable(0x3e7700);
  auto** import_slot = reinterpret_cast<void**>(executable.data());
  void* original_ctor = reinterpret_cast<void*>(lookup.qcolor_rgb_ctor_proc);
  *import_slot = original_ctor;
  auto** outline_slot = import_slot + 1;
  void* original_draw = reinterpret_cast<void*>(g_content_browser_folder_outline_api.draw_rects);
  *outline_slot = original_draw;
  for (const auto& color : k_content_browser_folder_colors) {
    auto* instruction = executable.data() + color.return_rva - 6;
    instruction[0] = 0xff;
    instruction[1] = 0x15;
    const auto displacement = -static_cast<std::int32_t>(color.return_rva);
    std::memcpy(instruction + 2, &displacement, sizeof(displacement));
    auto* outline_instruction = executable.data() + color.outline_return_rva - 6;
    outline_instruction[0] = 0xff;
    outline_instruction[1] = 0x15;
    const auto outline_displacement = static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(color.outline_return_rva);
    std::memcpy(outline_instruction + 2, &outline_displacement, sizeof(outline_displacement));
  }
  const auto slots = content_browser_folder_color_slots(executable.data(), executable.size(), original_ctor);
  require(slots.size() == 1 && slots.front() == import_slot, "Folder color calls did not resolve to their shared import");
  const auto outline_slots = content_browser_folder_color_slots(executable.data(), executable.size(), original_draw, true);
  require(outline_slots.size() == 1 && outline_slots.front() == outline_slot, "Folder outline calls did not resolve to their own import");
  require(content_browser_folder_color_slots(executable.data(), executable.size(), original_ctor, true).empty(),
    "Folder outline accepted the color-constructor import");
  executable[k_content_browser_folder_colors[1].outline_return_rva - 6] = 0x90;
  require(content_browser_folder_color_slots(executable.data(), executable.size(), original_draw, true).empty(),
    "Changed folder outline call was accepted");
  executable[k_content_browser_folder_colors[1].outline_return_rva - 6] = 0xff;
  require(content_browser_folder_color_slots(executable.data(), 16, original_ctor).empty(), "Short executable accepted for folder colors");
  executable[k_content_browser_folder_colors[1].return_rva - 6] = 0x90;
  require(content_browser_folder_color_slots(executable.data(), executable.size(), original_ctor).empty(),
    "Changed native instruction accepted for folder colors");
  executable[k_content_browser_folder_colors[1].return_rva - 6] = 0xff;
  *import_slot = executable.data();
  require(content_browser_folder_color_slots(executable.data(), executable.size(), original_ctor).empty(),
    "Different imported function accepted for folder colors");
  *import_slot = original_ctor;
  g_content_browser_folder_color_image_base = executable.data();
  g_content_browser_qcolor_rgb_ctor_proc = reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc);
  const auto rgba = qt_export<unsigned int(__cdecl*)(const void*)>(g_test_gui, "?rgba@QColor@@QEBAIXZ");
  const int expected_shades[] = {36, 40, 43};
  for (bool dark : {false, true, false, true}) {
    g_content_browser_text_dark = dark;
    for (int depth : {0, 1, 2}) {
      g_content_browser_paint_scope_depth = depth;
      for (size_t index = 0; index < std::size(k_content_browser_folder_colors); ++index) {
        const auto& native = k_content_browser_folder_colors[index];
        const void* caller = executable.data() + native.return_rva;
        const int shade = content_browser_folder_background_shade(
          caller, native.native_shade, native.native_shade, native.native_shade, 255);
        require(shade == (dark && depth > 0 ? expected_shades[index] : -1), "Folder fill did not follow its exact dark paint scope");
        require(content_browser_folder_background_shade(static_cast<const unsigned char*>(caller) + 1,
          native.native_shade, native.native_shade, native.native_shade, 255) == -1, "Neighboring paint call was recolored");
        require(content_browser_folder_background_shade(caller, 160, 160, 160, 255) == -1, "Native folder outline was recolored");
        require(content_browser_folder_background_shade(caller, native.native_shade, native.native_shade, 0, 255) == -1,
          "Colored folder artwork was recolored");
        require(content_browser_folder_background_shade(caller, native.native_shade, native.native_shade, native.native_shade, 128) == -1,
          "Translucent content was recolored");
        alignas(16) unsigned char color[64] = {};
        require(content_browser_folder_color_hook(color, native.native_shade, native.native_shade, native.native_shade, 255) == color,
          "QColor constructor hook lost its native return value");
        require(rgba(color) == (0xff000000u | (native.native_shade * 0x010101u)),
          "Unrelated QColor construction was recolored inside browser painting");
      }
    }
  }
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  const auto fill_rect = qt_export<void(__cdecl*)(void*, const void*, const void*)>(g_test_gui,
    "?fillRect@QPainter@@QEAAXAEBVQRect@@AEBVQColor@@@Z");
  for (double scale : {1.0, 1.25, 1.5, 2.0}) {
    alignas(16) unsigned char images[3][256] = {};
    const int extent = static_cast<int>(50 * scale);
    for (auto& image : images) {
      reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, extent, extent, 6);
      reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(image, scale);
    }
    auto render = [&](void* image, bool dark, bool mask) {
      g_content_browser_text_dark = dark;
      g_content_browser_paint_scope_depth = 1;
      reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, mask ? 0 : 0xff242424);
      alignas(16) unsigned char painter[256] = {};
      alignas(16) unsigned char color[64] = {};
      alignas(16) unsigned char pen[64] = {};
      reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
      const int outline = mask ? 0 : 160;
      g_content_browser_qcolor_rgb_ctor_proc(color, outline, outline, mask ? 4 : outline, 255);
      qt_export<void(__cdecl*)(void*, const void*)>(g_test_gui, "??0QPen@@QEAA@AEBVQColor@@@Z")(pen, color);
      qt_export<void(__cdecl*)(void*, int)>(g_test_gui, "?setWidth@QPen@@QEAAXH@Z")(pen, 0);
      qt_export<void(__cdecl*)(void*, const void*)>(g_test_gui, "?setPen@QPainter@@QEAAXAEBVQPen@@@Z")(painter, pen);
      for (size_t index = 0; index < std::size(k_content_browser_folder_colors); ++index) {
        const auto& native = k_content_browser_folder_colors[index];
        const int shade = content_browser_folder_background_shade(executable.data() + native.return_rva,
          native.native_shade, native.native_shade, native.native_shade, 255);
        const int fill = shade < 0 ? native.native_shade : shade;
        g_content_browser_qcolor_rgb_ctor_proc(color, mask ? 0 : fill, mask ? 0 : fill,
          mask ? static_cast<int>(index + 1) : fill, 255);
        alignas(16) unsigned char brush[64] = {};
        qt_export<void(__cdecl*)(void*, const void*, int)>(g_test_gui,
          "??0QBrush@@QEAA@AEBVQColor@@W4BrushStyle@Qt@@@Z")(brush, color, 1);
        qt_export<void(__cdecl*)(void*, const void*)>(g_test_gui, "?setBrush@QPainter@@QEAAXAEBVQBrush@@@Z")(painter, brush);
        const int offset = static_cast<int>(index) * 2;
        const int rect[4] = {6 - offset, 2 + offset, 45 - offset, 41 + offset};
        const void* caller = executable.data() + native.outline_return_rva;
        require(!draw_content_browser_folder_outline(lookup, painter, rect, 1, executable.data()),
          "Unrelated rectangle matched folder outlines");
        require(!draw_content_browser_folder_outline(lookup, painter, rect, 2, caller),
          "Multiple rectangles matched a single folder outline");
        const bool painted = !mask && draw_content_browser_folder_outline(lookup, painter, rect, 1, caller);
        require(painted == (dark && !mask), "Folder outline did not follow dark mode");
        if (!painted) {
          reinterpret_cast<ContentBrowserDrawRectsFn>(original_draw)(painter, rect, 1);
        }
        const void* restored_pen = reinterpret_cast<const void*(__cdecl*)(const void*)>(
          g_content_browser_folder_outline_api.painter_pen)(painter);
        alignas(16) unsigned char restored_color[64] = {};
        reinterpret_cast<void*(__cdecl*)(const void*, void*)>(g_content_browser_folder_outline_api.pen_color)(restored_pen, restored_color);
        require(rgba(restored_color) == (mask ? 0xff000004u : 0xffa0a0a0u) &&
          qt_export<int(__cdecl*)(const void*)>(g_test_gui, "?width@QPen@@QEBAHXZ")(restored_pen) == 0,
          "Folder outline changed the caller's color or cosmetic pen width");
        qt_export<void(__cdecl*)(void*)>(g_test_gui, "??1QBrush@@QEAA@XZ")(brush);
      }
      for (int marker : {0, 1}) {
        const int rect[4] = {12, 12 + marker * 20, 32, 25 + marker * 15};
        g_content_browser_qcolor_rgb_ctor_proc(color, mask ? 0 : (marker ? 255 : 185),
          mask ? 0 : (marker ? 255 : 152), mask ? 0 : (marker ? 255 : 85), 255);
        fill_rect(painter, rect, color);
      }
      g_content_browser_qcolor_rgb_ctor_proc(color, 111, 148, 206, 255);
      reinterpret_cast<QtPainterSetPenColorFn>(lookup.qpainter_set_pen_color_proc)(painter, color);
      const int highlight_rect[4] = {2, 2, 45, 45};
      require(!draw_content_browser_folder_outline(lookup, painter, highlight_rect, 1,
        executable.data() + k_content_browser_folder_colors[0].outline_return_rva), "Selection-color pen was recolored");
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
      qt_export<void(__cdecl*)(void*)>(g_test_gui, "??1QPen@@QEAA@XZ")(pen);
    };
    render(images[0], false, false);
    render(images[2], false, true);
    for (bool dark : {true, false, true, false}) {
      render(images[1], dark, false);
      size_t changed_pixels = 0;
      for (int row = 0; row < extent; ++row) {
        for (int column = 0; column < extent; ++column) {
          const unsigned int native = pixel(images[0], column, row);
          const unsigned int actual = pixel(images[1], column, row);
          const unsigned int layer = pixel(images[2], column, row) & 0x00ffffffu;
          require(layer <= 4, "Folder backing mask contains unexpected pixels");
          const int shade = layer == 4 ? 90 : (layer ? expected_shades[layer - 1] : 0);
          const unsigned int expected = dark && layer ? 0xff000000u | (shade * 0x010101u) : native;
          require(actual == expected, "Folder colors changed native artwork, outline thickness, placement or light-mode pixels");
          changed_pixels += actual != native;
        }
      }
      require(dark ? changed_pixels > 0 : changed_pixels == 0, "Folder backing did not restore across theme changes");
    }
    for (auto& image : images) {
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
    }
  }
  *import_slot = reinterpret_cast<void*>(&content_browser_folder_color_hook);
  g_content_browser_folder_color_hooks.push_back({import_slot, original_ctor});
  *outline_slot = reinterpret_cast<void*>(&content_browser_folder_outline_hook);
  g_content_browser_folder_outline_hooks.push_back({outline_slot, original_draw});
  remove_content_browser_text_hooks();
  require(*import_slot == original_ctor && g_content_browser_folder_color_hooks.empty() &&
    *outline_slot == original_draw && g_content_browser_folder_outline_hooks.empty() &&
    !g_content_browser_folder_color_image_base && !g_content_browser_text_dark,
    "Folder color hook was not restored on disable");
  std::puts("PASS folder fill call sites, color guards, dark/light scope and import restoration");
}

void check_content_browser_navigation(const QtLookup& lookup) {
  require(resolve_scene_search_icon_api(lookup), "Navigation pixmap API is unavailable");
  const auto& api = g_scene_search_icon_api;
  alignas(16) unsigned char widget[1024] = {};
  alignas(16) unsigned char image[256] = {};
  alignas(16) unsigned char pixmap[256] = {};
  alignas(16) unsigned char icon[64] = {};
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "??0QPushButton@@QEAA@PEAVQWidget@@@Z")(widget, nullptr);
  qt_export<void(__cdecl*)(void*, bool)>(g_test_widgets, "?setFlat@QPushButton@@QEAAX_N@Z")(widget, true);
  qt_export<void(__cdecl*)(void*, int, int, int, int)>(g_test_widgets,
    "?setGeometry@QWidget@@QEAAXHHHH@Z")(widget, 0, 0, 16, 24);
  reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, 8, 8, 6);
  reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, 0);
  const auto set_pixel = qt_export<void(__cdecl*)(void*, int, int, unsigned int)>(g_test_gui, "?setPixel@QImage@@QEAAXHHI@Z");
  for (int row = 2; row < 6; ++row) {
    for (int column = row - 2; column < 10 - row; ++column) {
      set_pixel(image, column, row, 0xff000000);
    }
  }
  reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned int)>(api.pixmap_from_image)(pixmap, image, 0);
  qt_export<void(__cdecl*)(void*, const void*)>(g_test_gui, "??0QIcon@@QEAA@AEBVQPixmap@@@Z")(icon, pixmap);
  qt_export<void(__cdecl*)(void*, const void*)>(g_test_widgets, "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z")(widget, icon);
  const int icon_size[2] = {8, 8};
  qt_export<void(__cdecl*)(void*, const void*)>(g_test_widgets, "?setIconSize@QAbstractButton@@QEAAXAEBVQSize@@@Z")(widget, icon_size);
  require(!is_content_browser_navigation_button(lookup, widget), "Ordinary push button matched content-browser navigation");
  for (bool styled : {false, true}) {
    if (styled) {
      alignas(16) unsigned char stylesheet[64] = {};
      const char* text = "QPushButton { background-color: #2b2b2b; color: #d8dbe2; }";
      reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(stylesheet, text, std::strlen(text));
      reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(widget, stylesheet);
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(stylesheet);
    }
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
      for (unsigned int state : {1u, 0u, 0x2001u, 0x2005u, 0x101u}) {
        check_scene_search_routing(lookup, widget, state, scale);
      }
    }
  }
  const int expected_geometry[4] = {0, 0, 15, 23};
  require(std::memcmp(expected_geometry, reinterpret_cast<const void*(__cdecl*)(const void*)>(
    g_toolbar_title_api.widget_geometry)(widget), sizeof(expected_geometry)) == 0,
    "Content-browser navigation button changed its native 16x24 geometry");
  remove_scene_search_label_hooks();
  qt_export<void(__cdecl*)(void*)>(g_test_gui, "??1QIcon@@QEAA@XZ")(icon);
  reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(pixmap);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QPushButton@@UEAA@XZ")(widget);
  std::puts("PASS native 8x8 navigation icon, 16x24 button, styles, state colors, placement and cache");
}

void check_content_browser_search_pixmap(const QtLookup& lookup, double scale) {
  require(resolve_scene_search_icon_api(lookup), "Content-browser pixmap API is unavailable");
  require(resolve_content_browser_search_icon_api(lookup), "Content-browser painter API is unavailable");
  alignas(16) unsigned char owner[0x58] = {};
  const void* expected_vtable = owner;
  *reinterpret_cast<const void**>(owner) = expected_vtable;
  auto* icon_size = reinterpret_cast<int*>(owner + 0x40);
  icon_size[0] = icon_size[1] = 24;
  require(content_browser_search_pixmap_owner(owner + 0x28, expected_vtable) == owner,
    "Content-browser cached pixmap identity was not recognized");
  require(!content_browser_search_pixmap_owner(owner + 0x30, expected_vtable), "Unrelated pixmap matched content-browser owner");
  require(!content_browser_search_pixmap_owner(owner + 0x28, owner + 8), "Wrong owner vtable was accepted");
  icon_size[0] = 32;
  require(!content_browser_search_pixmap_owner(owner + 0x28, expected_vtable), "Unexpected content-browser icon size was accepted");
  alignas(16) unsigned char page[0x150] = {};
  const void* navigation_button = page;
  const void* controls[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, navigation_button, owner};
  *reinterpret_cast<const void***>(page + 0x148) = controls;
  require(content_browser_navigation_owner_matches(page, navigation_button, expected_vtable),
    "Exact content-browser navigation button was not recognized");
  require(!content_browser_navigation_owner_matches(page, owner, expected_vtable), "Unrelated browser button was accepted");
  require(!content_browser_navigation_owner_matches(page, navigation_button, page), "Unrelated search owner was accepted");
  require(!content_browser_navigation_owner_matches(nullptr, navigation_button, expected_vtable), "Missing browser page was accepted");
  const auto& api = g_scene_search_icon_api;
  const int extent = static_cast<int>(24 * scale);
  alignas(16) unsigned char source[256] = {};
  alignas(16) unsigned char pixmap[256] = {};
  reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(source, extent, extent, 6);
  reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(source, scale);
  reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(source, 0);
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  const auto set_pixel = qt_export<void(__cdecl*)(void*, int, int, unsigned int)>(g_test_gui, "?setPixel@QImage@@QEAAXHHI@Z");
  for (int row = static_cast<int>(4 * scale); row < 20 * scale; ++row) {
    for (int column = static_cast<int>(4 * scale); column < 20 * scale; ++column) {
      set_pixel(source, column, row, column == static_cast<int>(4 * scale) ? 0x80101010 : 0xff202020);
    }
  }
  reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned int)>(api.pixmap_from_image)(pixmap, source, 0);
  for (bool enabled : {true, false, true}) {
    alignas(16) unsigned char tinted[256] = {};
    alignas(16) unsigned char image[256] = {};
    alignas(16) unsigned char unchanged[256] = {};
    require(create_content_browser_search_pixmap(lookup, pixmap, tinted, enabled), "Cannot tint content-browser search pixmap");
    reinterpret_cast<void*(__cdecl*)(const void*, void*)>(api.pixmap_to_image)(tinted, image);
    reinterpret_cast<void*(__cdecl*)(const void*, void*)>(api.pixmap_to_image)(pixmap, unchanged);
    require(qt_export<int(__cdecl*)(const void*)>(g_test_gui, "?width@QImage@@QEBAHXZ")(image) == extent &&
      qt_export<int(__cdecl*)(const void*)>(g_test_gui, "?height@QImage@@QEBAHXZ")(image) == extent,
      "Content-browser icon changed its raster size");
    require(reinterpret_cast<double(__cdecl*)(const void*)>(g_toolbar_title_api.device_ratio)(image) == scale,
      "Content-browser icon changed its DPR");
    for (int row = 0; row < extent; ++row) {
      for (int column = 0; column < extent; ++column) {
        const unsigned int native = pixel(source, column, row);
        const unsigned int actual = pixel(image, column, row);
        require(pixel(unchanged, column, row) == native, "Content-browser tint changed the cached source");
        require((actual >> 24) == (native >> 24), "Content-browser tint changed icon alpha");
        if ((native >> 24) == 255) {
          require(actual == (enabled ? 0xffd8dbe2 : 0xff707070), "Content-browser icon used the wrong state color");
        }
      }
    }
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(unchanged);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
    reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(tinted);
  }
  reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(pixmap);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(source);
  void* import_slot = reinterpret_cast<void*>(&content_browser_search_draw_pixmap_hook);
  void* original = reinterpret_cast<void*>(g_content_browser_draw_pixmap);
  require(g_content_browser_search_pixmap_hooks.empty(), "Content-browser test inherited patched imports");
  g_content_browser_search_pixmap_hooks.push_back({&import_slot, original});
  require(set_content_browser_search_icon_hook(lookup, true) && set_content_browser_search_icon_hook(lookup, true) &&
    g_content_browser_search_pixmap_hooks.size() == 1, "Repeated content-browser hook installation was not idempotent");
  require(set_content_browser_search_icon_hook(lookup, false) && !g_content_browser_search_dark &&
    g_content_browser_search_pixmap_hooks.empty() && import_slot == original, "Content-browser import was not restored on disable");
  std::printf("PASS content-browser search pixmap alpha, states, cache and DPR scale=%.2f\n", scale);
}

void check_scene_search_pixmap_edge(const QtLookup& lookup, double scale) {
  require(resolve_scene_search_icon_api(lookup), "Scene-search pixmap API is unavailable");
  const auto& api = g_scene_search_icon_api;
  const int extent = static_cast<int>(32 * scale);
  const int size[2] = {32, 32};
  alignas(16) unsigned char source[256] = {};
  alignas(16) unsigned char pixmap[256] = {};
  alignas(16) unsigned char icon[64] = {};
  reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(source, extent, extent, 6);
  reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(source, scale);
  reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(source, 0);
  const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
  const auto set_pixel = qt_export<void(__cdecl*)(void*, int, int, unsigned int)>(g_test_gui, "?setPixel@QImage@@QEAAXHHI@Z");
  const int strip_width = static_cast<int>(2 * scale + 0.5);
  for (int row = 0; row < extent; ++row) {
    for (int column = 0; column < extent; ++column) {
      if (column < strip_width) {
        set_pixel(source, column, row, column < scale ? 0xff202020 : 0x550b0b0b);
      } else if (column >= 12 * scale && column < 20 * scale && row >= 12 * scale && row < 20 * scale) {
        set_pixel(source, column, row, 0xff202020);
      }
    }
  }
  reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned int)>(api.pixmap_from_image)(pixmap, source, 0);
  qt_export<void(__cdecl*)(void*, const void*)>(g_test_gui, "??0QIcon@@QEAA@AEBVQPixmap@@@Z")(icon, pixmap);
  reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(pixmap);
  for (bool enabled : {true, false}) {
    alignas(16) unsigned char images[3][256] = {};
    const int mode = enabled ? 0 : 1;
    for (int pass = 0; pass < 3; ++pass) {
      if (pass == 1) {
        const SceneSearchPixmapContext context = {*reinterpret_cast<const void* const*>(icon), enabled};
        g_scene_search_pixmap_context = &context;
        scene_search_pixmap_hook(icon, pixmap, size, scale, mode, 1);
        g_scene_search_pixmap_context = nullptr;
      } else {
        reinterpret_cast<void*(__cdecl*)(const void*, void*, const void*, double, int, int)>(api.icon_pixmap)(
          icon, pixmap, size, scale, mode, 1);
      }
      reinterpret_cast<void*(__cdecl*)(const void*, void*)>(api.pixmap_to_image)(pixmap, images[pass]);
      reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(pixmap);
    }
    require(reinterpret_cast<double(__cdecl*)(const void*)>(g_toolbar_title_api.device_ratio)(images[0]) ==
      reinterpret_cast<double(__cdecl*)(const void*)>(g_toolbar_title_api.device_ratio)(images[1]),
      "Scene-search edge trim changed pixmap DPR");
    for (int row = 0; row < extent; ++row) {
      for (int column = 0; column < extent; ++column) {
        const unsigned int native = pixel(images[0], column, row);
        const unsigned int actual = pixel(images[1], column, row);
        const bool trimmed = column < strip_width &&
          (row < strip_width || row >= static_cast<int>(30 * scale + 0.5));
        require(pixel(images[2], column, row) == native, "Scene-search edge trim modified the cached source icon");
        if (trimmed) {
          require(actual == 0, "Scene-search pixmap still contains the full-height separator overhang");
        } else {
          require((actual >> 24) == (native >> 24), "Scene-search edge trim changed unrelated icon alpha");
          if ((native >> 24) == 255) {
            require(actual == (enabled ? 0xffd8dbe2 : 0xff707070), "Scene-search edge trim changed the state color");
          }
        }
      }
    }
    for (auto& image : images) {
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
    }
  }
  qt_export<void(__cdecl*)(void*)>(g_test_gui, "??1QIcon@@QEAA@XZ")(icon);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(source);
  std::printf("PASS scene-search 32px pixmap edge scale=%.2f\n", scale);
}

void check_scene_search_face_bounds(const QtLookup& lookup) {
  alignas(16) unsigned char style_name[32] = {};
  reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(style_name, "Fusion", 6);
  void* base_style = qt_export<void*(__cdecl*)(const void*)>(g_test_widgets,
    "?create@QStyleFactory@@SAPEAVQStyle@@AEBVQString@@@Z")(style_name);
  require(base_style != nullptr, "Cannot create scene-search Fusion base style");
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(style_name);
  alignas(16) unsigned char proxy_style[256] = {};
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "??0QProxyStyle@@QEAA@PEAVQStyle@@@Z")(proxy_style, base_style);
  alignas(16) unsigned char parent[1024] = {};
  qt_export<void(__cdecl*)(void*, void*, unsigned int)>(g_test_widgets, "??0QWidget@@QEAA@PEAV0@V?$QFlags@W4WindowType@Qt@@@@@Z")(
    parent, nullptr, 0);
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "?setStyle@QWidget@@QEAAXPEAVQStyle@@@Z")(parent, proxy_style);
  alignas(16) unsigned char widget[1024] = {};
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "??0QPushButton@@QEAA@PEAVQWidget@@@Z")(widget, parent);
  qt_export<void(__cdecl*)(void*, void*)>(g_test_widgets, "?setStyle@QWidget@@QEAAXPEAVQStyle@@@Z")(widget, proxy_style);
  qt_export<void(__cdecl*)(void*, int, int, int, int)>(g_test_widgets, "?setGeometry@QWidget@@QEAAXHHHH@Z")(widget, 0, 0, 40, 32);
  for (int stage : {0, 1, 2, 3}) {
    void* hints = reinterpret_cast<void*(__cdecl*)()>(lookup.gui_style_hints_proc)();
    reinterpret_cast<void(__cdecl*)(void*, int)>(lookup.style_hints_set_color_scheme_proc)(hints, stage >= 2 ? 2 : 1);
    if (stage == 3) {
      alignas(16) unsigned char stylesheet[64] = {};
      const char* text = separator_splitter_dark_qt_stylesheet();
      reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(stylesheet, text, std::strlen(text));
      reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(parent, stylesheet);
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(stylesheet);
    }
    require(apply_application_palette(lookup, stage != 0), "Could not configure scene face palette");
    require(apply_clicked_palette_to_widget(lookup, widget, stage != 0), "Could not configure scene button palette");
    flush_qt_pending_events(lookup);
    for (unsigned int state : {0x10001u, 0x10000u}) {
      qt_export<void(__cdecl*)(void*, bool)>(g_test_widgets, "?setEnabled@QWidget@@QEAAX_N@Z")(widget, (state & 1) != 0);
      alignas(16) unsigned char option[256] = {};
      qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??0QStyleOptionButton@@QEAA@XZ")(option);
      qt_export<void(__cdecl*)(const void*, void*)>(g_test_widgets,
        "?initStyleOption@QPushButton@@MEBAXPEAVQStyleOptionButton@@@Z")(widget, option);
      auto* button = reinterpret_cast<Qt6PushButtonOptionView*>(option);
      button->state = state;
      alignas(16) unsigned char image[256] = {};
      alignas(16) unsigned char painter[256] = {};
      reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(image, 40, 32, 6);
      reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, 0);
      reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
      void* style = qwidget_style(lookup, widget);
      reinterpret_cast<ToolbarTitleDrawFn>((*static_cast<void***>(style))[22])(style, 0, option, painter, widget);
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
      int bounds[4] = {40, 32, -1, -1};
      size_t painted = 0;
      const auto pixel = qt_export<unsigned int(__cdecl*)(const void*, int, int)>(g_test_gui, "?pixel@QImage@@QEBAIHH@Z");
      for (int row = 0; row < 32; ++row) {
        for (int column = 0; column < 40; ++column) {
          if ((pixel(image, column, row) >> 24) != 0) {
            bounds[0] = std::min(bounds[0], column);
            bounds[1] = std::min(bounds[1], row);
            bounds[2] = std::max(bounds[2], column);
            bounds[3] = std::max(bounds[3], row);
            ++painted;
          }
        }
      }
      require(painted != 0, "Scene face bounds probe rendered no button chrome");
      const int expected_rect[4] = {0, 0, 39, 31};
      require(std::memcmp(button->rect, expected_rect, sizeof(expected_rect)) == 0,
        "Scene-search native option changed the fixed button size");
      require(std::memcmp(bounds, expected_rect, sizeof(expected_rect)) == 0,
        "Scene-search palette or stylesheet reduced the native Fusion face bounds");
      std::printf("PASS scene-search-face stage=%d enabled=%u rect=%d,%d,%d,%d paint=%d,%d,%d,%d pixels=%zu\n",
        stage, state & 1,
        button->rect[0], button->rect[1], button->rect[2], button->rect[3], bounds[0], bounds[1], bounds[2], bounds[3], painted);
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
      qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QStyleOptionButton@@QEAA@XZ")(option);
    }
  }
  apply_qt_color_scheme(lookup, false);
  apply_application_palette(lookup, false);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QPushButton@@UEAA@XZ")(widget);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QWidget@@UEAA@XZ")(parent);
  qt_export<void(__cdecl*)(void*)>(g_test_widgets, "??1QProxyStyle@@UEAA@XZ")(proxy_style);
}

}

int main(int argc, char** argv) {
  if (argc != 2 || !*argv[1]) {
    std::puts("Set DARKMODE_QT_TEST_DIR to the directory containing SketchUp's Qt6 DLLs.");
    return 77;
  }
  try {
    const std::filesystem::path directory(argv[1]);
    require(SetDllDirectoryW(directory.c_str()) != 0, "Unable to set Qt DLL search directory");
    const auto plugin_directory = directory / "plugins";
    SetEnvironmentVariableW(L"QT_PLUGIN_PATH", plugin_directory.c_str());
    SetEnvironmentVariableW(L"QT_QPA_PLATFORM", L"windows:darkmode=0:fontengine=gdi");
    require(LoadLibraryW((directory / "Qt6Core.dll").c_str()) != nullptr, "Cannot load Qt6Core");
    HMODULE gui = LoadLibraryW((directory / "Qt6Gui.dll").c_str());
    HMODULE widgets = LoadLibraryW((directory / "Qt6Widgets.dll").c_str());
    require(gui && widgets, "Cannot load Qt6Gui/Qt6Widgets");
    g_test_gui = gui;
    g_test_widgets = widgets;
    alignas(16) unsigned char application[2048] = {};
    int qt_argc = 1;
    char program_name[] = "DarkModeToolbarTitleTest";
    char* qt_argv[] = {program_name, nullptr};
    qt_export<void(__cdecl*)(void*, int&, char**, int)>(widgets, "??0QApplication@@QEAA@AEAHPEAPEADH@Z")(
      application, qt_argc, qt_argv, 0x060901);
    const QtLookup& lookup = resolve_qt_lookup();
    require(resolve_toolbar_title_paint_api(lookup), "Title painting API is unavailable");
    alignas(16) unsigned char style_name[32] = {};
    reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(style_name, "Fusion", 6);
    require(qt_export<void*(__cdecl*)(const void*)>(widgets, "?setStyle@QApplication@@SAPEAVQStyle@@AEBVQString@@@Z")(
      style_name) != nullptr, "Cannot select SketchUp's Fusion base style");
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(style_name);
    alignas(16) unsigned char widget[1024] = {};
    qt_export<void(__cdecl*)(void*, void*, unsigned int)>(widgets, "??0QWidget@@QEAA@PEAV0@V?$QFlags@W4WindowType@Qt@@@@@Z")(
      widget, nullptr, 0);
    for (int styled = 0; styled != 2; ++styled) {
      if (styled) {
        require(apply_non_customizable_toolbar_flat_border_style_to_widget(lookup, widget, true),
          "Could not install the stylesheet wrapper");
      }
      for (int scale : {1, 2}) {
        check_render(lookup, widgets, gui, widget, true, true, scale, "", false);
        check_render(lookup, widgets, gui, widget, true, true, scale, "ExtensionStore", false);
        check_render(lookup, widgets, gui, widget, false, true, scale, "ExtensionStore", true);
        check_render(lookup, widgets, gui, widget, false, false, scale, "A title longer than the available toolbar width", true);
        check_render(lookup, widgets, gui, widget, true, true, scale, "ExtensionStore", false, true);
        check_render(lookup, widgets, gui, widget, true, true, scale, "", false, true);
        check_render(lookup, widgets, gui, widget, false, true, scale, "ExtensionStore", false, true);
        check_render(lookup, widgets, gui, widget, false, false, scale, "A title longer than the available toolbar width", false, true);
      }
    }
    alignas(16) unsigned char close_button[1024] = {};
    qt_export<void(__cdecl*)(void*, void*)>(widgets, "??0QToolButton@@QEAA@PEAVQWidget@@@Z")(close_button, widget);
    qt_export<void(__cdecl*)(void*, bool)>(widgets, "?setAutoRaise@QToolButton@@QEAAX_N@Z")(close_button, true);
    qt_export<void(__cdecl*)(void*, int, int, int, int)>(widgets, "?setGeometry@QWidget@@QEAAXHHHH@Z")(
      close_button, 214, 6, 16, 20);
    alignas(16) unsigned char close_icon[64] = {};
    void* button_style = qwidget_style(lookup, close_button);
    void** button_vtable = *static_cast<void***>(button_style);
    reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, const void*)>(button_vtable[31])(
      button_style, close_icon, 3, nullptr, close_button);
    qt_export<void(__cdecl*)(void*, const void*)>(widgets, "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z")(close_button, close_icon);
    qt_export<void(__cdecl*)(void*)>(gui, "??1QIcon@@QEAA@XZ")(close_icon);
    qt_export<void(__cdecl*)(void*, bool)>(widgets, "?setVisible@QWidget@@UEAAX_N@Z")(close_button, true);
    require(qwidget_child_at(lookup, widget, 229, 15) == close_button, "Native close-button fixture was not found");
    g_test_close_button = close_button;
    for (int scale : {1, 2}) {
      check_render(lookup, widgets, gui, widget, true, true, scale, "", false, true);
      check_render(lookup, widgets, gui, widget, true, true, scale, "ExtensionStore", false, true);
      check_render(lookup, widgets, gui, widget, false, true, scale, "A title longer than the available toolbar width", false, true);
    }
    const int expected_button_rect[4] = {214, 6, 229, 25};
    const void* actual_button_rect = reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.widget_geometry)(close_button);
    require(std::memcmp(expected_button_rect, actual_button_rect, sizeof(expected_button_rect)) == 0,
      "Native close-button geometry changed");
    require(install_non_customizable_toolbar_title_hook(lookup, close_button), "Close style hook installation failed");
    g_toolbar_title_close_buttons.push_back({widget, close_button, {8, 4, 231, 27}});
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
      for (unsigned int state : {0x1001u, 0x3001u, 0x3005u, 0x1005u, 0x1000u, 0x3004u}) {
        check_close_render(lookup, close_button, state, scale);
      }
    }
    remove_non_customizable_toolbar_title_hooks();
    require(g_toolbar_title_close_buttons.empty(), "Close-button tracking was not cleared on disable");
    g_test_bright_close_panel = true;
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
      check_close_render(lookup, close_button, 0x3005, scale);
    }
    g_test_bright_close_panel = false;
    alignas(16) unsigned char search_button[1024] = {};
    qt_export<void(__cdecl*)(void*, void*)>(widgets, "??0QPushButton@@QEAA@PEAVQWidget@@@Z")(search_button, widget);
    alignas(16) unsigned char search_icon[64] = {};
    qt_export<void*(__cdecl*)(const void*, void*)>(widgets, "?icon@QAbstractButton@@QEBA?AVQIcon@@XZ")(close_button, search_icon);
    alignas(16) unsigned char search_pixmap[256] = {};
    qt_export<void*(__cdecl*)(const void*, void*, int, int, int, int)>(gui,
      "?pixmap@QIcon@@QEBA?AVQPixmap@@HHW4Mode@1@W4State@1@@Z")(search_icon, search_pixmap, 16, 16, 0, 1);
    qt_export<void(__cdecl*)(void*)>(gui, "??1QIcon@@QEAA@XZ")(search_icon);
    qt_export<void(__cdecl*)(void*, const void*)>(gui, "??0QIcon@@QEAA@AEBVQPixmap@@@Z")(search_icon, search_pixmap);
    qt_export<void(__cdecl*)(void*)>(gui, "??1QPixmap@@UEAA@XZ")(search_pixmap);
    qt_export<void(__cdecl*)(void*, const void*)>(widgets, "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z")(search_button, search_icon);
    qt_export<void(__cdecl*)(void*)>(gui, "??1QIcon@@QEAA@XZ")(search_icon);
    qt_export<void(__cdecl*)(void*, int, int, int, int)>(widgets, "?setGeometry@QWidget@@QEAAXHHHH@Z")(search_button, 0, 0, 40, 32);
    const void* synthetic_owner[6] = {widget, nullptr, nullptr, nullptr, nullptr, search_button};
    require(scene_search_owner_matches(synthetic_owner, search_button, widget), "Exact scene-search member was not recognized");
    require(!scene_search_owner_matches(synthetic_owner, close_button, widget), "Non-search child matched the scene-search member");
    require(!scene_search_owner_matches(synthetic_owner, search_button, close_button), "Unrelated owner vtable matched CPageTabs");
    require(!is_scene_search_button(lookup, search_button), "Ordinary QPushButton matched scene search");
    void* search_style = qwidget_style(lookup, search_button);
    void** search_slot = *static_cast<void***>(search_style) + 22;
    void* search_original = *search_slot;
    require(install_scene_search_label_hook(lookup, search_button), "Scene-search hook installation failed");
    require(install_scene_search_label_hook(lookup, search_button) && g_scene_search_hooks.size() == 1,
      "Scene-search hook installation was not idempotent");
    require(!g_scene_search_pixmap_hooks.empty(), "Scene-search pixmap import hook was not installed");
    const auto saved_pixmap_hooks = g_scene_search_pixmap_hooks;
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
      for (unsigned int state : {1u, 0u, 0x2001u, 0x2005u, 1u}) {
        check_scene_search_render(lookup, search_button, state, scale);
      }
    }
    require(g_scene_search_draw_count == 0, "Scene-search hook recolored an unrelated button");
    remove_scene_search_label_hooks();
    require(!g_scene_search_dark && g_scene_search_hooks.empty() && g_scene_search_pixmap_hooks.empty() &&
      *search_slot == search_original,
      "Scene-search native style was not restored on disable");
    for (const auto& hook : saved_pixmap_hooks) {
      require(*hook.slot == hook.original_proc, "Scene-search pixmap import was not restored on disable");
    }
    require(install_scene_search_label_hook(lookup, search_button), "Scene-search hook could not be re-enabled");
    remove_scene_search_label_hooks();
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
      for (unsigned int state : {1u, 0u, 0x2001u, 0x2005u, 0x101u, 0x105u}) {
        check_scene_search_routing(lookup, search_button, state, scale);
      }
    }
    remove_scene_search_label_hooks();
    require(g_scene_search_pixmap_hooks.empty(), "Scene-search pixmap test left an import patched");
    qt_export<void(__cdecl*)(void*)>(widgets, "??1QPushButton@@UEAA@XZ")(search_button);
    g_test_close_button = nullptr;
    qt_export<void(__cdecl*)(void*)>(widgets, "??1QToolButton@@UEAA@XZ")(close_button);
    void* style = qwidget_style(lookup, widget);
    void** slot = &(*static_cast<void***>(style))[24];
    void* original = *slot;
    require(install_non_customizable_toolbar_title_hook(lookup, widget), "Hook installation failed");
    const size_t hook_count = g_toolbar_title_hooks.size();
    require(install_non_customizable_toolbar_title_hook(lookup, widget), "Repeated hook installation failed");
    require(g_toolbar_title_hooks.size() == hook_count, "Repeated install duplicated the hook");
    check_render(lookup, widgets, gui, widget, true, true, 1, "Unrelated QWidget", true);
    g_toolbar_title_dark = false;
    check_render(lookup, widgets, gui, widget, false, true, 1, "Disabled title hook", true);
    remove_non_customizable_toolbar_title_hooks();
    require(*slot == original && g_toolbar_title_hooks.empty() && !g_toolbar_title_dark, "Hook was not restored");
    set_theme_info("Toolbar test");
    char status[2048] = {};
    const LONG saved_draw_count = g_toolbar_title_draw_count;
    darkmode_skp_theme_info(status, sizeof(status));
    require(std::string(status).find("toolbar-title-draws=" + std::to_string(saved_draw_count)) != std::string::npos,
      "Theme info did not report the current draw count");
    InterlockedIncrement(&g_toolbar_title_draw_count);
    darkmode_skp_theme_info(status, sizeof(status));
    require(std::string(status).find("toolbar-title-draws=" + std::to_string(saved_draw_count + 1)) != std::string::npos,
      "Theme info returned a stale draw count");
    InterlockedExchange(&g_toolbar_title_draw_count, saved_draw_count);
    check_scene_container_layout(lookup);
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
      check_content_browser_search_pixmap(lookup, scale);
      check_scene_search_pixmap_edge(lookup, scale);
    }
    check_scene_search_face_bounds(lookup);
    check_content_browser_navigation(lookup);
    check_content_browser_folder_colors(lookup);
    check_omnibar_colors(lookup);
    qt_export<void(__cdecl*)(void*)>(widgets, "??1QWidget@@UEAA@XZ")(widget);
    qt_export<void(__cdecl*)(void*)>(widgets, "??1QApplication@@UEAA@XZ")(application);
    std::puts("PASS title pixels, text, clipping, option ownership, buttons, and hook lifecycle");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}