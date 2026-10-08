#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <richedit.h>
#include <intrin.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::mutex g_info_mutex;
std::string g_theme_info = "Dark Mode SKP helper not used";
volatile LONG g_dark_mode_enabled = 0;
volatile LONG g_model_services_cef_hook_installed = 0;
volatile LONG g_model_services_cef_observer_only = 0;
HWINEVENTHOOK g_about_sketchup_window_hook = nullptr;
HWND g_about_sketchup_window = nullptr;
DWORD g_about_sketchup_window_hook_thread_id = 0;
std::vector<HWND> g_tray_mismatch_style_hwnds;
std::vector<HWND> g_tray_mismatch_leaf_hwnds;
int g_vcb_original_typed_input_background = -1;
alignas(16) unsigned char g_original_app_palette_storage[256] = {};
bool g_original_app_palette_constructed = false;

struct MeasurementsEntryPadSubclass {
  HWND hwnd = nullptr;
  WNDPROC original_proc = nullptr;
};

struct MeasurementsEntryPadEditColor {
  HWND hwnd = nullptr;
  COLORREF original_background = CLR_INVALID;
};

struct VcbStatusPaintHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

struct TagHeaderColorBackup {
  void* widget = nullptr;
  void* object = nullptr;
  alignas(16) unsigned char color_storage[6][16] = {};
};

struct TagHeaderPaintHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

struct ComponentNavigatorPaintHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

struct ComponentNavigatorTextDrawHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

struct ContentBrowserPaintHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

using QtColorRgbCtorFn = void(__cdecl*)(void*, int, int, int, int);
using QtColorRgbFn = unsigned int(__cdecl*)(const void*);
using QtImageDefaultCtorFn = void(__cdecl*)(void*);
using QtImageDestructorFn = void(__cdecl*)(void*);
using QtImageFillColorFn = void(__cdecl*)(void*, const void*);
using QtPainterSetPenColorFn = void(__cdecl*)(void*, const void*);
using ContentBrowserDrawTextOptionFn = void(__cdecl*)(void*, const void*, const void*, const void*);
using ContentBrowserDrawTextFlagsFn = void(__cdecl*)(void*, const void*, int, const void*, void*);

struct ContentBrowserTextDrawHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

std::vector<HWND> g_measurements_entry_pad_roots;
std::vector<MeasurementsEntryPadSubclass> g_measurements_entry_pad_subclasses;
std::vector<MeasurementsEntryPadEditColor> g_measurements_entry_pad_edit_colors;
HBRUSH g_measurements_entry_pad_background_brush = nullptr;
std::vector<VcbStatusPaintHook> g_vcb_edit_paint_hooks;
bool g_vcb_edit_paint_dark = false;
volatile LONG g_vcb_edit_paint_hook_invocation_count = 0;
std::vector<VcbStatusPaintHook> g_vcb_status_paint_hooks;
void* g_vcb_status_paint_widget = nullptr;
bool g_vcb_status_paint_dark = false;
bool g_vcb_status_paint_palette_applied = false;
volatile LONG g_vcb_status_paint_hook_invocation_count = 0;
std::vector<std::unique_ptr<TagHeaderColorBackup>> g_tag_header_color_backups;
TagHeaderPaintHook g_tag_header_paint_hook;
bool g_tag_header_paint_dark = false;
ComponentNavigatorPaintHook g_component_navigator_paint_hook;
std::vector<ComponentNavigatorTextDrawHook> g_component_navigator_draw_text_option_hooks;
std::vector<ComponentNavigatorTextDrawHook> g_component_navigator_draw_text_flags_hooks;
QtColorRgbCtorFn g_component_navigator_qcolor_rgb_ctor_proc = nullptr;
QtPainterSetPenColorFn g_component_navigator_set_pen_color_proc = nullptr;
bool g_component_navigator_paint_dark = false;
volatile LONG g_component_navigator_paint_hook_invocation_count = 0;
volatile LONG g_component_navigator_text_draw_hook_invocation_count = 0;
thread_local int g_component_navigator_paint_scope_depth = 0;
ContentBrowserPaintHook g_content_browser_paint_hook;
std::vector<ContentBrowserTextDrawHook> g_content_browser_draw_text_option_hooks;
std::vector<ContentBrowserTextDrawHook> g_content_browser_draw_text_flags_hooks;
std::vector<ContentBrowserTextDrawHook> g_content_browser_qimage_fill_color_hooks;
QtColorRgbCtorFn g_content_browser_qcolor_rgb_ctor_proc = nullptr;
QtColorRgbFn g_content_browser_qcolor_rgb_proc = nullptr;
QtImageDefaultCtorFn g_content_browser_qimage_default_ctor_proc = nullptr;
QtImageDestructorFn g_content_browser_qimage_destructor_proc = nullptr;
QtImageFillColorFn g_content_browser_qimage_fill_color_proc = nullptr;
QtPainterSetPenColorFn g_content_browser_set_pen_color_proc = nullptr;
const unsigned char* g_content_browser_image_generator_begin = nullptr;
const unsigned char* g_content_browser_image_generator_end = nullptr;
const unsigned char* g_content_browser_resource_image_renderer_begin = nullptr;
const unsigned char* g_content_browser_resource_image_renderer_end = nullptr;
std::unordered_set<void*> g_content_browser_image_cache_reset_widgets;
bool g_content_browser_text_dark = false;
volatile LONG g_content_browser_text_draw_hook_invocation_count = 0;
volatile LONG g_content_browser_paint_hook_invocation_count = 0;
volatile LONG g_content_browser_folder_image_fill_hook_invocation_count = 0;
thread_local int g_content_browser_paint_scope_depth = 0;
struct CefBaseRefCounted {
  size_t size = 0;
  void(__fastcall* add_ref)(CefBaseRefCounted* self) = nullptr;
  int(__fastcall* release)(CefBaseRefCounted* self) = nullptr;
  int(__fastcall* has_one_ref)(CefBaseRefCounted* self) = nullptr;
  int(__fastcall* has_at_least_one_ref)(CefBaseRefCounted* self) = nullptr;
};

struct CefString {
  char16_t* str = nullptr;
  size_t length = 0;
  void(__fastcall* dtor)(char16_t* str) = nullptr;
};

struct CefFrame;
struct CefLoadHandler;

struct CefClient {
  CefBaseRefCounted base;
  void* get_audio_handler = nullptr;
  void* get_command_handler = nullptr;
  void* get_context_menu_handler = nullptr;
  void* get_dialog_handler = nullptr;
  void* get_display_handler = nullptr;
  void* get_download_handler = nullptr;
  void* get_drag_handler = nullptr;
  void* get_find_handler = nullptr;
  void* get_focus_handler = nullptr;
  void* get_frame_handler = nullptr;
  void* get_permission_handler = nullptr;
  void* get_jsdialog_handler = nullptr;
  void* get_keyboard_handler = nullptr;
  void* get_life_span_handler = nullptr;
  CefLoadHandler*(__fastcall* get_load_handler)(CefClient* self) = nullptr;
};

struct CefLoadHandler {
  CefBaseRefCounted base;
  void* on_loading_state_change = nullptr;
  void* on_load_start = nullptr;
  void(__fastcall* on_load_end)(CefLoadHandler* self, void* browser, CefFrame* frame, int http_status_code) = nullptr;
  void* on_load_error = nullptr;
};

struct CefFrame {
  CefBaseRefCounted base;
  void* is_valid = nullptr;
  void* undo = nullptr;
  void* redo = nullptr;
  void* cut = nullptr;
  void* copy = nullptr;
  void* paste = nullptr;
  void* del = nullptr;
  void* select_all = nullptr;
  void* view_source = nullptr;
  void* get_source = nullptr;
  void* get_text = nullptr;
  void* load_request = nullptr;
  void* load_url = nullptr;
  void(__fastcall* execute_java_script)(CefFrame* self, const CefString* code, const CefString* script_url, int start_line) = nullptr;
  int(__fastcall* is_main)(CefFrame* self) = nullptr;
  void* is_focused = nullptr;
  void* get_name = nullptr;
  void* get_identifier = nullptr;
  void* get_parent = nullptr;
  CefString*(__fastcall* get_url)(CefFrame* self) = nullptr;
};

static_assert(sizeof(CefBaseRefCounted) == 40, "Unexpected CEF ref-counted base layout");
static_assert(offsetof(CefClient, get_load_handler) == 152, "Unexpected CEF client layout");
static_assert(offsetof(CefLoadHandler, on_load_end) == 56, "Unexpected CEF load handler layout");
static_assert(offsetof(CefFrame, execute_java_script) == 144, "Unexpected CEF frame layout");
static_assert(offsetof(CefFrame, get_url) == 192, "Unexpected CEF frame layout");

using CefBrowserHostCreateBrowserFn = int(__fastcall*)(
  const void* window_info,
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
);

using CefBrowserHostCreateBrowserSyncFn = void*(__fastcall*)(
  const void* window_info,
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
);

using CefBrowserViewCreateFn = void*(__fastcall*)(
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
);

struct CefLoadEndHook {
  CefLoadHandler* handler = nullptr;
  void* original_proc = nullptr;
};

std::mutex g_model_services_cef_mutex;
CefBrowserHostCreateBrowserFn g_original_cef_browser_create = nullptr;
CefBrowserHostCreateBrowserSyncFn g_original_cef_browser_create_sync = nullptr;
CefBrowserViewCreateFn g_original_cef_browser_view_create = nullptr;
void** g_cef_browser_create_iat_slot = nullptr;
void** g_cef_browser_create_sync_iat_slot = nullptr;
void** g_cef_browser_view_create_iat_slot = nullptr;
std::vector<CefLoadEndHook> g_model_services_load_end_hooks;
std::vector<CefFrame*> g_model_services_frames;

void set_theme_info(const std::string& message) {
  std::lock_guard<std::mutex> lock(g_info_mutex);
  g_theme_info = message;
}

std::string theme_info() {
  std::lock_guard<std::mutex> lock(g_info_mutex);
  return g_theme_info;
}

std::string lower_ascii(const std::string& value) {
  std::string lowered = value;
  for (char& ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

std::string wide_to_utf8_lossy(const wchar_t* value) {
  if (!value || value[0] == L'\0') {
    return "";
  }

  const int needed = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (needed <= 1) {
    return "";
  }

  std::string out(static_cast<size_t>(needed - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), needed, nullptr, nullptr);
  return out;
}

void add_unique_module(std::vector<HMODULE>& modules, HMODULE module_handle) {
  if (!module_handle) {
    return;
  }
  if (std::find(modules.begin(), modules.end(), module_handle) == modules.end()) {
    modules.push_back(module_handle);
  }
}

std::vector<HMODULE> loaded_modules_matching(const std::vector<std::wstring>& exact_names,
                                             const std::vector<std::string>& keywords) {
  std::vector<HMODULE> modules;
  for (const std::wstring& exact_name : exact_names) {
    add_unique_module(modules, GetModuleHandleW(exact_name.c_str()));
  }

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return modules;
  }

  MODULEENTRY32W module_entry = {};
  module_entry.dwSize = sizeof(module_entry);
  if (Module32FirstW(snapshot, &module_entry)) {
    do {
      const std::string module_name = lower_ascii(wide_to_utf8_lossy(module_entry.szModule));
      bool matches = true;
      for (const std::string& keyword : keywords) {
        if (module_name.find(keyword) == std::string::npos) {
          matches = false;
          break;
        }
      }
      if (matches) {
        add_unique_module(modules, module_entry.hModule);
      }
    } while (Module32NextW(snapshot, &module_entry));
  }
  CloseHandle(snapshot);
  return modules;
}

std::string module_name_for_handle(HMODULE module_handle) {
  wchar_t module_path[MAX_PATH] = {};
  if (!module_handle || GetModuleFileNameW(module_handle, module_path, MAX_PATH) == 0) {
    return "unknown";
  }
  std::filesystem::path path(module_path);
  return path.filename().string();
}

std::string module_name_for_address(const void* address) {
  HMODULE module_handle = nullptr;
  if (!address || !GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(address),
        &module_handle)) {
    return "unknown";
  }
  return module_name_for_handle(module_handle);
}

std::string describe_modules(const std::vector<HMODULE>& modules) {
  if (modules.empty()) {
    return "none";
  }

  std::string description;
  for (HMODULE module_handle : modules) {
    if (!description.empty()) {
      description += ",";
    }
    description += module_name_for_handle(module_handle);
  }
  return description;
}

FARPROC find_export(const std::vector<HMODULE>& modules,
                    const std::vector<const char*>& symbol_names,
                    std::string& resolved_module,
                    std::string& resolved_symbol) {
  for (HMODULE module_handle : modules) {
    for (const char* symbol_name : symbol_names) {
      FARPROC proc = GetProcAddress(module_handle, symbol_name);
      if (proc) {
        resolved_module = module_name_for_handle(module_handle);
        resolved_symbol = symbol_name;
        return proc;
      }
    }
  }
  return nullptr;
}

const char* blender_dark_qt_stylesheet() {
  return R"QSS(
QMainWindow, QDialog, QDockWidget, QToolBar, QMenuBar, QStatusBar, QTabWidget::pane {
  background-color: #2b2b2b;
  color: #d8dbe2;
}
QLabel, QCheckBox, QRadioButton, QGroupBox {
  color: #d8dbe2;
}
QMenu {
  background-color: #303030;
  color: #d8dbe2;
}
QMenu::item:selected, QMenuBar::item:selected {
  background-color: #4772b3;
  color: #ffffff;
}
QPushButton {
  background-color: #3a3a3a;
  color: #e6e8ee;
}
QPushButton:hover {
  background-color: #454545;
}
QPushButton:pressed {
  background-color: #252525;
}
)QSS";
}

const char* tiny_dark_qt_stylesheet() {
  return "QMenu { background-color: #303030; color: #d8dbe2; }\n";
}

const char* active_window_dark_qt_stylesheet() {
  return R"QSS(
QWidget {
  background-color: #2b2b2b;
  color: #d8dbe2;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
}
QMenu, QMenuBar, QToolBar, QStatusBar, QDockWidget, QTabWidget::pane {
  background-color: #303030;
  color: #d8dbe2;
}
QDockWidget::title {
  background-color: #303030;
  color: #d8dbe2;
}
QDockWidget::close-button, QDockWidget::float-button {
  background-color: #303030;
  border-color: #303030;
}
QDockWidget::close-button:hover, QDockWidget::float-button:hover {
  background-color: #454545;
  border-color: #454545;
}
KDDockWidgets--TitleBar, KDDockWidgets--TitleBarWidget,
KDDockWidgets--TitleBar QWidget, KDDockWidgets--TitleBarWidget QWidget,
KDDockWidgets--Frame, KDDockWidgets--FrameWidget,
KDDockWidgets--DockWidget, KDDockWidgets--DockWidgetBase,
KDDockWidgets--FloatingWindow, KDDockWidgets--FloatingWindowWidget,
KDDockWidgets--TabBar, KDDockWidgets--TabBarWidget,
KDDockWidgets--TabWidget, KDDockWidgets--TabWidgetWidget {
  background-color: #303030;
  color: #d8dbe2;
}
KDDockWidgets--Button,
KDDockWidgets--TitleBarButton,
KDDockWidgets--TitleBar QAbstractButton,
KDDockWidgets--TitleBarWidget QAbstractButton {
  background-color: #303030;
  border-color: #303030;
  color: #d8dbe2;
}
KDDockWidgets--Button:hover,
KDDockWidgets--TitleBarButton:hover,
KDDockWidgets--TitleBar QAbstractButton:hover,
KDDockWidgets--TitleBarWidget QAbstractButton:hover {
  background-color: #454545;
  border-color: #454545;
}
QLabel, QCheckBox, QRadioButton, QGroupBox {
  color: #d8dbe2;
}
QPushButton {
  background-color: #3a3a3a;
  color: #e6e8ee;
}
QPushButton:hover {
  background-color: #454545;
}
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
  background-color: #1f1f1f;
  color: #e6e8ee;
}
QTreeView, QListView, QTableView, QAbstractItemView {
  background-color: #242424;
  alternate-background-color: #303030;
  color: #d8dbe2;
}
QHeaderView::section {
  background-color: #333333;
  color: #d8dbe2;
}
)QSS";
}

const char* clicked_child_force_dark_qt_stylesheet() {
  return R"QSS(
* {
  background-color: #202020;
  color: #e6e8ee;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
}
QFrame, QGroupBox, QAbstractScrollArea, QScrollArea, QAbstractItemView {
  background-color: #202020;
  color: #e6e8ee;
}
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
  background-color: #181818;
  color: #f0f2f7;
  border: 1px solid #54575f;
}
QHeaderView::section {
  background-color: #303030;
  color: #e6e8ee;
}
)QSS";
}

const char* clicked_delegate_force_dark_qt_stylesheet() {
  return R"QSS(
* {
  background-color: #202020;
  color: #e8ebf2;
  alternate-background-color: #282828;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
  border-color: #50535a;
}
QWidget, QFrame, QGroupBox, QStackedWidget,
QAbstractScrollArea, QScrollArea, QAbstractScrollArea > QWidget, QAbstractScrollArea > QWidget > QWidget {
  background-color: #202020;
  color: #e8ebf2;
}
QAbstractItemView, QTreeView, QListView, QTableView, QColumnView {
  background-color: #202020;
  alternate-background-color: #282828;
  color: #e8ebf2;
  border: 1px solid #50535a;
  show-decoration-selected: 1;
}
QAbstractItemView::item, QTreeView::item, QListView::item, QTableView::item {
  background-color: #202020;
  color: #e8ebf2;
}
QAbstractItemView::item:hover, QTreeView::item:hover, QListView::item:hover, QTableView::item:hover {
  background-color: #30343b;
  color: #ffffff;
}
QAbstractItemView::item:selected, QTreeView::item:selected, QListView::item:selected, QTableView::item:selected {
  background-color: #4772b3;
  color: #ffffff;
}
QTreeView::branch, QTreeView::branch:open, QTreeView::branch:closed, QTreeView::branch:has-children {
  background-color: #202020;
  border-image: none;
  image: none;
}
QHeaderView, QHeaderView::section, QTableCornerButton::section {
  background-color: #303030;
  color: #e8ebf2;
  border: 1px solid #50535a;
}
QToolButton, QToolButton:checked, QToolButton:pressed, QToolButton:hover,
QPushButton, QComboBox, QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
  background-color: #181818;
  color: #f0f2f7;
  border: 1px solid #50535a;
}
QDockWidget, QDockWidget::title, QTabWidget::pane, QTabBar::tab,
KDDockWidgets--TitleBar, KDDockWidgets--TitleBarWidget,
KDDockWidgets--Frame, KDDockWidgets--FrameWidget,
KDDockWidgets--DockWidget, KDDockWidgets--DockWidgetBase,
KDDockWidgets--FloatingWindow, KDDockWidgets--FloatingWindowWidget,
KDDockWidgets--TabBar, KDDockWidgets--TabBarWidget,
KDDockWidgets--TabWidget, KDDockWidgets--TabWidgetWidget {
  background-color: #202020;
  color: #e8ebf2;
}
)QSS";
}

const char* separator_splitter_dark_qt_stylesheet() {
  return R"QSS(
/* ?????? Splitters & Separators ?????? */
QSplitter::handle {
  background-color: #3a3a3a;
  border: none;
}
QSplitter::handle:horizontal {
  background-color: #3a3a3a;
  border: none;
}
QSplitter::handle:vertical {
  background-color: #3a3a3a;
  border: none;
}
QMenu::separator {
  background-color: #3a3a3a;
  height: 1px;
  margin: 4px 8px;
}
QToolBar::separator {
  background-color: #3a3a3a;
  width: 1px;
  margin: 2px 4px;
}
QFrame[frameShape="4"], QFrame[frameShape="5"] {
  color: #3a3a3a;
  background-color: #3a3a3a;
  border: none;
  max-height: 1px;
  max-width: 1px;
}
QHeaderView {
  background-color: #2b2b2b;
  color: #d8dbe2;
}
QHeaderView::section {
  background-color: #2b2b2b;
  color: #d8dbe2;
  border: none;
  border-right: 1px solid #3a3a3a;
  border-bottom: 1px solid #3a3a3a;
}

/* ?????? Sliders ?????? */
QSlider::groove:horizontal {
  background-color: #3a3a3a;
  height: 4px;
  border-radius: 2px;
}
QSlider::handle:horizontal {
  background-color: #5a5a5a;
  width: 14px;
  height: 14px;
  margin: -5px 0;
  border-radius: 7px;
}
QSlider::handle:horizontal:hover {
  background-color: #6a6a6a;
}
QSlider::groove:vertical {
  background-color: #3a3a3a;
  width: 4px;
  border-radius: 2px;
}
QSlider::handle:vertical {
  background-color: #5a5a5a;
  width: 14px;
  height: 14px;
  margin: 0 -5px;
  border-radius: 7px;
}

/* ?????? Progress Bars ?????? */
QProgressBar {
  background-color: #2b2b2b;
  border: 1px solid #3a3a3a;
  border-radius: 3px;
  text-align: center;
  color: #d8dbe2;
}
QProgressBar::chunk {
  background-color: #4772b3;
  border-radius: 2px;
}

/* ?????? Tab Bars ?????? */
QTabBar::tab {
  background-color: #2b2b2b;
  color: #999999;
  border: 1px solid #3a3a3a;
  border-bottom: none;
  padding: 4px 12px;
  margin-right: 2px;
}
QTabBar::tab:selected {
  background-color: #242424;
  color: #d8dbe2;
  border-color: #3a3a3a;
}
QTabBar::tab:hover:!selected {
  background-color: #333333;
  color: #cccccc;
}
QTabBar::tab:disabled {
  color: #555555;
}
QTabWidget::pane {
  border: 1px solid #3a3a3a;
  background-color: #242424;
}

/* ?????? Menu Bar ?????? */
QMenuBar {
  background-color: #303030;
  color: #d8dbe2;
  border-bottom: 1px solid #3a3a3a;
}
QMenuBar::item {
  background-color: transparent;
  color: #d8dbe2;
  padding: 2px 5px;
  margin: 0;
}
QMenuBar::item:selected {
  background-color: #3a3a3a;
}
QMenuBar::item:pressed {
  background-color: #2a2a2a;
}

/* ?????? Menus ?????? */
QMenu {
  background-color: #2b2b2b;
  color: #d8dbe2;
  border: 1px solid #3a3a3a;
  padding: 2px 0;
}
QMenu::item:selected {
  background-color: #4772b3;
  color: #ffffff;
}
QMenu::item:disabled {
  color: #555555;
}

/* ?????? Status Bar ?????? */
QStatusBar {
  background-color: #303030;
  color: #999999;
  border-top: 1px solid #3a3a3a;
}
QStatusBar::item {
  border: none;
}

/* ?????? Tool Bar ?????? */
QToolBar {
  background-color: #2b2b2b;
  border: none;
  spacing: 2px;
  padding: 2px;
}
QToolBar::handle {
  background-color: #2b2b2b;
  border: none;
}

/* ?????? Dock / tray title chrome ?????? */
QDockWidget {
  background-color: #2b2b2b;
  color: #d8dbe2;
  border: 1px solid #303030;
}
QDockWidget::title {
  background-color: #303030;
  color: #d8dbe2;
  border: 1px solid #303030;
}
QDockWidget::close-button, QDockWidget::float-button {
  background-color: #303030;
  border-color: #303030;
}
QDockWidget::close-button:hover, QDockWidget::float-button:hover {
  background-color: #454545;
  border-color: #454545;
}

/* ?????? Size Grip ?????? */
QSizeGrip {
  background-color: transparent;
}

/* ?????? Rubber Band (selection) ?????? */
QRubberBand {
  background-color: rgba(71, 114, 179, 60);
  border: 1px solid #4772b3;
}

/* ?????? Dialogs & Windows ?????? */
QDialog, QWizard, QWizardPage, QMessageBox,
QInputDialog, QFileDialog, QPrintDialog {
  background-color: #2b2b2b;
  color: #d8dbe2;
}
QDialogButtonBox {
  background-color: #2b2b2b;
}

/* ?????? Combo Box popup ?????? */
QComboBox QAbstractItemView {
  background-color: #242424;
  color: #d8dbe2;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
  border: 1px solid #3a3a3a;
}

/* ?????? Spin Box buttons ?????? */
QAbstractSpinBox::up-button {
  background-color: #3a3a3a;
  border-left: 1px solid #444444;
}
QAbstractSpinBox::down-button {
  background-color: #3a3a3a;
  border-left: 1px solid #444444;
}
QAbstractSpinBox::up-button:hover,
QAbstractSpinBox::down-button:hover {
  background-color: #454545;
}
QAbstractSpinBox::up-button:pressed,
QAbstractSpinBox::down-button:pressed {
  background-color: #2a2a2a;
}

/* ?????? KDDockWidgets extras ?????? */
KDDockWidgets--DropIndicatorOverlay {
  background-color: #4772b3;
}
KDDockWidgets--Frame,
KDDockWidgets--FrameWidget,
KDDockWidgets--DockWidget,
KDDockWidgets--DockWidgetBase,
KDDockWidgets--TitleBar,
KDDockWidgets--TitleBarWidget {
  background-color: #303030;
  color: #d8dbe2;
  border: 1px solid #303030;
}
KDDockWidgets--TitleBar QFrame,
KDDockWidgets--TitleBarWidget QFrame {
  background-color: #303030;
  border: 1px solid #303030;
}
KDDockWidgets--TitleBar QLabel,
KDDockWidgets--TitleBarWidget QLabel {
  background-color: transparent;
  color: #d8dbe2;
  border: none;
}
KDDockWidgets--TitleBarButton,
KDDockWidgets--TitleBar QAbstractButton,
KDDockWidgets--TitleBarWidget QAbstractButton {
  background-color: #303030;
  border-color: #303030;
  color: #d8dbe2;
}
KDDockWidgets--TitleBarButton:hover,
KDDockWidgets--TitleBar QAbstractButton:hover,
KDDockWidgets--TitleBarWidget QAbstractButton:hover {
  background-color: #454545;
  border-color: #454545;
}
KDDockWidgets--Separator {
  background-color: #3a3a3a;
}
KDDockWidgets--SideBar {
  background-color: #303030;
  color: #d8dbe2;
}

/* ?????? Group Box ?????? */
QGroupBox {
  color: #d8dbe2;
  border: 1px solid #3a3a3a;
  border-radius: 3px;
  margin-top: 8px;
  padding-top: 8px;
}
QGroupBox::title {
  color: #d8dbe2;
  padding: 0 4px;
}

/* ?????? Disabled global ?????? */
*:disabled {
  color: #666666;
}

/* ?????? Focus rectangle ?????? */
*:focus {
  outline: none;
}
)QSS";
}

std::string pointer_hex(const void* pointer) {
  char buffer[32] = {};
  std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(pointer)));
  return buffer;
}

std::string hwnd_hex(HWND hwnd) {
  return pointer_hex(reinterpret_cast<const void*>(hwnd));
}

struct NativeWindowCandidate {
  HWND hwnd = nullptr;
  std::string title;
  std::string class_name;
  long long area = 0;
};

std::string hwnd_text(HWND hwnd) {
  wchar_t text[512] = {};
  const int length = GetWindowTextW(hwnd, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
  return length > 0 ? wide_to_utf8_lossy(text) : "";
}

std::string hwnd_class_name(HWND hwnd) {
  wchar_t class_name[256] = {};
  const int length = GetClassNameW(hwnd, class_name, static_cast<int>(sizeof(class_name) / sizeof(class_name[0])));
  return length > 0 ? wide_to_utf8_lossy(class_name) : "";
}

BOOL CALLBACK collect_process_windows_proc(HWND hwnd, LPARAM lparam) {
  auto* candidates = reinterpret_cast<std::vector<NativeWindowCandidate>*>(lparam);
  if (!candidates || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) {
    return TRUE;
  }

  DWORD window_pid = 0;
  GetWindowThreadProcessId(hwnd, &window_pid);
  if (window_pid != GetCurrentProcessId()) {
    return TRUE;
  }

  RECT rect = {};
  if (!GetWindowRect(hwnd, &rect)) {
    return TRUE;
  }

  const long long width = static_cast<long long>(std::max<LONG>(0, rect.right - rect.left));
  const long long height = static_cast<long long>(std::max<LONG>(0, rect.bottom - rect.top));
  const long long area = width * height;
  if (area <= 0) {
    return TRUE;
  }

  candidates->push_back({hwnd, hwnd_text(hwnd), hwnd_class_name(hwnd), area});
  return TRUE;
}

std::vector<NativeWindowCandidate> process_top_level_windows() {
  std::vector<NativeWindowCandidate> candidates;
  EnumWindows(collect_process_windows_proc, reinterpret_cast<LPARAM>(&candidates));
  std::sort(candidates.begin(), candidates.end(), [](const NativeWindowCandidate& left, const NativeWindowCandidate& right) {
    const bool left_sketchup = lower_ascii(left.title).find("sketchup") != std::string::npos;
    const bool right_sketchup = lower_ascii(right.title).find("sketchup") != std::string::npos;
    if (left_sketchup != right_sketchup) {
      return left_sketchup && !right_sketchup;
    }
    return left.area > right.area;
  });
  return candidates;
}

std::string describe_native_window(const NativeWindowCandidate& candidate) {
  return "hwnd=" + hwnd_hex(candidate.hwnd) +
    " area=" + std::to_string(candidate.area) +
    " class=" + candidate.class_name +
    " title=" + candidate.title;
}

const char* noop_qt_stylesheet() {
  return "/* dark mode qss parser check */\n";
}

struct QtLookup {
  std::vector<HMODULE> core_modules;
  std::vector<HMODULE> gui_modules;
  std::vector<HMODULE> widgets_modules;
  FARPROC instance_proc = nullptr;
  FARPROC process_events_proc = nullptr;
  FARPROC send_posted_events_proc = nullptr;
  FARPROC set_stylesheet_proc = nullptr;
  FARPROC from_utf8_proc = nullptr;
  FARPROC object_inherits_proc = nullptr;
  FARPROC object_meta_object_proc = nullptr;
  FARPROC meta_object_class_name_proc = nullptr;
  FARPROC active_window_proc = nullptr;
  FARPROC app_focus_widget_proc = nullptr;
  FARPROC widget_find_proc = nullptr;
  FARPROC widget_set_stylesheet_proc = nullptr;
  FARPROC widget_stylesheet_proc = nullptr;
  FARPROC widget_style_proc = nullptr;
  FARPROC widget_set_attribute_proc = nullptr;
  FARPROC widget_set_auto_fill_background_proc = nullptr;
  FARPROC widget_child_at_proc = nullptr;
  FARPROC widget_palette_proc = nullptr;
  FARPROC widget_set_palette_proc = nullptr;
  FARPROC widget_set_background_role_proc = nullptr;
  FARPROC widget_set_foreground_role_proc = nullptr;
  FARPROC abstract_item_view_item_delegate_proc = nullptr;
  FARPROC abstract_scroll_area_viewport_proc = nullptr;
  FARPROC app_palette_class_proc = nullptr;
  FARPROC app_palette_widget_proc = nullptr;
  FARPROC app_set_palette_proc = nullptr;
  FARPROC qpalette_copy_ctor_proc = nullptr;
  FARPROC qpalette_destructor_proc = nullptr;
  FARPROC qpalette_color_role_proc = nullptr;
  FARPROC qpalette_color_group_role_proc = nullptr;
  FARPROC qpalette_set_color_group_proc = nullptr;
  FARPROC qpalette_set_color_role_proc = nullptr;
  FARPROC qcolor_rgb_ctor_proc = nullptr;
  FARPROC qcolor_rgb_proc = nullptr;
  FARPROC qcolor_set_rgb_proc = nullptr;
  FARPROC qcolor_rgba_proc = nullptr;
  FARPROC qpainter_save_proc = nullptr;
  FARPROC qpainter_restore_proc = nullptr;
  FARPROC qpainter_set_pen_color_proc = nullptr;
  FARPROC qbrush_copy_ctor_proc = nullptr;
  FARPROC qbrush_assign_proc = nullptr;
  FARPROC qbrush_destructor_proc = nullptr;
  FARPROC qbrush_color_proc = nullptr;
  FARPROC qbrush_set_color_proc = nullptr;
  FARPROC widget_update_proc = nullptr;
  FARPROC widget_repaint_proc = nullptr;
  FARPROC app_all_widgets_proc = nullptr;
  FARPROC app_top_level_widgets_proc = nullptr;
  FARPROC object_object_name_proc = nullptr;
  FARPROC object_parent_proc = nullptr;
  FARPROC object_children_proc = nullptr;
  FARPROC qstring_to_utf8_proc = nullptr;
  FARPROC qstring_destructor_proc = nullptr;
  FARPROC qbytearray_const_data_proc = nullptr;
  FARPROC qbytearray_size_proc = nullptr;
  FARPROC qbytearray_destructor_proc = nullptr;
  FARPROC widget_geometry_proc = nullptr;
  FARPROC widget_rect_proc = nullptr;
  FARPROC widget_is_visible_proc = nullptr;
  FARPROC widget_is_window_proc = nullptr;
  FARPROC widget_parent_widget_proc = nullptr;
  FARPROC widget_window_title_proc = nullptr;
  std::string instance_module;
  std::string gui_module_description;
  std::string instance_symbol;
  std::string process_events_module;
  std::string process_events_symbol;
  std::string send_posted_events_module;
  std::string send_posted_events_symbol;
  std::string set_stylesheet_module;
  std::string set_stylesheet_symbol;
  std::string from_utf8_module;
  std::string from_utf8_symbol;
  std::string object_inherits_module;
  std::string object_inherits_symbol;
  std::string object_meta_object_module;
  std::string object_meta_object_symbol;
  std::string meta_object_class_name_module;
  std::string meta_object_class_name_symbol;
  std::string active_window_module;
  std::string active_window_symbol;
  std::string app_focus_widget_module;
  std::string app_focus_widget_symbol;
  std::string widget_find_module;
  std::string widget_find_symbol;
  std::string widget_set_stylesheet_module;
  std::string widget_set_stylesheet_symbol;
  std::string widget_stylesheet_module;
  std::string widget_stylesheet_symbol;
  std::string widget_style_module;
  std::string widget_style_symbol;
  std::string widget_set_attribute_module;
  std::string widget_set_attribute_symbol;
  std::string widget_set_auto_fill_background_module;
  std::string widget_set_auto_fill_background_symbol;
  std::string widget_child_at_module;
  std::string widget_child_at_symbol;
  std::string widget_palette_module;
  std::string widget_palette_symbol;
  std::string widget_set_palette_module;
  std::string widget_set_palette_symbol;
  std::string widget_set_background_role_module;
  std::string widget_set_background_role_symbol;
  std::string widget_set_foreground_role_module;
  std::string widget_set_foreground_role_symbol;
  std::string abstract_item_view_item_delegate_module;
  std::string abstract_item_view_item_delegate_symbol;
  std::string abstract_scroll_area_viewport_module;
  std::string abstract_scroll_area_viewport_symbol;
  std::string app_palette_class_module;
  std::string app_palette_class_symbol;
  std::string app_palette_widget_module;
  std::string app_palette_widget_symbol;
  std::string app_set_palette_module;
  std::string app_set_palette_symbol;
  std::string qpalette_copy_ctor_module;
  std::string qpalette_copy_ctor_symbol;
  std::string qpalette_destructor_module;
  std::string qpalette_destructor_symbol;
  std::string qpalette_color_role_module;
  std::string qpalette_color_role_symbol;
  std::string qpalette_color_group_role_module;
  std::string qpalette_color_group_role_symbol;
  std::string qpalette_set_color_group_module;
  std::string qpalette_set_color_group_symbol;
  std::string qpalette_set_color_role_module;
  std::string qpalette_set_color_role_symbol;
  std::string qcolor_rgb_ctor_module;
  std::string qcolor_rgb_ctor_symbol;
  std::string qcolor_rgb_module;
  std::string qcolor_rgb_symbol;
  std::string qcolor_set_rgb_module;
  std::string qcolor_set_rgb_symbol;
  std::string qcolor_rgba_module;
  std::string qcolor_rgba_symbol;
  std::string qpainter_save_module;
  std::string qpainter_save_symbol;
  std::string qpainter_restore_module;
  std::string qpainter_restore_symbol;
  std::string qpainter_set_pen_color_module;
  std::string qpainter_set_pen_color_symbol;
  std::string qbrush_copy_ctor_module;
  std::string qbrush_copy_ctor_symbol;
  std::string qbrush_assign_module;
  std::string qbrush_assign_symbol;
  std::string qbrush_destructor_module;
  std::string qbrush_destructor_symbol;
  std::string qbrush_color_module;
  std::string qbrush_color_symbol;
  std::string qbrush_set_color_module;
  std::string qbrush_set_color_symbol;
  std::string widget_update_module;
  std::string widget_update_symbol;
  std::string widget_repaint_module;
  std::string widget_repaint_symbol;
  std::string app_all_widgets_module;
  std::string app_all_widgets_symbol;
  std::string app_top_level_widgets_module;
  std::string app_top_level_widgets_symbol;
  std::string object_object_name_module;
  std::string object_object_name_symbol;
  std::string object_parent_module;
  std::string object_parent_symbol;
  std::string object_children_module;
  std::string object_children_symbol;
  std::string qstring_to_utf8_module;
  std::string qstring_to_utf8_symbol;
  std::string qstring_destructor_module;
  std::string qstring_destructor_symbol;
  std::string qbytearray_const_data_module;
  std::string qbytearray_const_data_symbol;
  std::string qbytearray_size_module;
  std::string qbytearray_size_symbol;
  std::string qbytearray_destructor_module;
  std::string qbytearray_destructor_symbol;
  std::string widget_geometry_module;
  std::string widget_geometry_symbol;
  std::string widget_rect_module;
  std::string widget_rect_symbol;
  std::string widget_is_visible_module;
  std::string widget_is_visible_symbol;
  std::string widget_is_window_module;
  std::string widget_is_window_symbol;
  std::string widget_parent_widget_module;
  std::string widget_parent_widget_symbol;
  std::string widget_window_title_module;
  std::string widget_window_title_symbol;
};

QtLookup resolve_qt_lookup() {
  QtLookup lookup;
  lookup.core_modules = loaded_modules_matching(
    {L"Qt6Core.dll", L"Qt5Core.dll"},
    {"qt", "core"}
  );
  lookup.gui_modules = loaded_modules_matching(
    {L"Qt6Gui.dll", L"Qt5Gui.dll"},
    {"qt", "gui"}
  );
  lookup.widgets_modules = loaded_modules_matching(
    {L"Qt6Widgets.dll", L"Qt5Widgets.dll"},
    {"qt", "widgets"}
  );
  lookup.gui_module_description = describe_modules(lookup.gui_modules);

  lookup.instance_proc = find_export(lookup.core_modules, {
    "?instance@QCoreApplication@@SAPEAV1@XZ"
  }, lookup.instance_module, lookup.instance_symbol);

  lookup.process_events_proc = find_export(lookup.core_modules, {
    "?processEvents@QCoreApplication@@SAXV?$QFlags@W4ProcessEventsFlag@QEventLoop@@@@@Z"
  }, lookup.process_events_module, lookup.process_events_symbol);

  lookup.send_posted_events_proc = find_export(lookup.core_modules, {
    "?sendPostedEvents@QCoreApplication@@SAXPEAVQObject@@H@Z"
  }, lookup.send_posted_events_module, lookup.send_posted_events_symbol);

  lookup.set_stylesheet_proc = find_export(lookup.widgets_modules, {
    "?setStyleSheet@QApplication@@QEAAXAEBVQString@@@Z"
  }, lookup.set_stylesheet_module, lookup.set_stylesheet_symbol);

  lookup.from_utf8_proc = find_export(lookup.core_modules, {
    "?fromUtf8@QString@@SA?AV1@PEBD_J@Z",
    "?fromUtf8@QString@@SA?AV1@PEBDH@Z"
  }, lookup.from_utf8_module, lookup.from_utf8_symbol);

  lookup.object_inherits_proc = find_export(lookup.core_modules, {
    "?inherits@QObject@@QEBA_NPEBD@Z"
  }, lookup.object_inherits_module, lookup.object_inherits_symbol);

  lookup.object_meta_object_proc = find_export(lookup.core_modules, {
    "?metaObject@QObject@@UEBAPEBVQMetaObject@@XZ",
    "?metaObject@QObject@@UEBAPEBUQMetaObject@@XZ"
  }, lookup.object_meta_object_module, lookup.object_meta_object_symbol);

  lookup.meta_object_class_name_proc = find_export(lookup.core_modules, {
    "?className@QMetaObject@@QEBAPEBDXZ"
  }, lookup.meta_object_class_name_module, lookup.meta_object_class_name_symbol);

  lookup.active_window_proc = find_export(lookup.widgets_modules, {
    "?activeWindow@QApplication@@SAPEAVQWidget@@XZ"
  }, lookup.active_window_module, lookup.active_window_symbol);

  lookup.app_focus_widget_proc = find_export(lookup.widgets_modules, {
    "?focusWidget@QApplication@@SAPEAVQWidget@@XZ"
  }, lookup.app_focus_widget_module, lookup.app_focus_widget_symbol);

  lookup.widget_find_proc = find_export(lookup.widgets_modules, {
    "?find@QWidget@@SAPEAV1@_K@Z"
  }, lookup.widget_find_module, lookup.widget_find_symbol);

  lookup.widget_set_stylesheet_proc = find_export(lookup.widgets_modules, {
    "?setStyleSheet@QWidget@@QEAAXAEBVQString@@@Z"
  }, lookup.widget_set_stylesheet_module, lookup.widget_set_stylesheet_symbol);

  lookup.widget_stylesheet_proc = find_export(lookup.widgets_modules, {
    "?styleSheet@QWidget@@QEBA?AVQString@@XZ"
  }, lookup.widget_stylesheet_module, lookup.widget_stylesheet_symbol);

  lookup.widget_style_proc = find_export(lookup.widgets_modules, {
    "?style@QWidget@@QEBAPEAVQStyle@@XZ"
  }, lookup.widget_style_module, lookup.widget_style_symbol);

  lookup.widget_set_attribute_proc = find_export(lookup.widgets_modules, {
    "?setAttribute@QWidget@@QEAAXW4WidgetAttribute@Qt@@_N@Z"
  }, lookup.widget_set_attribute_module, lookup.widget_set_attribute_symbol);

  lookup.widget_set_auto_fill_background_proc = find_export(lookup.widgets_modules, {
    "?setAutoFillBackground@QWidget@@QEAAX_N@Z"
  }, lookup.widget_set_auto_fill_background_module, lookup.widget_set_auto_fill_background_symbol);

  lookup.widget_child_at_proc = find_export(lookup.widgets_modules, {
    "?childAt@QWidget@@QEBAPEAV1@HH@Z"
  }, lookup.widget_child_at_module, lookup.widget_child_at_symbol);

  lookup.widget_palette_proc = find_export(lookup.widgets_modules, {
    "?palette@QWidget@@QEBAAEBVQPalette@@XZ"
  }, lookup.widget_palette_module, lookup.widget_palette_symbol);

  lookup.widget_set_palette_proc = find_export(lookup.widgets_modules, {
    "?setPalette@QWidget@@QEAAXAEBVQPalette@@@Z"
  }, lookup.widget_set_palette_module, lookup.widget_set_palette_symbol);

  lookup.widget_set_background_role_proc = find_export(lookup.widgets_modules, {
    "?setBackgroundRole@QWidget@@QEAAXW4ColorRole@QPalette@@@Z"
  }, lookup.widget_set_background_role_module, lookup.widget_set_background_role_symbol);

  lookup.widget_set_foreground_role_proc = find_export(lookup.widgets_modules, {
    "?setForegroundRole@QWidget@@QEAAXW4ColorRole@QPalette@@@Z"
  }, lookup.widget_set_foreground_role_module, lookup.widget_set_foreground_role_symbol);

  lookup.abstract_item_view_item_delegate_proc = find_export(lookup.widgets_modules, {
    "?itemDelegate@QAbstractItemView@@QEBAPEAVQAbstractItemDelegate@@XZ"
  }, lookup.abstract_item_view_item_delegate_module, lookup.abstract_item_view_item_delegate_symbol);

  lookup.abstract_scroll_area_viewport_proc = find_export(lookup.widgets_modules, {
    "?viewport@QAbstractScrollArea@@QEBAPEAVQWidget@@XZ"
  }, lookup.abstract_scroll_area_viewport_module, lookup.abstract_scroll_area_viewport_symbol);

  lookup.app_palette_class_proc = find_export(lookup.widgets_modules, {
    "?palette@QApplication@@SA?AVQPalette@@PEBD@Z"
  }, lookup.app_palette_class_module, lookup.app_palette_class_symbol);

  lookup.app_palette_widget_proc = find_export(lookup.widgets_modules, {
    "?palette@QApplication@@SA?AVQPalette@@PEBVQWidget@@@Z"
  }, lookup.app_palette_widget_module, lookup.app_palette_widget_symbol);

  lookup.app_set_palette_proc = find_export(lookup.widgets_modules, {
    "?setPalette@QApplication@@SAXAEBVQPalette@@PEBD@Z"
  }, lookup.app_set_palette_module, lookup.app_set_palette_symbol);

  lookup.qpalette_copy_ctor_proc = find_export(lookup.gui_modules, {
    "??0QPalette@@QEAA@AEBV0@@Z"
  }, lookup.qpalette_copy_ctor_module, lookup.qpalette_copy_ctor_symbol);

  lookup.qpalette_destructor_proc = find_export(lookup.gui_modules, {
    "??1QPalette@@QEAA@XZ"
  }, lookup.qpalette_destructor_module, lookup.qpalette_destructor_symbol);

  lookup.qpalette_color_role_proc = find_export(lookup.gui_modules, {
    "?color@QPalette@@QEBA?AVQColor@@W4ColorRole@1@@Z"
  }, lookup.qpalette_color_role_module, lookup.qpalette_color_role_symbol);

  lookup.qpalette_color_group_role_proc = find_export(lookup.gui_modules, {
    "?color@QPalette@@QEBAAEBVQColor@@W4ColorGroup@1@W4ColorRole@1@@Z"
  }, lookup.qpalette_color_group_role_module, lookup.qpalette_color_group_role_symbol);

  lookup.qpalette_set_color_group_proc = find_export(lookup.gui_modules, {
    "?setColor@QPalette@@QEAAXW4ColorGroup@1@W4ColorRole@1@AEBVQColor@@@Z"
  }, lookup.qpalette_set_color_group_module, lookup.qpalette_set_color_group_symbol);

  lookup.qpalette_set_color_role_proc = find_export(lookup.gui_modules, {
    "?setColor@QPalette@@QEAAXW4ColorRole@1@AEBVQColor@@@Z"
  }, lookup.qpalette_set_color_role_module, lookup.qpalette_set_color_role_symbol);

  lookup.qcolor_rgb_ctor_proc = find_export(lookup.gui_modules, {
    "??0QColor@@QEAA@HHHH@Z"
  }, lookup.qcolor_rgb_ctor_module, lookup.qcolor_rgb_ctor_symbol);

  lookup.qcolor_rgb_proc = find_export(lookup.gui_modules, {
    "?rgb@QColor@@QEBAIXZ"
  }, lookup.qcolor_rgb_module, lookup.qcolor_rgb_symbol);

  lookup.qcolor_set_rgb_proc = find_export(lookup.gui_modules, {
    "?setRgb@QColor@@QEAAXHHHH@Z"
  }, lookup.qcolor_set_rgb_module, lookup.qcolor_set_rgb_symbol);

  lookup.qcolor_rgba_proc = find_export(lookup.gui_modules, {
    "?rgba@QColor@@QEBAIXZ"
  }, lookup.qcolor_rgba_module, lookup.qcolor_rgba_symbol);

  lookup.qpainter_save_proc = find_export(lookup.gui_modules, {
    "?save@QPainter@@QEAAXXZ"
  }, lookup.qpainter_save_module, lookup.qpainter_save_symbol);

  lookup.qpainter_restore_proc = find_export(lookup.gui_modules, {
    "?restore@QPainter@@QEAAXXZ"
  }, lookup.qpainter_restore_module, lookup.qpainter_restore_symbol);

  lookup.qpainter_set_pen_color_proc = find_export(lookup.gui_modules, {
    "?setPen@QPainter@@QEAAXAEBVQColor@@@Z"
  }, lookup.qpainter_set_pen_color_module, lookup.qpainter_set_pen_color_symbol);

  lookup.qbrush_copy_ctor_proc = find_export(lookup.gui_modules, {
    "??0QBrush@@QEAA@AEBV0@@Z"
  }, lookup.qbrush_copy_ctor_module, lookup.qbrush_copy_ctor_symbol);

  lookup.qbrush_assign_proc = find_export(lookup.gui_modules, {
    "??4QBrush@@QEAAAEAV0@AEBV0@@Z"
  }, lookup.qbrush_assign_module, lookup.qbrush_assign_symbol);

  lookup.qbrush_destructor_proc = find_export(lookup.gui_modules, {
    "??1QBrush@@QEAA@XZ"
  }, lookup.qbrush_destructor_module, lookup.qbrush_destructor_symbol);

  lookup.qbrush_color_proc = find_export(lookup.gui_modules, {
    "?color@QBrush@@QEBAAEBVQColor@@XZ"
  }, lookup.qbrush_color_module, lookup.qbrush_color_symbol);

  lookup.qbrush_set_color_proc = find_export(lookup.gui_modules, {
    "?setColor@QBrush@@QEAAXAEBVQColor@@@Z"
  }, lookup.qbrush_set_color_module, lookup.qbrush_set_color_symbol);

  lookup.widget_update_proc = find_export(lookup.widgets_modules, {
    "?update@QWidget@@QEAAXXZ"
  }, lookup.widget_update_module, lookup.widget_update_symbol);

  lookup.widget_repaint_proc = find_export(lookup.widgets_modules, {
    "?repaint@QWidget@@QEAAXXZ"
  }, lookup.widget_repaint_module, lookup.widget_repaint_symbol);

  lookup.app_all_widgets_proc = find_export(lookup.widgets_modules, {
    "?allWidgets@QApplication@@SA?AV?$QList@PEAVQWidget@@@@XZ"
  }, lookup.app_all_widgets_module, lookup.app_all_widgets_symbol);

  lookup.app_top_level_widgets_proc = find_export(lookup.widgets_modules, {
    "?topLevelWidgets@QApplication@@SA?AV?$QList@PEAVQWidget@@@@XZ"
  }, lookup.app_top_level_widgets_module, lookup.app_top_level_widgets_symbol);

  lookup.object_object_name_proc = find_export(lookup.core_modules, {
    "?objectName@QObject@@QEBA?AVQString@@XZ"
  }, lookup.object_object_name_module, lookup.object_object_name_symbol);

  lookup.object_parent_proc = find_export(lookup.core_modules, {
    "?parent@QObject@@QEBAPEAV1@XZ"
  }, lookup.object_parent_module, lookup.object_parent_symbol);

  lookup.object_children_proc = find_export(lookup.core_modules, {
    "?children@QObject@@QEBAAEBV?$QList@PEAVQObject@@@@XZ"
  }, lookup.object_children_module, lookup.object_children_symbol);

  lookup.qstring_to_utf8_proc = find_export(lookup.core_modules, {
    "?toUtf8@QString@@QEGBA?AVQByteArray@@XZ",
    "?toUtf8@QString@@QEHAA?AVQByteArray@@XZ"
  }, lookup.qstring_to_utf8_module, lookup.qstring_to_utf8_symbol);

  lookup.qstring_destructor_proc = find_export(lookup.core_modules, {
    "??1QString@@QEAA@XZ"
  }, lookup.qstring_destructor_module, lookup.qstring_destructor_symbol);

  lookup.qbytearray_const_data_proc = find_export(lookup.core_modules, {
    "?constData@QByteArray@@QEBAPEBDXZ"
  }, lookup.qbytearray_const_data_module, lookup.qbytearray_const_data_symbol);

  lookup.qbytearray_size_proc = find_export(lookup.core_modules, {
    "?size@QByteArray@@QEBA_JXZ"
  }, lookup.qbytearray_size_module, lookup.qbytearray_size_symbol);

  lookup.qbytearray_destructor_proc = find_export(lookup.core_modules, {
    "??1QByteArray@@QEAA@XZ"
  }, lookup.qbytearray_destructor_module, lookup.qbytearray_destructor_symbol);

  lookup.widget_geometry_proc = find_export(lookup.widgets_modules, {
    "?geometry@QWidget@@QEBAAEBVQRect@@XZ"
  }, lookup.widget_geometry_module, lookup.widget_geometry_symbol);

  lookup.widget_rect_proc = find_export(lookup.widgets_modules, {
    "?rect@QWidget@@QEBA?AVQRect@@XZ"
  }, lookup.widget_rect_module, lookup.widget_rect_symbol);

  lookup.widget_is_visible_proc = find_export(lookup.widgets_modules, {
    "?isVisible@QWidget@@QEBA_NXZ"
  }, lookup.widget_is_visible_module, lookup.widget_is_visible_symbol);

  lookup.widget_is_window_proc = find_export(lookup.widgets_modules, {
    "?isWindow@QWidget@@QEBA_NXZ"
  }, lookup.widget_is_window_module, lookup.widget_is_window_symbol);

  lookup.widget_parent_widget_proc = find_export(lookup.widgets_modules, {
    "?parentWidget@QWidget@@QEBAPEAV1@XZ"
  }, lookup.widget_parent_widget_module, lookup.widget_parent_widget_symbol);

  lookup.widget_window_title_proc = find_export(lookup.widgets_modules, {
    "?windowTitle@QWidget@@QEBA?AVQString@@XZ"
  }, lookup.widget_window_title_module, lookup.widget_window_title_symbol);

  return lookup;
}

std::string describe_qt_lookup(const QtLookup& lookup) {
  return "core=" + describe_modules(lookup.core_modules) +
    " gui=" + describe_modules(lookup.gui_modules) +
    " widgets=" + describe_modules(lookup.widgets_modules) +
    " instance=" + std::to_string(lookup.instance_proc != nullptr) +
    " processEvents=" + std::to_string(lookup.process_events_proc != nullptr) +
    " sendPostedEvents=" + std::to_string(lookup.send_posted_events_proc != nullptr) +
    " setStyleSheet=" + std::to_string(lookup.set_stylesheet_proc != nullptr) +
    " QString::fromUtf8=" + std::to_string(lookup.from_utf8_proc != nullptr) +
    " QObject::inherits=" + std::to_string(lookup.object_inherits_proc != nullptr) +
    " QObject::metaObject=" + std::to_string(lookup.object_meta_object_proc != nullptr) +
    " QMetaObject::className=" + std::to_string(lookup.meta_object_class_name_proc != nullptr) +
    " activeWindow=" + std::to_string(lookup.active_window_proc != nullptr) +
    " focusWidget=" + std::to_string(lookup.app_focus_widget_proc != nullptr) +
    " QWidget::find=" + std::to_string(lookup.widget_find_proc != nullptr) +
    " QWidget::setStyleSheet=" + std::to_string(lookup.widget_set_stylesheet_proc != nullptr) +
    " QWidget::setAttribute=" + std::to_string(lookup.widget_set_attribute_proc != nullptr) +
    " QWidget::setAutoFillBackground=" + std::to_string(lookup.widget_set_auto_fill_background_proc != nullptr) +
    " QWidget::childAt=" + std::to_string(lookup.widget_child_at_proc != nullptr) +
    " QWidget::palette=" + std::to_string(lookup.widget_palette_proc != nullptr) +
    " QWidget::setPalette=" + std::to_string(lookup.widget_set_palette_proc != nullptr) +
    " QAbstractScrollArea::viewport=" + std::to_string(lookup.abstract_scroll_area_viewport_proc != nullptr) +
    " QApplication::palette(class)=" + std::to_string(lookup.app_palette_class_proc != nullptr) +
    " QApplication::palette(widget)=" + std::to_string(lookup.app_palette_widget_proc != nullptr) +
    " QApplication::setPalette=" + std::to_string(lookup.app_set_palette_proc != nullptr) +
    " QPalette::copy=" + std::to_string(lookup.qpalette_copy_ctor_proc != nullptr) +
    " QPalette::setColor=" + std::to_string((lookup.qpalette_set_color_group_proc || lookup.qpalette_set_color_role_proc) ? 1 : 0) +
    " QColor::ctor=" + std::to_string(lookup.qcolor_rgb_ctor_proc != nullptr) +
    " QColor::rgb=" + std::to_string(lookup.qcolor_rgb_proc != nullptr) +
    " QColor::setRgb=" + std::to_string(lookup.qcolor_set_rgb_proc != nullptr) +
    " QPainter::pen=" + std::to_string(lookup.qpainter_save_proc != nullptr &&
      lookup.qpainter_restore_proc != nullptr && lookup.qpainter_set_pen_color_proc != nullptr) +
    " QWidget::update=" + std::to_string(lookup.widget_update_proc != nullptr) +
    " QWidget::repaint=" + std::to_string(lookup.widget_repaint_proc != nullptr) +
    " allWidgets=" + std::to_string(lookup.app_all_widgets_proc != nullptr) +
    " topLevelWidgets=" + std::to_string(lookup.app_top_level_widgets_proc != nullptr) +
    " QObject::objectName=" + std::to_string(lookup.object_object_name_proc != nullptr) +
    " QObject::children=" + std::to_string(lookup.object_children_proc != nullptr) +
    " QObject::parent=" + std::to_string(lookup.object_parent_proc != nullptr) +
    " QString::toUtf8=" + std::to_string(lookup.qstring_to_utf8_proc != nullptr) +
    " QWidget::geometry=" + std::to_string(lookup.widget_geometry_proc != nullptr) +
    " QWidget::rect=" + std::to_string(lookup.widget_rect_proc != nullptr) +
    " QWidget::windowTitle=" + std::to_string(lookup.widget_window_title_proc != nullptr) +
    " instance_symbol=" + lookup.instance_symbol +
    " processEvents_symbol=" + lookup.process_events_symbol +
    " sendPostedEvents_symbol=" + lookup.send_posted_events_symbol +
    " setStyleSheet_symbol=" + lookup.set_stylesheet_symbol +
    " fromUtf8_symbol=" + lookup.from_utf8_symbol +
    " inherits_symbol=" + lookup.object_inherits_symbol +
    " metaObject_symbol=" + lookup.object_meta_object_symbol +
    " className_symbol=" + lookup.meta_object_class_name_symbol +
    " activeWindow_symbol=" + lookup.active_window_symbol +
    " focusWidget_symbol=" + lookup.app_focus_widget_symbol +
    " widgetFind_symbol=" + lookup.widget_find_symbol +
    " widgetSetStyleSheet_symbol=" + lookup.widget_set_stylesheet_symbol +
    " widgetSetAttribute_symbol=" + lookup.widget_set_attribute_symbol +
    " widgetSetAutoFillBackground_symbol=" + lookup.widget_set_auto_fill_background_symbol +
    " widgetChildAt_symbol=" + lookup.widget_child_at_symbol +
    " widgetPalette_symbol=" + lookup.widget_palette_symbol +
    " widgetSetPalette_symbol=" + lookup.widget_set_palette_symbol +
    " abstractScrollAreaViewport_symbol=" + lookup.abstract_scroll_area_viewport_symbol +
    " appPaletteClass_symbol=" + lookup.app_palette_class_symbol +
    " appPaletteWidget_symbol=" + lookup.app_palette_widget_symbol +
    " appSetPalette_symbol=" + lookup.app_set_palette_symbol +
    " qpaletteCopy_symbol=" + lookup.qpalette_copy_ctor_symbol +
    " qpaletteColorGroupRole_symbol=" + lookup.qpalette_color_group_role_symbol +
    " qpaletteSetColorGroup_symbol=" + lookup.qpalette_set_color_group_symbol +
    " qpaletteSetColorRole_symbol=" + lookup.qpalette_set_color_role_symbol +
    " qcolorRgbCtor_symbol=" + lookup.qcolor_rgb_ctor_symbol +
    " qcolorRgb_symbol=" + lookup.qcolor_rgb_symbol +
    " qcolorSetRgb_symbol=" + lookup.qcolor_set_rgb_symbol +
    " widgetUpdate_symbol=" + lookup.widget_update_symbol +
    " widgetRepaint_symbol=" + lookup.widget_repaint_symbol +
    " allWidgets_symbol=" + lookup.app_all_widgets_symbol +
    " topLevelWidgets_symbol=" + lookup.app_top_level_widgets_symbol +
    " objectName_symbol=" + lookup.object_object_name_symbol +
    " children_symbol=" + lookup.object_children_symbol +
    " parent_symbol=" + lookup.object_parent_symbol +
    " qstringToUtf8_symbol=" + lookup.qstring_to_utf8_symbol +
    " widgetGeometry_symbol=" + lookup.widget_geometry_symbol +
    " widgetRect_symbol=" + lookup.widget_rect_symbol +
    " widgetWindowTitle_symbol=" + lookup.widget_window_title_symbol;
}

void flush_qt_pending_events(const QtLookup& lookup) {
  using QtProcessEventsFn = void(__cdecl*)(int);
  using QtSendPostedEventsFn = void(__cdecl*)(void*, int);

  if (!lookup.process_events_proc && !lookup.send_posted_events_proc) {
    return;
  }

  if (lookup.send_posted_events_proc) {
    reinterpret_cast<QtSendPostedEventsFn>(lookup.send_posted_events_proc)(nullptr, 0);
  }
  if (lookup.process_events_proc) {
    reinterpret_cast<QtProcessEventsFn>(lookup.process_events_proc)(0);
  }
  if (lookup.send_posted_events_proc) {
    reinterpret_cast<QtSendPostedEventsFn>(lookup.send_posted_events_proc)(nullptr, 0);
  }
}

std::string qobject_inherits_summary(const QtLookup& lookup, const void* object) {
  using QtObjectInheritsFn = bool(__cdecl*)(const void*, const char*);
  if (!object || !lookup.object_inherits_proc) {
    return "inherits=unknown";
  }

  QtObjectInheritsFn inherits = reinterpret_cast<QtObjectInheritsFn>(lookup.object_inherits_proc);
  return "inherits QWidget=" + std::to_string(inherits(object, "QWidget")) +
    " QMainWindow=" + std::to_string(inherits(object, "QMainWindow")) +
    " QDialog=" + std::to_string(inherits(object, "QDialog")) +
    " QDockWidget=" + std::to_string(inherits(object, "QDockWidget"));
}

std::string qobject_class_name(const QtLookup& lookup, const void* object) {
  using QtMetaObjectFn = const void*(__cdecl*)(const void*);
  using QtClassNameFn = const char*(__cdecl*)(const void*);

  if (!object || !lookup.object_meta_object_proc || !lookup.meta_object_class_name_proc) {
    return "unknown";
  }

  const void* meta_object = reinterpret_cast<QtMetaObjectFn>(lookup.object_meta_object_proc)(object);
  if (!meta_object) {
    return "null-metaobject";
  }

  const char* class_name = reinterpret_cast<QtClassNameFn>(lookup.meta_object_class_name_proc)(meta_object);
  return class_name ? class_name : "null-classname";
}

std::string qobject_class_name_quiet(const QtLookup& lookup, const void* object) {
  using QtMetaObjectFn = const void*(__cdecl*)(const void*);
  using QtClassNameFn = const char*(__cdecl*)(const void*);

  if (!object || !lookup.object_meta_object_proc || !lookup.meta_object_class_name_proc) {
    return "unknown";
  }

  const void* meta_object = reinterpret_cast<QtMetaObjectFn>(lookup.object_meta_object_proc)(object);
  if (!meta_object) {
    return "null-metaobject";
  }

  const char* class_name = reinterpret_cast<QtClassNameFn>(lookup.meta_object_class_name_proc)(meta_object);
  return class_name ? class_name : "null-classname";
}

bool memory_readable(const void* address, size_t bytes) {
  if (!address || bytes == 0) {
    return false;
  }

  const auto start = reinterpret_cast<std::uintptr_t>(address);
  const auto end = start + bytes;
  if (end < start) {
    return false;
  }

  std::uintptr_t current = start;
  while (current < end) {
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(reinterpret_cast<const void*>(current), &info, sizeof(info)) != sizeof(info)) {
      return false;
    }
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
      return false;
    }
    const auto region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (region_end <= current) {
      return false;
    }
    current = std::min(end, region_end);
  }
  return true;
}

struct VcbTypedInputBackgroundControl {
  using Setter = void(__fastcall*)(bool);

  Setter setter = nullptr;
  volatile unsigned char* value = nullptr;
};

const unsigned char* rip_relative_target(const unsigned char* instruction, size_t instruction_size) {
  std::int32_t displacement = 0;
  std::memcpy(&displacement, instruction + instruction_size - sizeof(displacement), sizeof(displacement));
  const auto next_instruction = reinterpret_cast<std::intptr_t>(instruction + instruction_size);
  return reinterpret_cast<const unsigned char*>(next_instruction + displacement);
}

std::vector<void**> find_function_indirect_call_slots(void* function_proc, const std::vector<void*>& target_procs) {
  std::vector<void**> slots;
  if (!function_proc || target_procs.empty()) {
    return slots;
  }

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  using RtlLookupFunctionEntryFn = PRUNTIME_FUNCTION(WINAPI*)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
  const auto rtl_lookup_function_entry = ntdll ? reinterpret_cast<RtlLookupFunctionEntryFn>(GetProcAddress(ntdll, "RtlLookupFunctionEntry")) : nullptr;
  if (!rtl_lookup_function_entry) {
    return slots;
  }

  DWORD64 image_base = 0;
  const PRUNTIME_FUNCTION function_entry = rtl_lookup_function_entry(
    reinterpret_cast<DWORD64>(function_proc), &image_base, nullptr);
  if (!function_entry || image_base == 0 || function_entry->EndAddress <= function_entry->BeginAddress) {
    return slots;
  }

  const auto* function_begin = reinterpret_cast<const unsigned char*>(image_base + function_entry->BeginAddress);
  const auto* function_end = reinterpret_cast<const unsigned char*>(image_base + function_entry->EndAddress);
  if (!memory_readable(function_begin, static_cast<size_t>(function_end - function_begin))) {
    return slots;
  }

  for (const auto* instruction = function_begin; instruction + 6 <= function_end; ++instruction) {
    void** slot = nullptr;
    if (instruction[0] == 0xff && instruction[1] == 0x15) {
      slot = reinterpret_cast<void**>(const_cast<unsigned char*>(rip_relative_target(instruction, 6)));
    } else if (instruction[0] == 0xe8) {
      std::int32_t displacement = 0;
      std::memcpy(&displacement, instruction + 1, sizeof(displacement));
      const auto* thunk = reinterpret_cast<const unsigned char*>(
        reinterpret_cast<std::intptr_t>(instruction + 5) + displacement);
      if (memory_readable(thunk, 6) && thunk[0] == 0xff && thunk[1] == 0x25) {
        slot = reinterpret_cast<void**>(const_cast<unsigned char*>(rip_relative_target(thunk, 6)));
      }
    }
    if (!slot ||
        !memory_readable(slot, sizeof(*slot)) ||
        std::find(target_procs.begin(), target_procs.end(), *slot) == target_procs.end() ||
        std::find(slots.begin(), slots.end(), slot) != slots.end()) {
      continue;
    }
    slots.push_back(slot);
  }
  return slots;
}

VcbTypedInputBackgroundControl find_vcb_typed_input_background_control() {
  constexpr char key[] = "VCBShowTypedInputBackground";
  HMODULE executable = GetModuleHandleW(nullptr);
  if (!executable) {
    return {};
  }

  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base);
  if (!memory_readable(dos_header, sizeof(*dos_header)) || dos_header->e_magic != IMAGE_DOS_SIGNATURE || dos_header->e_lfanew <= 0) {
    return {};
  }

  const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew);
  if (!memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE || nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return {};
  }

  const size_t image_size = nt_headers->OptionalHeader.SizeOfImage;
  if (image_size == 0 || !memory_readable(image_base, image_size)) {
    return {};
  }

  const auto* key_address = std::search(image_base, image_base + image_size, key, key + sizeof(key) - 1);
  if (key_address == image_base + image_size) {
    return {};
  }

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  using RtlLookupFunctionEntryFn = PRUNTIME_FUNCTION(WINAPI*)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
  const auto rtl_lookup_function_entry = ntdll ? reinterpret_cast<RtlLookupFunctionEntryFn>(GetProcAddress(ntdll, "RtlLookupFunctionEntry")) : nullptr;
  if (!rtl_lookup_function_entry) {
    return {};
  }

  const auto* section = IMAGE_FIRST_SECTION(nt_headers);
  for (WORD section_index = 0; section_index < nt_headers->FileHeader.NumberOfSections; ++section_index) {
    const IMAGE_SECTION_HEADER& current_section = section[section_index];
    if ((current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 || current_section.Misc.VirtualSize < 7) {
      continue;
    }

    const auto* section_begin = image_base + current_section.VirtualAddress;
    const auto* section_end = section_begin + current_section.Misc.VirtualSize;
    if (!memory_readable(section_begin, current_section.Misc.VirtualSize)) {
      continue;
    }

    for (const auto* instruction = section_begin; instruction + 7 <= section_end; ++instruction) {
      if (instruction[0] != 0x48 || instruction[1] != 0x8d || instruction[2] != 0x15 || rip_relative_target(instruction, 7) != key_address) {
        continue;
      }

      const auto* write_begin = instruction > section_begin + 64 ? instruction - 64 : section_begin;
      for (const auto* write = write_begin; write + 6 <= instruction; ++write) {
        if (write[0] != 0x88 || write[1] != 0x0d) {
          continue;
        }

        auto* value = const_cast<volatile unsigned char*>(rip_relative_target(write, 6));
        if (!memory_readable(const_cast<const unsigned char*>(value), sizeof(*value))) {
          continue;
        }

        DWORD64 function_image_base = 0;
        PRUNTIME_FUNCTION function_entry = rtl_lookup_function_entry(reinterpret_cast<DWORD64>(instruction), &function_image_base, nullptr);
        if (!function_entry || function_image_base != reinterpret_cast<DWORD64>(image_base)) {
          continue;
        }

        auto* function_address = image_base + function_entry->BeginAddress;
        return {reinterpret_cast<VcbTypedInputBackgroundControl::Setter>(const_cast<unsigned char*>(function_address)), value};
      }
    }
  }

  return {};
}

bool set_vcb_typed_input_background(bool show) {
  const VcbTypedInputBackgroundControl control = find_vcb_typed_input_background_control();
  if (!control.setter || !control.value) {
    return false;
  }

  if (show && g_vcb_original_typed_input_background < 0) {
    return true;
  }

  if (!show && g_vcb_original_typed_input_background < 0) {
    g_vcb_original_typed_input_background = *control.value == 0 ? 0 : 1;
  }

  const bool target = show ? g_vcb_original_typed_input_background != 0 : false;
  control.setter(target);
  const bool applied = (*control.value != 0) == target;

  if (show) {
    g_vcb_original_typed_input_background = -1;
  }
  return applied;
}

bool pointer_looks_like_cpp_object(const void* object) {
  if (!object || (reinterpret_cast<std::uintptr_t>(object) & 0x7) != 0 || !memory_readable(object, sizeof(void*))) {
    return false;
  }
  const void* const* object_words = reinterpret_cast<const void* const*>(object);
  const void* vtable = object_words[0];
  return vtable && memory_readable(vtable, sizeof(void*));
}

std::string sanitize_log_text(std::string value, size_t limit = 240) {
  for (char& ch : value) {
    if (ch == '\r' || ch == '\n' || ch == '\t') {
      ch = ' ';
    }
  }
  if (value.size() > limit) {
    value.resize(limit);
    value += "...";
  }
  return value;
}

bool qt_object_inherits(const QtLookup& lookup, const void* object, const char* class_name) {
  using QtObjectInheritsFn = bool(__cdecl*)(const void*, const char*);
  if (!object || !lookup.object_inherits_proc || !class_name) {
    return false;
  }
  return reinterpret_cast<QtObjectInheritsFn>(lookup.object_inherits_proc)(object, class_name);
}

std::string qobject_inherits_detail(const QtLookup& lookup, const void* object) {
  const char* names[] = {
    "QWidget", "QFrame", "QLabel", "QAbstractButton", "QToolButton", "QPushButton",
    "QMainWindow", "QDialog", "QDockWidget", "QToolBar", "QMenu", "QMenuBar",
    "QTabBar", "QSplitter", "QScrollArea", "QAbstractScrollArea", "QStackedWidget",
    "QGroupBox", "QLineEdit", "QComboBox", "QTextEdit", "QPlainTextEdit", "QSpinBox",
    "QDoubleSpinBox", "QSlider", "QHeaderView", "QListView", "QTreeView", "QTableView",
    "QColumnView", "QAbstractItemView", "QLayout", "QAction"
  };

  std::string result = "inherits=";
  bool any = false;
  for (const char* name : names) {
    if (qt_object_inherits(lookup, object, name)) {
      result += any ? "," : "";
      result += name;
      any = true;
    }
  }
  return any ? result : "inherits=none-of-probed";
}

std::string qt_string_return_to_utf8(const QtLookup& lookup, FARPROC string_return_proc, const void* object) {
  if (!string_return_proc || !object || !lookup.qstring_to_utf8_proc || !lookup.qbytearray_const_data_proc ||
      !lookup.qbytearray_size_proc || !lookup.qstring_destructor_proc || !lookup.qbytearray_destructor_proc) {
    return "";
  }

  using QtStringReturnFn = void(__cdecl*)(void*, const void*);
  using QtStringToUtf8Fn = void(__cdecl*)(void*, const void*);
  using QtByteArrayConstDataFn = const char*(__cdecl*)(const void*);
  using QtByteArraySizeFn = long long(__cdecl*)(const void*);
  using QtDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char qstring_storage[64] = {};
  alignas(16) unsigned char bytearray_storage[64] = {};
  reinterpret_cast<QtStringReturnFn>(string_return_proc)(qstring_storage, object);
  reinterpret_cast<QtStringToUtf8Fn>(lookup.qstring_to_utf8_proc)(bytearray_storage, qstring_storage);

  std::string result;
  const long long size = reinterpret_cast<QtByteArraySizeFn>(lookup.qbytearray_size_proc)(bytearray_storage);
  const char* data = reinterpret_cast<QtByteArrayConstDataFn>(lookup.qbytearray_const_data_proc)(bytearray_storage);
  if (data && size > 0 && size < 4096 && memory_readable(data, static_cast<size_t>(size))) {
    result.assign(data, data + size);
  }

  reinterpret_cast<QtDestructorFn>(lookup.qbytearray_destructor_proc)(bytearray_storage);
  reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  return sanitize_log_text(result);
}

struct QtPointerListView {
  void** items = nullptr;
  long long size = 0;
  std::string layout;
};

QtPointerListView decode_qt_pointer_list(const void* list_object) {
  QtPointerListView view;
  if (!list_object || !memory_readable(list_object, 24)) {
    view.layout = "unreadable-list";
    return view;
  }

  const auto* bytes = reinterpret_cast<const unsigned char*>(list_object);
  void* ptr = *reinterpret_cast<void* const*>(bytes + 8);
  const long long size = *reinterpret_cast<const long long*>(bytes + 16);
  if (size < 0 || size > 20000) {
    view.layout = "rejected-size-" + std::to_string(size);
    return view;
  }
  if (size > 0 && (!ptr || !memory_readable(ptr, static_cast<size_t>(size) * sizeof(void*)))) {
    view.layout = "unreadable-items size=" + std::to_string(size);
    return view;
  }

  view.items = reinterpret_cast<void**>(ptr);
  view.size = size;
  view.layout = "qt6-dptr-ptr-size";
  return view;
}

std::string qrect_values_to_string(const int* values) {
  if (!values) {
    return "unknown";
  }
  const int left = values[0];
  const int top = values[1];
  const int right = values[2];
  const int bottom = values[3];
  return std::to_string(left) + "," + std::to_string(top) + "," +
    std::to_string(right) + "," + std::to_string(bottom) +
    " size=" + std::to_string(std::max(0, right - left + 1)) + "x" +
    std::to_string(std::max(0, bottom - top + 1));
}

std::string qt_widget_detail(const QtLookup& lookup, const void* widget) {
  if (!widget || !qt_object_inherits(lookup, widget, "QWidget")) {
    return "";
  }

  using QtBoolWidgetFn = bool(__cdecl*)(const void*);
  using QtPtrWidgetFn = void*(__cdecl*)(const void*);
  using QtRectRefFn = const int*(__cdecl*)(const void*);

  std::string line = " widget=1";
  if (lookup.widget_is_visible_proc) {
    line += " visible=" + std::to_string(reinterpret_cast<QtBoolWidgetFn>(lookup.widget_is_visible_proc)(widget) ? 1 : 0);
  }
  if (lookup.widget_is_window_proc) {
    line += " isWindow=" + std::to_string(reinterpret_cast<QtBoolWidgetFn>(lookup.widget_is_window_proc)(widget) ? 1 : 0);
  }
  if (lookup.widget_parent_widget_proc) {
    void* parent_widget = reinterpret_cast<QtPtrWidgetFn>(lookup.widget_parent_widget_proc)(widget);
    line += " parentWidget=" + pointer_hex(parent_widget);
  }
  if (lookup.widget_geometry_proc) {
    const int* geometry = reinterpret_cast<QtRectRefFn>(lookup.widget_geometry_proc)(widget);
    if (geometry && memory_readable(geometry, sizeof(int) * 4)) {
      line += " geometry=" + qrect_values_to_string(geometry);
    }
  }
  if (lookup.widget_window_title_proc) {
    const std::string title = qt_string_return_to_utf8(lookup, lookup.widget_window_title_proc, widget);
    if (!title.empty()) {
      line += " windowTitle=\"" + title + "\"";
    }
  }
  return line;
}

std::string qt_object_detail(const QtLookup& lookup, const void* object) {
  using QtParentFn = void*(__cdecl*)(const void*);
  std::string line = "object=" + pointer_hex(object) + " class=" + qobject_class_name_quiet(lookup, object);
  const std::string object_name = qt_string_return_to_utf8(lookup, lookup.object_object_name_proc, object);
  if (!object_name.empty()) {
    line += " objectName=\"" + object_name + "\"";
  }
  if (lookup.object_parent_proc) {
    void* parent = reinterpret_cast<QtParentFn>(lookup.object_parent_proc)(object);
    line += " parent=" + pointer_hex(parent);
    if (parent && pointer_looks_like_cpp_object(parent)) {
      line += " parentClass=" + qobject_class_name_quiet(lookup, parent);
    }
  }
  line += " " + qobject_inherits_detail(lookup, object);
  line += qt_widget_detail(lookup, object);
  return line;
}

std::string qt_object_safe_detail(const QtLookup& lookup, const void* object) {
  if (!object || !pointer_looks_like_cpp_object(object)) {
    return "object=" + pointer_hex(object) + " unreadable=1";
  }
  return "object=" + pointer_hex(object) + " class=" + qobject_class_name_quiet(lookup, object) + " " + qobject_inherits_summary(lookup, object) + " " + qobject_inherits_detail(lookup, object);
}

size_t dump_qwidget_list_return(FARPROC list_proc, std::vector<void*>& widgets, size_t log_limit) {
  if (!list_proc) {
    return 0;
  }

  using QtWidgetListReturnFn = void(__cdecl*)(void*);
  alignas(16) unsigned char list_storage[64] = {};
  reinterpret_cast<QtWidgetListReturnFn>(list_proc)(list_storage);
  QtPointerListView view = decode_qt_pointer_list(list_storage);

  const size_t count = static_cast<size_t>(std::max<long long>(0, view.size));
  const size_t limit = std::min(count, log_limit);
  for (size_t index = 0; index < limit; ++index) {
    void* widget = view.items ? view.items[index] : nullptr;
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }
    widgets.push_back(widget);
  }
  if (count > limit) {
  }
  return count;
}

void dump_qobject_tree(const QtLookup& lookup, const void* object, int depth, size_t& count, size_t limit, std::unordered_set<std::uintptr_t>& visited) {
  if (!object || count >= limit || !pointer_looks_like_cpp_object(object)) {
    return;
  }

  const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(object);
  if (!visited.insert(key).second) {
    return;
  }

  count += 1;
  if (!lookup.object_children_proc || depth >= 24 || count >= limit) {
    return;
  }

  using QtChildrenFn = const void*(__cdecl*)(const void*);
  const void* children_list = reinterpret_cast<QtChildrenFn>(lookup.object_children_proc)(object);
  QtPointerListView children = decode_qt_pointer_list(children_list);
  const size_t child_count = static_cast<size_t>(std::max<long long>(0, children.size));
  for (size_t index = 0; index < child_count && count < limit; ++index) {
    void* child = children.items ? children.items[index] : nullptr;
    dump_qobject_tree(lookup, child, depth + 1, count, limit, visited);
  }
}

size_t dump_main_window_children(const QtLookup& lookup, size_t child_limit, const char* label);

size_t dump_deep_qt_inventory(const QtLookup& lookup) {

  const size_t native_count = dump_main_window_children(lookup, 6000, "stage 26 safe native");

  return native_count;
}

bool try_qobject_children_list(FARPROC object_children_proc, const void* object, const void** children_list) {
  if (children_list) {
    *children_list = nullptr;
  }
  if (!object_children_proc || !object || !children_list) {
    return false;
  }

  using QtChildrenFn = const void*(__cdecl*)(const void*);
  __try {
    *children_list = reinterpret_cast<QtChildrenFn>(object_children_proc)(object);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

std::string pointer_words_summary(const void* address, size_t word_count = 4) {
  if (!address || !memory_readable(address, sizeof(void*) * word_count)) {
    return "words=unreadable";
  }

  const auto* words = reinterpret_cast<const void* const*>(address);
  std::string summary = "words=";
  for (size_t index = 0; index < word_count; ++index) {
    summary += index == 0 ? "" : ",";
    summary += pointer_hex(words[index]);
  }
  return summary;
}

size_t dump_qobject_children_limited(const QtLookup& lookup, const void* object, const std::string& label, int depth, int max_depth, size_t& count, size_t limit, std::unordered_set<std::uintptr_t>& visited, bool follow_widget_children_only = false) {
  if (!object || count >= limit || depth > max_depth) {
    return count;
  }

  const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(object);
  if (!visited.insert(key).second) {
    return count;
  }

  if (!lookup.object_children_proc || depth >= max_depth || count >= limit) {
    return count;
  }

  const void* children_list = nullptr;
  if (!try_qobject_children_list(lookup.object_children_proc, object, &children_list)) {
    return count;
  }

  QtPointerListView children = decode_qt_pointer_list(children_list);
  const size_t child_count = std::min<size_t>(static_cast<size_t>(std::max<long long>(0, children.size)), 32);
  for (size_t index = 0; index < child_count && count < limit; ++index) {
    void* child = children.items ? children.items[index] : nullptr;
    count += 1;
    if (!pointer_looks_like_cpp_object(child)) {
      continue;
    }
    if (!follow_widget_children_only || qt_object_inherits(lookup, child, "QWidget")) {
      dump_qobject_children_limited(lookup, child, label, depth + 1, max_depth, count, limit, visited, follow_widget_children_only);
    }
  }
  if (static_cast<size_t>(std::max<long long>(0, children.size)) > child_count) {
  }
  return count;
}

std::string describe_hwnd_rect(HWND hwnd) {
  RECT rect = {};
  if (!GetWindowRect(hwnd, &rect)) {
    return " rect=unknown";
  }

  return " rect=" + std::to_string(rect.left) + "," + std::to_string(rect.top) + "," +
    std::to_string(rect.right) + "," + std::to_string(rect.bottom) +
    " size=" + std::to_string(std::max<LONG>(0, rect.right - rect.left)) + "x" +
    std::to_string(std::max<LONG>(0, rect.bottom - rect.top));
}

struct ChildWindowDumpContext {
  const QtLookup* lookup = nullptr;
  size_t count = 0;
  size_t limit = 500;
  const char* label = "stage 15";
};

BOOL CALLBACK dump_child_window_proc(HWND hwnd, LPARAM lparam) {
  auto* context = reinterpret_cast<ChildWindowDumpContext*>(lparam);
  if (!context || context->count >= context->limit) {
    return FALSE;
  }

  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  context->count += 1;
  std::string line = std::string(context->label ? context->label : "stage 15") + " child[" + std::to_string(context->count) + "] hwnd=" + hwnd_hex(hwnd) +
    " parent=" + hwnd_hex(GetParent(hwnd)) +
    " visible=" + std::to_string(IsWindowVisible(hwnd) ? 1 : 0) +
    " class=" + hwnd_class_name(hwnd) +
    " title=" + hwnd_text(hwnd) +
    describe_hwnd_rect(hwnd);

  if (context->lookup && context->lookup->widget_find_proc) {
    QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(context->lookup->widget_find_proc);
    void* widget = widget_find(reinterpret_cast<std::uintptr_t>(hwnd));
    line += " widget=" + pointer_hex(widget);
    if (widget) {
      line += " qt_class=" + qobject_class_name_quiet(*context->lookup, widget) +
        " " + qobject_inherits_summary(*context->lookup, widget);
    }
  }

  return context->count < context->limit;
}

size_t dump_main_window_children(const QtLookup& lookup, size_t child_limit = 500, const char* label = "stage 15") {
  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();

  size_t total_children = 0;
  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  QtWidgetFindFn widget_find = lookup.widget_find_proc ? reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc) : nullptr;

  for (size_t index = 0; index < candidates.size() && index < 12; ++index) {
    const NativeWindowCandidate& candidate = candidates[index];
    std::string line = std::string(label ? label : "stage 15") + " top[" + std::to_string(index + 1) + "] " + describe_native_window(candidate) + describe_hwnd_rect(candidate.hwnd);
    if (widget_find) {
      void* widget = widget_find(reinterpret_cast<std::uintptr_t>(candidate.hwnd));
      line += " widget=" + pointer_hex(widget);
      if (widget) {
        line += " qt_class=" + qobject_class_name_quiet(lookup, widget) + " " + qobject_inherits_summary(lookup, widget);
      }
    }

    ChildWindowDumpContext context;
    context.lookup = &lookup;
    context.limit = child_limit;
    context.label = label;
    EnumChildWindows(candidate.hwnd, dump_child_window_proc, reinterpret_cast<LPARAM>(&context));
    total_children += context.count;
  }

  return total_children;
}

bool text_looks_like_tray_marker(const std::string& title, const std::string& class_name);

std::string describe_hwnd_for_probe(const QtLookup& lookup, HWND hwnd) {
  std::string line = describe_native_window({hwnd, hwnd_text(hwnd), hwnd_class_name(hwnd), 0}) +
    " parent=" + hwnd_hex(GetParent(hwnd)) +
    " visible=" + std::to_string(IsWindowVisible(hwnd) ? 1 : 0) +
    describe_hwnd_rect(hwnd);

  if (lookup.widget_find_proc) {
    using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
    QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc);
    void* widget = widget_find(reinterpret_cast<std::uintptr_t>(hwnd));
    line += " widget=" + pointer_hex(widget);
    if (widget) {
      line += " qt_class=" + qobject_class_name_quiet(lookup, widget) +
        " " + qobject_inherits_summary(lookup, widget);
    }
  }

  return line;
}

void add_unique_hwnd(std::vector<HWND>& hwnds, HWND hwnd) {
  if (hwnd && std::find(hwnds.begin(), hwnds.end(), hwnd) == hwnds.end()) {
    hwnds.push_back(hwnd);
  }
}

bool hwnd_is_self_or_descendant(HWND root, HWND hwnd) {
  for (HWND current = hwnd; current; current = GetParent(current)) {
    if (current == root) {
      return true;
    }
  }
  return false;
}

bool hwnd_is_current_process(HWND hwnd) {
  DWORD window_pid = 0;
  GetWindowThreadProcessId(hwnd, &window_pid);
  return window_pid == GetCurrentProcessId();
}

bool hwnd_looks_like_non_tray_surface(HWND hwnd) {
  const std::string text = lower_ascii(hwnd_text(hwnd) + " " + hwnd_class_name(hwnd));
  return text.find("sketchupview") != std::string::npos ||
    text.find("chrome") != std::string::npos ||
    text.find("cef") != std::string::npos ||
    text.find("internet explorer") != std::string::npos;
}

bool text_looks_like_viewport_marker(const std::string& title, const std::string& class_name) {
  const std::string text = lower_ascii(title + " " + class_name);
  return text.find("sketchupview") != std::string::npos ||
    text.find("sketchup view") != std::string::npos;
}

struct HwndListContext {
  std::vector<HWND>* hwnds = nullptr;
};

BOOL CALLBACK collect_viewport_marker_hwnds_proc(HWND hwnd, LPARAM lparam) {
  auto* context = reinterpret_cast<HwndListContext*>(lparam);
  if (!context || !context->hwnds) {
    return TRUE;
  }

  if (text_looks_like_viewport_marker(hwnd_text(hwnd), hwnd_class_name(hwnd))) {
    add_unique_hwnd(*context->hwnds, hwnd);
  }
  return TRUE;
}

BOOL CALLBACK collect_tray_marker_hwnds_proc(HWND hwnd, LPARAM lparam) {
  auto* context = reinterpret_cast<HwndListContext*>(lparam);
  if (!context || !context->hwnds) {
    return TRUE;
  }

  if (text_looks_like_tray_marker(hwnd_text(hwnd), hwnd_class_name(hwnd))) {
    add_unique_hwnd(*context->hwnds, hwnd);
  }
  return TRUE;
}

BOOL CALLBACK collect_descendant_hwnds_proc(HWND hwnd, LPARAM lparam) {
  auto* context = reinterpret_cast<HwndListContext*>(lparam);
  if (!context || !context->hwnds) {
    return TRUE;
  }

  add_unique_hwnd(*context->hwnds, hwnd);
  return TRUE;
}

std::vector<HWND> find_tray_marker_hwnds() {
  std::vector<HWND> tray_hwnds;
  HwndListContext context;
  context.hwnds = &tray_hwnds;

  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();
  for (size_t index = 0; index < candidates.size() && index < 12; ++index) {
    if (text_looks_like_tray_marker(candidates[index].title, candidates[index].class_name)) {
      add_unique_hwnd(tray_hwnds, candidates[index].hwnd);
    }
    EnumChildWindows(candidates[index].hwnd, collect_tray_marker_hwnds_proc, reinterpret_cast<LPARAM>(&context));
  }
  return tray_hwnds;
}

std::vector<HWND> find_viewport_marker_hwnds() {
  std::vector<HWND> viewport_hwnds;
  HwndListContext context;
  context.hwnds = &viewport_hwnds;

  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();
  for (size_t index = 0; index < candidates.size() && index < 12; ++index) {
    if (text_looks_like_viewport_marker(candidates[index].title, candidates[index].class_name)) {
      add_unique_hwnd(viewport_hwnds, candidates[index].hwnd);
    }
    EnumChildWindows(candidates[index].hwnd, collect_viewport_marker_hwnds_proc, reinterpret_cast<LPARAM>(&context));
  }
  return viewport_hwnds;
}

std::vector<HWND> hwnd_descendants_including_self(HWND root) {
  std::vector<HWND> hwnds;
  add_unique_hwnd(hwnds, root);
  HwndListContext context;
  context.hwnds = &hwnds;
  EnumChildWindows(root, collect_descendant_hwnds_proc, reinterpret_cast<LPARAM>(&context));
  return hwnds;
}

std::vector<HWND> process_windows_including_descendants() {
  std::vector<HWND> hwnds;
  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();
  for (const NativeWindowCandidate& candidate : candidates) {
    add_unique_hwnd(hwnds, candidate.hwnd);
    HwndListContext context;
    context.hwnds = &hwnds;
    EnumChildWindows(candidate.hwnd, collect_descendant_hwnds_proc, reinterpret_cast<LPARAM>(&context));
  }
  return hwnds;
}

bool text_looks_like_measurements_entry_pad(const std::string& title, const std::string& class_name) {
  const std::string text = lower_ascii(title + " " + class_name);
  return text.find("measurementsentrypad") != std::string::npos ||
    text.find("statusvcbedit") != std::string::npos;
}

std::vector<HWND> find_measurements_entry_pad_roots() {
  std::vector<HWND> roots;
  for (HWND hwnd : process_windows_including_descendants()) {
    const std::string title = hwnd_text(hwnd);
    const std::string class_name = hwnd_class_name(hwnd);
    if (!text_looks_like_measurements_entry_pad(title, class_name)) {
      continue;
    }
    add_unique_hwnd(roots, hwnd);
  }
  return roots;
}

bool measurements_entry_pad_contains(HWND hwnd) {
  if (!hwnd) {
    return false;
  }
  for (HWND root : g_measurements_entry_pad_roots) {
    if (root == hwnd || IsChild(root, hwnd)) {
      return true;
    }
  }
  return false;
}

MeasurementsEntryPadSubclass* measurements_entry_pad_subclass_for(HWND hwnd) {
  auto existing = std::find_if(g_measurements_entry_pad_subclasses.begin(), g_measurements_entry_pad_subclasses.end(), [hwnd](const MeasurementsEntryPadSubclass& entry) {
    return entry.hwnd == hwnd;
  });
  return existing == g_measurements_entry_pad_subclasses.end() ? nullptr : &(*existing);
}

LRESULT CALLBACK measurements_entry_pad_wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  MeasurementsEntryPadSubclass* entry = measurements_entry_pad_subclass_for(hwnd);
  WNDPROC original_proc = entry ? entry->original_proc : nullptr;

  if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC) &&
      g_measurements_entry_pad_background_brush &&
      measurements_entry_pad_contains(reinterpret_cast<HWND>(lparam))) {
    HDC hdc = reinterpret_cast<HDC>(wparam);
    if (hdc) {
      SetTextColor(hdc, RGB(230, 232, 238));
      SetBkColor(hdc, RGB(31, 31, 31));
      SetBkMode(hdc, OPAQUE);
    }
    return reinterpret_cast<LRESULT>(g_measurements_entry_pad_background_brush);
  }

  if (message == WM_ERASEBKGND &&
      g_measurements_entry_pad_background_brush &&
      measurements_entry_pad_contains(hwnd)) {
    HDC hdc = reinterpret_cast<HDC>(wparam);
    RECT rect = {};
    if (hdc && GetClientRect(hwnd, &rect)) {
      FillRect(hdc, &rect, g_measurements_entry_pad_background_brush);
      return 1;
    }
  }

  if (message == WM_NCDESTROY && entry && original_proc) {
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_proc));
    g_measurements_entry_pad_subclasses.erase(std::remove_if(g_measurements_entry_pad_subclasses.begin(), g_measurements_entry_pad_subclasses.end(), [hwnd](const MeasurementsEntryPadSubclass& item) {
      return item.hwnd == hwnd;
    }), g_measurements_entry_pad_subclasses.end());
  }

  return original_proc ? CallWindowProcW(original_proc, hwnd, message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

void restore_measurements_entry_pad_edit_colors() {
  for (const MeasurementsEntryPadEditColor& entry : g_measurements_entry_pad_edit_colors) {
    if (entry.hwnd && IsWindow(entry.hwnd)) {
      SendMessageW(entry.hwnd, EM_SETBKGNDCOLOR, TRUE, static_cast<LPARAM>(entry.original_background));
      RedrawWindow(entry.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
  }
  g_measurements_entry_pad_edit_colors.clear();
}

size_t remove_measurements_entry_pad_subclasses() {
  size_t removed = 0;
  for (const MeasurementsEntryPadSubclass& entry : g_measurements_entry_pad_subclasses) {
    if (!entry.hwnd || !entry.original_proc || !IsWindow(entry.hwnd)) {
      continue;
    }
    if (GetWindowLongPtrW(entry.hwnd, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(measurements_entry_pad_wndproc)) {
      SetWindowLongPtrW(entry.hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(entry.original_proc));
      removed += 1;
    }
    RedrawWindow(entry.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  }
  g_measurements_entry_pad_subclasses.clear();
  restore_measurements_entry_pad_edit_colors();
  g_measurements_entry_pad_roots.clear();
  if (g_measurements_entry_pad_background_brush) {
    DeleteObject(g_measurements_entry_pad_background_brush);
    g_measurements_entry_pad_background_brush = nullptr;
  }
  return removed;
}

void set_measurements_entry_pad_edit_backgrounds(HWND root) {
  for (HWND hwnd : hwnd_descendants_including_self(root)) {
    if (lower_ascii(hwnd_class_name(hwnd)) != "edit") {
      continue;
    }
    const LRESULT original_background = SendMessageW(hwnd, EM_SETBKGNDCOLOR, TRUE, RGB(31, 31, 31));
    g_measurements_entry_pad_edit_colors.push_back({hwnd, static_cast<COLORREF>(original_background)});
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
  }
}

size_t install_measurements_entry_pad_subclasses(bool dark) {
  remove_measurements_entry_pad_subclasses();
  if (!dark) {
    return 0;
  }

  g_measurements_entry_pad_roots = find_measurements_entry_pad_roots();
  if (g_measurements_entry_pad_roots.empty()) {
    return 0;
  }

  g_measurements_entry_pad_background_brush = CreateSolidBrush(RGB(31, 31, 31));
  if (!g_measurements_entry_pad_background_brush) {
    g_measurements_entry_pad_roots.clear();
    return 0;
  }

  size_t installed = 0;
  for (HWND root : g_measurements_entry_pad_roots) {
    set_measurements_entry_pad_edit_backgrounds(root);
    for (HWND current = root; current && current != GetDesktopWindow(); current = GetParent(current)) {
      if (measurements_entry_pad_subclass_for(current)) {
        continue;
      }
      const LONG_PTR current_proc = GetWindowLongPtrW(current, GWLP_WNDPROC);
      if (!current_proc) {
        continue;
      }
      SetLastError(0);
      const LONG_PTR previous_proc = SetWindowLongPtrW(current, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(measurements_entry_pad_wndproc));
      if (!previous_proc && GetLastError() != 0) {
        continue;
      }
      g_measurements_entry_pad_subclasses.push_back({current, reinterpret_cast<WNDPROC>(previous_proc ? previous_proc : current_proc)});
      installed += 1;
      RedrawWindow(current, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
  }

  return installed;
}

size_t apply_windows_dark_mode(bool enabled) {
  HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
  HMODULE uxtheme = LoadLibraryW(L"uxtheme.dll");

  using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
  using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
  using AllowDarkModeForWindowFn = BOOL(WINAPI*)(HWND, BOOL);
  using SetPreferredAppModeFn = int(WINAPI*)(int);
  using FlushMenuThemesFn = void(WINAPI*)();

  DwmSetWindowAttributeFn dwm_set_window_attribute = dwmapi ? reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwmapi, "DwmSetWindowAttribute")) : nullptr;
  SetWindowThemeFn set_window_theme = uxtheme ? reinterpret_cast<SetWindowThemeFn>(GetProcAddress(uxtheme, "SetWindowTheme")) : nullptr;
  AllowDarkModeForWindowFn allow_dark_mode_for_window = uxtheme ? reinterpret_cast<AllowDarkModeForWindowFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(133))) : nullptr;
  SetPreferredAppModeFn set_preferred_app_mode = uxtheme ? reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135))) : nullptr;
  FlushMenuThemesFn flush_menu_themes = uxtheme ? reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136))) : nullptr;


  if (!enabled && set_preferred_app_mode) {
    set_preferred_app_mode(0);
  }
  if (!enabled && flush_menu_themes) {
    flush_menu_themes();
  }

  const std::vector<HWND> hwnds = process_windows_including_descendants();
  const BOOL dark_bool = enabled ? TRUE : FALSE;
  size_t changed = 0;
  for (HWND hwnd : hwnds) {
    if (!IsWindow(hwnd) || !hwnd_is_current_process(hwnd)) {
      continue;
    }

    bool touched = false;
    if (!enabled && allow_dark_mode_for_window) {
      allow_dark_mode_for_window(hwnd, FALSE);
      touched = true;
    }
    if (!enabled && set_window_theme) {
      set_window_theme(hwnd, nullptr, nullptr);
      touched = true;
    }
    if (dwm_set_window_attribute) {
      BOOL value = dark_bool;
      HRESULT result = dwm_set_window_attribute(hwnd, 20, &value, sizeof(value));
      if (FAILED(result)) {
        result = dwm_set_window_attribute(hwnd, 19, &value, sizeof(value));
      }
      touched = touched || SUCCEEDED(result);
    }

    if (touched) {
      changed += 1;
      SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
      RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
      if (GetParent(hwnd) == nullptr) {
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
      }
    }
  }

  if (dwmapi) {
    FreeLibrary(dwmapi);
  }
  if (uxtheme) {
    FreeLibrary(uxtheme);
  }
  return changed;
}

void* qwidget_for_hwnd(const QtLookup& lookup, HWND hwnd) {
  if (!lookup.widget_find_proc || !hwnd) {
    return nullptr;
  }

  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  return reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc)(reinterpret_cast<std::uintptr_t>(hwnd));
}

void* qwidget_parent_widget(const QtLookup& lookup, void* widget) {
  if (!lookup.widget_parent_widget_proc || !widget) {
    return nullptr;
  }

  using QtPtrWidgetFn = void*(__cdecl*)(const void*);
  void* parent = nullptr;
  __try {
    parent = reinterpret_cast<QtPtrWidgetFn>(lookup.widget_parent_widget_proc)(widget);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    parent = nullptr;
  }
  return parent;
}

bool qwidget_is_self_or_descendant(const QtLookup& lookup, void* widget, void* ancestor) {
  for (int depth = 0; widget && depth < 16; ++depth) {
    if (widget == ancestor) {
      return true;
    }
    widget = qwidget_parent_widget(lookup, widget);
  }
  return false;
}

int probe_focused_qt_widget() {
  const QtLookup lookup = resolve_qt_lookup();
  if (!lookup.app_focus_widget_proc) {
    return 0;
  }

  using QtFocusWidgetFn = void*(__cdecl*)();
  void* widget = reinterpret_cast<QtFocusWidgetFn>(lookup.app_focus_widget_proc)();
  if (!widget) {
    return 0;
  }

  for (int depth = 0; widget && depth < 12; ++depth) {
    widget = qwidget_parent_widget(lookup, widget);
  }

  return 1;
}

void* qwidget_child_at(const QtLookup& lookup, void* widget, int x, int y) {
  if (!lookup.widget_child_at_proc || !widget) {
    return nullptr;
  }

  using QtWidgetChildAtFn = void*(__cdecl*)(const void*, int, int);
  void* child = nullptr;
  __try {
    child = reinterpret_cast<QtWidgetChildAtFn>(lookup.widget_child_at_proc)(widget, x, y);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    child = nullptr;
  }
  return child;
}

void* qwidget_style(const QtLookup& lookup, const void* widget) {
  if (!lookup.widget_style_proc || !widget) {
    return nullptr;
  }

  using QtWidgetStyleFn = void*(__cdecl*)(const void*);
  void* style = nullptr;
  __try {
    style = reinterpret_cast<QtWidgetStyleFn>(lookup.widget_style_proc)(widget);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    style = nullptr;
  }
  return style;
}

struct PaletteRoleColor {
  int role = 0;
  int red = 0;
  int green = 0;
  int blue = 0;
};

bool set_qpalette_role_color(const QtLookup& lookup, void* palette, const PaletteRoleColor& role_color) {
  if (!palette || !lookup.qcolor_rgb_ctor_proc || (!lookup.qpalette_set_color_group_proc && !lookup.qpalette_set_color_role_proc)) {
    return false;
  }

  using QtColorCtorFn = void(__cdecl*)(void*, int, int, int, int);
  using QtPaletteSetColorGroupFn = void(__cdecl*)(void*, int, int, const void*);
  using QtPaletteSetColorRoleFn = void(__cdecl*)(void*, int, const void*);

  alignas(16) unsigned char color_storage[64] = {};
  __try {
    reinterpret_cast<QtColorCtorFn>(lookup.qcolor_rgb_ctor_proc)(color_storage, role_color.red, role_color.green, role_color.blue, 255);
    if (lookup.qpalette_set_color_group_proc) {
      QtPaletteSetColorGroupFn set_color = reinterpret_cast<QtPaletteSetColorGroupFn>(lookup.qpalette_set_color_group_proc);
      set_color(palette, 0, role_color.role, color_storage);
      set_color(palette, 1, role_color.role, color_storage);
      set_color(palette, 2, role_color.role, color_storage);
    } else {
      reinterpret_cast<QtPaletteSetColorRoleFn>(lookup.qpalette_set_color_role_proc)(palette, role_color.role, color_storage);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }

  return true;
}

void apply_blender_palette_roles(const QtLookup& lookup, void* palette) {
  const PaletteRoleColor colors[] = {
    {0, 216, 219, 226},
    {1, 58, 58, 58},
    {2, 69, 69, 69},
    {3, 63, 63, 63},
    {4, 37, 37, 37},
    {5, 48, 48, 48},
    {6, 230, 232, 238},
    {7, 255, 255, 255},
    {8, 230, 232, 238},
    {9, 36, 36, 36},
    {10, 43, 43, 43},
    {11, 21, 21, 21},
    {12, 71, 114, 179},
    {13, 255, 255, 255},
    {14, 112, 156, 230},
    {15, 152, 128, 210},
    {16, 48, 48, 48},
    {18, 48, 48, 48},
    {19, 216, 219, 226},
    {20, 154, 160, 170},
    {21, 71, 114, 179}
  };
  for (const PaletteRoleColor& color : colors) {
    set_qpalette_role_color(lookup, palette, color);
  }
}

void collect_qwidget_descendants_from_qobject(
  const QtLookup& lookup,
  const void* object,
  int depth,
  int max_depth,
  std::vector<void*>& widgets,
  size_t limit,
  std::unordered_set<std::uintptr_t>& visited,
  const std::string& label,
  bool follow_non_widget_branches
);

bool apply_clicked_palette_to_widget(const QtLookup& lookup, void* widget, bool dark) {
  if (!widget || !lookup.widget_set_palette_proc || !lookup.qpalette_destructor_proc) {
    return false;
  }

  using QtWidgetPaletteFn = const void*(__cdecl*)(const void*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtWidgetSetRoleFn = void(__cdecl*)(void*, int);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteReturnFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  bool constructed = false;

  if (dark) {
    if (!lookup.widget_palette_proc || !lookup.qpalette_copy_ctor_proc) {
      return false;
    }
    const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
    if (!source_palette) {
      return false;
    }
    reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
    constructed = true;
  } else if (lookup.app_palette_widget_proc) {
    reinterpret_cast<QtPaletteReturnFn>(lookup.app_palette_widget_proc)(palette_storage, widget);
    constructed = true;
  } else if (lookup.widget_palette_proc && lookup.qpalette_copy_ctor_proc) {
    const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
    if (!source_palette) {
      return false;
    }
    reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
    constructed = true;
  }

  if (!constructed) {
    return false;
  }

  if (dark) {
    apply_blender_palette_roles(lookup, palette_storage);
  }

  if (lookup.widget_set_background_role_proc) {
    reinterpret_cast<QtWidgetSetRoleFn>(lookup.widget_set_background_role_proc)(widget, 10);
  }
  if (lookup.widget_set_foreground_role_proc) {
    reinterpret_cast<QtWidgetSetRoleFn>(lookup.widget_set_foreground_role_proc)(widget, 0);
  }

  reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(widget, palette_storage);

  if (constructed) {
    reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  }
  return true;
}

bool is_about_sketchup_window(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }

  wchar_t title[64] = {};
  GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
  return lstrcmpW(title, L"About SketchUp") == 0;
}

void add_unique_pointer(std::vector<void*>& pointers, void* pointer);

std::vector<void*> find_about_sketchup_content_widgets(const QtLookup& lookup, HWND hwnd, void* root_widget) {
  std::vector<void*> content_widgets;
  if (!hwnd || !root_widget) {
    return content_widgets;
  }

  RECT window_rect = {};
  if (!GetWindowRect(hwnd, &window_rect)) {
    return content_widgets;
  }

  const LONG width = std::max<LONG>(1, window_rect.right - window_rect.left);
  const LONG height = std::max<LONG>(1, window_rect.bottom - window_rect.top);
  const POINT samples[] = {
    {window_rect.left + width / 4, window_rect.top + height / 3},
    {window_rect.left + width / 2, window_rect.top + height / 3},
    {window_rect.left + (width * 3) / 4, window_rect.top + height / 3},
    {window_rect.left + width / 2, window_rect.top + height / 2}
  };

  for (const POINT& screen_point : samples) {
    POINT local_point = screen_point;
    if (!ScreenToClient(hwnd, &local_point)) {
      continue;
    }

    void* widget = qwidget_child_at(lookup, root_widget, local_point.x, local_point.y);
    for (int depth = 0; widget && widget != root_widget && depth < 12; ++depth) {
      if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QWidget")) {
        break;
      }

      void* parent = qwidget_parent_widget(lookup, widget);
      if (parent == root_widget) {
        add_unique_pointer(content_widgets, widget);
        break;
      }
      widget = parent;
    }
  }

  return content_widgets;
}

bool apply_about_sketchup_palette(bool dark) {
  if (!g_about_sketchup_window || !IsWindow(g_about_sketchup_window)) {
    g_about_sketchup_window = nullptr;
    return false;
  }

  const QtLookup lookup = resolve_qt_lookup();
  void* root_widget = qwidget_for_hwnd(lookup, g_about_sketchup_window);
  if (!root_widget || !pointer_looks_like_cpp_object(root_widget)) {
    return false;
  }

  std::vector<void*> widgets;
  std::unordered_set<std::uintptr_t> visited;
  collect_qwidget_descendants_from_qobject(
    lookup,
    root_widget,
    0,
    8,
    widgets,
    64,
    visited,
    "About SketchUp",
    true
  );

  const std::vector<void*> content_widgets = find_about_sketchup_content_widgets(lookup, g_about_sketchup_window, root_widget);
  for (void* widget : content_widgets) {
    add_unique_pointer(widgets, widget);
  }

  size_t applied_count = 0;
  for (size_t index = 0; index < widgets.size(); ++index) {
    if (apply_clicked_palette_to_widget(lookup, widgets[index], dark)) {
      applied_count += 1;
    }
  }

  if (lookup.widget_set_auto_fill_background_proc) {
    using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);
    for (void* widget : content_widgets) {
      reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(widget, dark);
    }
  }

  if (applied_count > 0 && lookup.widget_update_proc) {
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    for (void* widget : widgets) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    }
  }
  if (applied_count > 0 && lookup.widget_repaint_proc) {
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    for (void* widget : widgets) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    }
  }
  return applied_count > 0;
}

void CALLBACK about_sketchup_window_event(
  HWINEVENTHOOK,
  DWORD,
  HWND hwnd,
  LONG object_id,
  LONG child_id,
  DWORD,
  DWORD
) {
  if (object_id != OBJID_WINDOW || child_id != 0 || !is_about_sketchup_window(hwnd)) {
    return;
  }
  if (GetCurrentThreadId() != g_about_sketchup_window_hook_thread_id) {
    return;
  }

  g_about_sketchup_window = hwnd;
  const bool dark = InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) != 0;
  static_cast<void>(apply_about_sketchup_palette(dark));
  static_cast<void>(apply_windows_dark_mode(dark));
}

bool install_about_sketchup_window_hook() {
  if (g_about_sketchup_window_hook) {
    return true;
  }

  g_about_sketchup_window_hook_thread_id = GetCurrentThreadId();
  g_about_sketchup_window_hook = SetWinEventHook(
    EVENT_OBJECT_SHOW,
    EVENT_OBJECT_SHOW,
    nullptr,
    about_sketchup_window_event,
    GetCurrentProcessId(),
    0,
    WINEVENT_OUTOFCONTEXT
  );
  return g_about_sketchup_window_hook != nullptr;
}

bool apply_application_palette(const QtLookup& lookup, bool dark) {
  if (!lookup.app_palette_class_proc || !lookup.app_set_palette_proc || !lookup.qpalette_copy_ctor_proc || !lookup.qpalette_destructor_proc) {
    return false;
  }

  using QtAppPaletteClassFn = void(__cdecl*)(void*, const char*);
  using QtAppSetPaletteFn = void(__cdecl*)(const void*, const char*);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  if (dark && !g_original_app_palette_constructed) {
    reinterpret_cast<QtAppPaletteClassFn>(lookup.app_palette_class_proc)(g_original_app_palette_storage, nullptr);
    g_original_app_palette_constructed = true;
  }

  if (!dark && g_original_app_palette_constructed) {
    reinterpret_cast<QtAppSetPaletteFn>(lookup.app_set_palette_proc)(g_original_app_palette_storage, nullptr);
    reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(g_original_app_palette_storage);
    std::memset(g_original_app_palette_storage, 0, sizeof(g_original_app_palette_storage));
    g_original_app_palette_constructed = false;
    return true;
  }

  if (!dark) {
    return true;
  }

  alignas(16) unsigned char palette_storage[256] = {};
  reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, g_original_app_palette_storage);
  apply_blender_palette_roles(lookup, palette_storage);
  reinterpret_cast<QtAppSetPaletteFn>(lookup.app_set_palette_proc)(palette_storage, nullptr);
  reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  return true;
}

bool read_application_window_rgb(const QtLookup& lookup, unsigned int& rgba) {
  using QtAppPaletteClassFn = void(__cdecl*)(void*, const char*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);
  using QtPaletteColorGroupRoleFn = const void*(__cdecl*)(const void*, int, int);

  alignas(16) unsigned char palette_storage[256] = {};
  bool read = false;
  __try {
    reinterpret_cast<QtAppPaletteClassFn>(lookup.app_palette_class_proc)(palette_storage, nullptr);
    const void* window_color = reinterpret_cast<QtPaletteColorGroupRoleFn>(lookup.qpalette_color_group_role_proc)(palette_storage, 0, 10);
    if (window_color) {
      rgba = reinterpret_cast<QtColorRgbFn>(lookup.qcolor_rgb_proc)(window_color);
      read = true;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    read = false;
  }
  reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  return read;
}

bool application_palette_is_dark() {
  const QtLookup lookup = resolve_qt_lookup();
  if (!lookup.app_palette_class_proc || !lookup.qpalette_destructor_proc || !lookup.qpalette_color_group_role_proc || !lookup.qcolor_rgb_proc) {
    return false;
  }

  unsigned int rgba = 0;
  if (!read_application_window_rgb(lookup, rgba)) {
    return false;
  }
  const unsigned int red = (rgba >> 16) & 0xff;
  const unsigned int green = (rgba >> 8) & 0xff;
  const unsigned int blue = rgba & 0xff;
  const bool dark = red <= 96 && green <= 96 && blue <= 96;
  return dark;
}

void add_unique_pointer(std::vector<void*>& pointers, void* pointer) {
  if (pointer && std::find(pointers.begin(), pointers.end(), pointer) == pointers.end()) {
    pointers.push_back(pointer);
  }
}

void collect_qwidget_descendants_from_qobject(const QtLookup& lookup, const void* object, int depth, int max_depth, std::vector<void*>& widgets, size_t limit, std::unordered_set<std::uintptr_t>& visited, const std::string& label, bool follow_non_widget_branches = false) {
  if (!object || widgets.size() >= limit || depth > max_depth || !pointer_looks_like_cpp_object(object)) {
    return;
  }

  const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(object);
  if (!visited.insert(key).second) {
    return;
  }

  const bool is_widget = qt_object_inherits(lookup, object, "QWidget");
  if (is_widget) {
    add_unique_pointer(widgets, const_cast<void*>(object));
  }
  if ((!is_widget && !follow_non_widget_branches) || !lookup.object_children_proc || depth >= max_depth || widgets.size() >= limit) {
    return;
  }

  const void* children_list = nullptr;
  if (!try_qobject_children_list(lookup.object_children_proc, object, &children_list)) {
    return;
  }
  QtPointerListView children = decode_qt_pointer_list(children_list);

  const size_t child_count = std::min<size_t>(static_cast<size_t>(std::max<long long>(0, children.size)), 32);
  for (size_t index = 0; index < child_count && widgets.size() < limit; ++index) {
    void* child = children.items ? children.items[index] : nullptr;
    if (!child || !pointer_looks_like_cpp_object(child)) {
      continue;
    }
    if (follow_non_widget_branches || qt_object_inherits(lookup, child, "QWidget")) {
      collect_qwidget_descendants_from_qobject(lookup, child, depth + 1, max_depth, widgets, limit, visited, label, follow_non_widget_branches);
    } else {
    }
  }
}

void collect_qwidget_descendants_quiet(const QtLookup& lookup, const void* object, int depth, int max_depth, std::vector<void*>& widgets, size_t limit, std::unordered_set<std::uintptr_t>& visited) {
  if (!object || widgets.size() >= limit || depth > max_depth || !pointer_looks_like_cpp_object(object)) {
    return;
  }

  const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(object);
  if (!visited.insert(key).second) {
    return;
  }

  if (qt_object_inherits(lookup, object, "QWidget")) {
    add_unique_pointer(widgets, const_cast<void*>(object));
  }
  if (!lookup.object_children_proc || depth >= max_depth || widgets.size() >= limit) {
    return;
  }

  const void* children_list = nullptr;
  if (!try_qobject_children_list(lookup.object_children_proc, object, &children_list)) {
    return;
  }
  const QtPointerListView children = decode_qt_pointer_list(children_list);
  const size_t child_count = std::min<size_t>(static_cast<size_t>(std::max<long long>(0, children.size)), 32);
  for (size_t index = 0; index < child_count && widgets.size() < limit; ++index) {
    void* child = children.items ? children.items[index] : nullptr;
    if (child && pointer_looks_like_cpp_object(child)) {
      collect_qwidget_descendants_quiet(lookup, child, depth + 1, max_depth, widgets, limit, visited);
    }
  }
}

std::vector<void*> collect_tray_qobject_qwidgets(const QtLookup& lookup, int stage, bool follow_non_widget_branches = false) {
  std::vector<void*> widgets;
  if (!lookup.widget_find_proc || !lookup.object_children_proc) {
    return widgets;
  }

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  for (size_t root_index = 0; root_index < tray_roots.size() && root_index < 4; ++root_index) {
    HWND tray_root = tray_roots[root_index];
    void* tray_widget = qwidget_for_hwnd(lookup, tray_root);
    if (!tray_widget || !pointer_looks_like_cpp_object(tray_widget)) {
      continue;
    }
    std::unordered_set<std::uintptr_t> visited;
    collect_qwidget_descendants_from_qobject(lookup, tray_widget, 0, follow_non_widget_branches ? 7 : 5, widgets, follow_non_widget_branches ? 160 : 80, visited, "stage " + std::to_string(stage) + " tray-qss", follow_non_widget_branches);
  }
  return widgets;
}

size_t dump_tray_branch_children(const QtLookup& lookup, int stage, bool include_qobject_children, int qobject_max_depth = 1, bool follow_widget_children_only = false) {
  const std::string label = "stage " + std::to_string(stage);
  if (!lookup.widget_find_proc) {
    return 0;
  }

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  size_t dumped_count = 0;

  for (size_t root_index = 0; root_index < tray_roots.size() && root_index < 4; ++root_index) {
    HWND tray_root = tray_roots[root_index];

    size_t ancestor_index = 0;
    for (HWND current = tray_root; current && ancestor_index < 16; current = GetParent(current), ++ancestor_index) {
      void* widget = qwidget_for_hwnd(lookup, current);
      if (widget && pointer_looks_like_cpp_object(widget)) {
        dumped_count += 1;
      }
    }

    const std::vector<HWND> branch_hwnds = hwnd_descendants_including_self(tray_root);
    const size_t native_limit = std::min<size_t>(branch_hwnds.size(), 120);
    for (size_t index = 0; index < native_limit; ++index) {
      HWND hwnd = branch_hwnds[index];
      void* widget = qwidget_for_hwnd(lookup, hwnd);
      if (widget && pointer_looks_like_cpp_object(widget)) {
        dumped_count += 1;
      }
    }
    if (branch_hwnds.size() > native_limit) {
    }

    void* tray_widget = qwidget_for_hwnd(lookup, tray_root);
    if (include_qobject_children && tray_widget && pointer_looks_like_cpp_object(tray_widget)) {
      size_t child_count = 0;
      std::unordered_set<std::uintptr_t> visited;
      dump_qobject_children_limited(lookup, tray_widget, label + " tray-qobject", 0, qobject_max_depth, child_count, 160, visited, follow_widget_children_only);
      dumped_count += child_count;
    }
  }

  return dumped_count;
}

std::string color_hex(COLORREF color) {
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", static_cast<unsigned int>(GetRValue(color)), static_cast<unsigned int>(GetGValue(color)), static_cast<unsigned int>(GetBValue(color)));
  return buffer;
}

int color_luma(COLORREF color) {
  return (static_cast<int>(GetRValue(color)) * 299 + static_cast<int>(GetGValue(color)) * 587 + static_cast<int>(GetBValue(color)) * 114) / 1000;
}

bool is_tray_mismatch_color(COLORREF color) {
  const int red = static_cast<int>(GetRValue(color));
  const int green = static_cast<int>(GetGValue(color));
  const int blue = static_cast<int>(GetBValue(color));
  const int luma = color_luma(color);
  const int max_channel = std::max(red, std::max(green, blue));
  const int min_channel = std::min(red, std::min(green, blue));
  const int spread = max_channel - min_channel;
  const bool light_gray = luma >= 120 && spread <= 80;
  const bool blue_gray = luma >= 85 && blue >= red && blue >= green - 8 && spread <= 55;
  return light_gray || blue_gray;
}

struct LightPixelStats {
  HWND hwnd = nullptr;
  int samples = 0;
  int light = 0;
  int sum_red = 0;
  int sum_green = 0;
  int sum_blue = 0;
};

LightPixelStats& stats_for_hwnd(std::vector<LightPixelStats>& stats, HWND hwnd) {
  auto existing = std::find_if(stats.begin(), stats.end(), [hwnd](const LightPixelStats& stat) {
    return stat.hwnd == hwnd;
  });
  if (existing != stats.end()) {
    return *existing;
  }

  LightPixelStats stat;
  stat.hwnd = hwnd;
  stats.push_back(stat);
  return stats.back();
}

std::vector<HWND> collect_right_side_tray_mismatch_hwnds(std::vector<HWND>& leaf_hwnds) {
  std::vector<HWND> candidate_hwnds;
  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();
  if (candidates.empty()) {
    return candidate_hwnds;
  }

  HWND main_hwnd = nullptr;
  for (const NativeWindowCandidate& candidate : candidates) {
    if (lower_ascii(candidate.title).find("sketchup") != std::string::npos) {
      main_hwnd = candidate.hwnd;
      break;
    }
  }
  if (!main_hwnd) {
    main_hwnd = candidates.front().hwnd;
  }

  RECT main_rect = {};
  if (!GetWindowRect(main_hwnd, &main_rect)) {
    return candidate_hwnds;
  }

  const LONG width = std::max<LONG>(0, main_rect.right - main_rect.left);
  const LONG height = std::max<LONG>(0, main_rect.bottom - main_rect.top);
  if (width < 300 || height < 300) {
    return candidate_hwnds;
  }

  HDC screen_dc = GetDC(nullptr);
  if (!screen_dc) {
    return candidate_hwnds;
  }

  std::vector<LightPixelStats> stats;
  const LONG scan_width = std::min<LONG>(560, std::max<LONG>(240, width / 3));
  const LONG left = std::max<LONG>(main_rect.left, main_rect.right - scan_width);
  const LONG top = std::min<LONG>(main_rect.bottom, main_rect.top + 90);
  const int step = 24;
  const int max_samples = 1200;
  int samples = 0;
  int skipped = 0;

  for (LONG y = top; y < main_rect.bottom && samples < max_samples; y += step) {
    for (LONG x = left; x < main_rect.right && samples < max_samples; x += step) {
      COLORREF color = GetPixel(screen_dc, x, y);
      if (color == CLR_INVALID) {
        continue;
      }

      samples += 1;
      if (!is_tray_mismatch_color(color)) {
        continue;
      }

      POINT point = {x, y};
      HWND hit = WindowFromPoint(point);
      if (!hit || hit == main_hwnd || !hwnd_is_current_process(hit) || !hwnd_is_self_or_descendant(main_hwnd, hit) || hwnd_looks_like_non_tray_surface(hit)) {
        skipped += 1;
        continue;
      }

      LightPixelStats& stat = stats_for_hwnd(stats, hit);
      stat.samples += 1;
      stat.light += 1;
      stat.sum_red += static_cast<int>(GetRValue(color));
      stat.sum_green += static_cast<int>(GetGValue(color));
      stat.sum_blue += static_cast<int>(GetBValue(color));
    }
  }
  ReleaseDC(nullptr, screen_dc);


  std::sort(stats.begin(), stats.end(), [](const LightPixelStats& left_stat, const LightPixelStats& right_stat) {
    if (left_stat.light != right_stat.light) {
      return left_stat.light > right_stat.light;
    }
    return left_stat.samples > right_stat.samples;
  });

  const size_t limit = std::min<size_t>(stats.size(), 4);
  for (size_t index = 0; index < limit; ++index) {
    if (stats[index].light < 3) {
      continue;
    }

    add_unique_hwnd(leaf_hwnds, stats[index].hwnd);
    for (HWND current = stats[index].hwnd; current && current != main_hwnd; current = GetParent(current)) {
      if (!hwnd_looks_like_non_tray_surface(current)) {
        add_unique_hwnd(candidate_hwnds, current);
      }
    }
  }

  return candidate_hwnds;
}

size_t scan_tray_light_pixels() {
  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  if (tray_roots.empty()) {
    return 0;
  }

  HDC screen_dc = GetDC(nullptr);
  if (!screen_dc) {
    return tray_roots.size();
  }

  size_t logged_light_points = 0;
  size_t total_samples = 0;
  for (size_t root_index = 0; root_index < tray_roots.size(); ++root_index) {
    HWND tray_root = tray_roots[root_index];
    RECT tray_rect = {};
    if (!GetWindowRect(tray_root, &tray_rect)) {
      continue;
    }

    std::vector<LightPixelStats> stats;
    const int step = 24;
    const int max_samples_per_root = 900;
    int root_samples = 0;
    int skipped_non_tray_hits = 0;
    for (LONG y = tray_rect.top; y < tray_rect.bottom && root_samples < max_samples_per_root; y += step) {
      for (LONG x = tray_rect.left; x < tray_rect.right && root_samples < max_samples_per_root; x += step) {
        COLORREF color = GetPixel(screen_dc, x, y);
        if (color == CLR_INVALID) {
          continue;
        }

        root_samples += 1;
        total_samples += 1;
        if (!is_tray_mismatch_color(color)) {
          continue;
        }

        POINT point = {x, y};
        HWND hit = WindowFromPoint(point);
        if (!hwnd_is_self_or_descendant(tray_root, hit)) {
          skipped_non_tray_hits += 1;
          continue;
        }

        LightPixelStats& stat = stats_for_hwnd(stats, hit);
        stat.samples += 1;
        stat.light += 1;
        stat.sum_red += static_cast<int>(GetRValue(color));
        stat.sum_green += static_cast<int>(GetGValue(color));
        stat.sum_blue += static_cast<int>(GetBValue(color));

        if (logged_light_points < 24) {
          logged_light_points += 1;
        }
      }
    }

    std::sort(stats.begin(), stats.end(), [](const LightPixelStats& left, const LightPixelStats& right) {
      if (left.light != right.light) {
        return left.light > right.light;
      }
      return left.samples > right.samples;
    });

    const size_t limit = std::min<size_t>(stats.size(), 30);
    for (size_t index = 0; index < limit; ++index) {
      if (stats[index].light <= 0) {
        break;
      }
      HWND parent = GetParent(stats[index].hwnd);
      for (int depth = 1; parent && depth <= 4; ++depth, parent = GetParent(parent)) {
      }
    }
  }

  ReleaseDC(nullptr, screen_dc);
  return total_samples;
}

size_t dump_cursor_window_after_delay() {
  Sleep(3000);

  POINT point = {};
  if (!GetCursorPos(&point)) {
    return 0;
  }

  HWND hwnd = WindowFromPoint(point);
  size_t count = 0;
  for (HWND current = hwnd; current && count < 24; current = GetParent(current)) {
    count += 1;
  }
  return count;
}

bool left_mouse_button_down() {
  return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

bool wait_for_next_left_click(POINT& point, DWORD timeout_ms) {
  const DWORD release_start = GetTickCount();
  while (left_mouse_button_down() && GetTickCount() - release_start < 2500) {
    Sleep(20);
  }

  const DWORD wait_start = GetTickCount();
  bool was_down = false;
  while (GetTickCount() - wait_start < timeout_ms) {
    const bool is_down = left_mouse_button_down();
    if (is_down && !was_down) {
      if (GetCursorPos(&point)) {
        return true;
      }
      return false;
    }
    was_down = is_down;
    Sleep(15);
  }
  return false;
}

size_t dump_click_window_owner(const QtLookup& lookup) {
  POINT point = {};
  if (!wait_for_next_left_click(point, 10000)) {
    return 0;
  }

  HWND hit = WindowFromPoint(point);

  size_t count = 0;
  HWND nearest_qt_hwnd = nullptr;
  void* nearest_qt_widget = nullptr;
  for (HWND current = hit; current && count < 32; current = GetParent(current)) {
    if (!nearest_qt_widget) {
      void* widget = qwidget_for_hwnd(lookup, current);
      if (widget && pointer_looks_like_cpp_object(widget)) {
        nearest_qt_hwnd = current;
        nearest_qt_widget = widget;
      }
    }
    count += 1;
  }

  if (nearest_qt_widget) {
  } else {
  }

  return count;
}

std::vector<void*> collect_clicked_qwidget_ancestry(const QtLookup& lookup, POINT point) {
  std::vector<void*> targets;
  HWND hit = WindowFromPoint(point);

  size_t count = 0;
  for (HWND current = hit; current && count < 32; current = GetParent(current), ++count) {
    void* widget = qwidget_for_hwnd(lookup, current);
    if (widget && pointer_looks_like_cpp_object(widget) && qt_object_inherits(lookup, widget, "QWidget")) {
      add_unique_pointer(targets, widget);
    }
  }
  return targets;
}

std::vector<void*> collect_clicked_childat_qwidgets(const QtLookup& lookup, POINT point) {
  std::vector<void*> targets;
  HWND hit = WindowFromPoint(point);

  size_t count = 0;
  for (HWND current = hit; current && count < 32; current = GetParent(current), ++count) {
    void* widget = qwidget_for_hwnd(lookup, current);
    POINT local = point;
    ScreenToClient(current, &local);
    if (!widget || !pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QWidget")) {
      continue;
    }

    void* child = qwidget_child_at(lookup, widget, local.x, local.y);
    if (child && pointer_looks_like_cpp_object(child)) {
      if (qt_object_inherits(lookup, child, "QWidget")) {
        add_unique_pointer(targets, child);
      }

      void* parent = qwidget_parent_widget(lookup, child);
      for (int depth = 1; parent && depth <= 12; ++depth) {
        if (pointer_looks_like_cpp_object(parent) && qt_object_inherits(lookup, parent, "QWidget")) {
          add_unique_pointer(targets, parent);
        }
        parent = qwidget_parent_widget(lookup, parent);
      }
    } else {
    }

    add_unique_pointer(targets, widget);
  }

  return targets;
}

int probe_dimensions_cursor_widget() {
  const QtLookup lookup = resolve_qt_lookup();
  POINT point = {};
  if (!GetCursorPos(&point)) {
    return 0;
  }

  const std::vector<void*> targets = collect_clicked_childat_qwidgets(lookup, point);
  const bool dark = InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) != 0;
  size_t header_applied = 0;
  size_t viewport_applied = 0;
  if (dark && lookup.from_utf8_proc && lookup.widget_set_stylesheet_proc && lookup.qstring_destructor_proc) {
    using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
    using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
    using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
    using QtDestructorFn = void(__cdecl*)(void*);
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);

    const char* header_stylesheet = "QHeaderView { background-color: #2b2b2b; color: #d8dbe2; } "
      "QHeaderView::section { background-color: #2b2b2b; color: #d8dbe2; border: none; "
      "border-right: 1px solid #3a3a3a; border-bottom: 1px solid #3a3a3a; }";
    const char* viewport_stylesheet = "QWidget { background-color: #2b2b2b; color: #d8dbe2; }";
    alignas(16) unsigned char header_qstring_storage[256] = {};
    alignas(16) unsigned char viewport_qstring_storage[256] = {};
    const size_t header_stylesheet_length = std::strlen(header_stylesheet);
    const size_t viewport_stylesheet_length = std::strlen(viewport_stylesheet);
    if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
      reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(header_qstring_storage, header_stylesheet, static_cast<long long>(header_stylesheet_length));
      reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(viewport_qstring_storage, viewport_stylesheet, static_cast<long long>(viewport_stylesheet_length));
    } else {
      reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(header_qstring_storage, header_stylesheet, static_cast<int>(header_stylesheet_length));
      reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(viewport_qstring_storage, viewport_stylesheet, static_cast<int>(viewport_stylesheet_length));
    }

    for (void* widget : targets) {
      const bool header = qt_object_inherits(lookup, widget, "QHeaderView");
      void* parent = header ? nullptr : qwidget_parent_widget(lookup, widget);
      const bool viewport = parent && qt_object_inherits(lookup, parent, "QHeaderView");
      if (!header && !viewport) {
        continue;
      }
      reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(widget, header ? header_qstring_storage : viewport_qstring_storage);
      const std::string retained_stylesheet = qt_string_return_to_utf8(lookup, lookup.widget_stylesheet_proc, widget);
      static_cast<void>(qwidget_style(lookup, widget));
      apply_clicked_palette_to_widget(lookup, widget, true);
      if (viewport && lookup.widget_set_auto_fill_background_proc) {
        reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(widget, true);
      }
      if (lookup.widget_update_proc) {
        reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
      }
      if (lookup.widget_repaint_proc) {
        reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
      }
      if (header) {
        header_applied += 1;
      } else {
        viewport_applied += 1;
      }
    }
    reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(header_qstring_storage);
    reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(viewport_qstring_storage);
  }

  return targets.empty() ? 0 : 1;
}

const char* probe_stage_name(int stage) {
  switch (stage) {
    case 0: return "symbols";
    case 1: return "instance";
    case 2: return "qstring";
    case 3: return "clear_stylesheet";
    case 4: return "object_inherits";
    case 5: return "active_window";
    case 6: return "tiny_active_window_stylesheet";
    case 7: return "danger_noop_app_stylesheet";
    case 8: return "danger_tiny_app_stylesheet";
    case 9: return "danger_dark_app_stylesheet";
    case 10: return "dark_active_window_stylesheet";
    case 11: return "clear_active_window_stylesheet";
    case 12: return "main_hwnd_qwidget_find";
    case 13: return "dark_main_hwnd_stylesheet";
    case 14: return "clear_main_hwnd_stylesheet";
    case 15: return "dump_main_hwnd_children";
    case 16: return "scan_tray_light_pixels";
    case 17: return "cursor_hwnd_after_delay";
    case 18: return "dark_tray_mismatch_owners";
    case 19: return "force_tray_mismatch_fill";
    case 20: return "native_fill_tray_mismatch_hwnds";
    case 21: return "install_tray_erase_subclass";
    case 22: return "remove_tray_erase_subclass";
    case 23: return "install_tray_postpaint_scrub";
    case 24: return "install_tray_buffered_scrub";
    case 25: return "install_tray_leaf_buffered_scrub";
    case 26: return "deep_qt_widget_inventory_dump";
    case 27: return "tray_branch_children_dump";
    case 28: return "tray_qobject_children_raw_dump";
    case 29: return "tray_qwidget_children_deep_dump";
    case 30: return "tray_qwidget_children_dark";
    case 31: return "tray_qwidget_children_clear";
    case 32: return "tray_qobject_all_deep_dump";
    case 33: return "tray_qobject_all_deep_dark";
    case 34: return "tray_qobject_all_deep_clear";
    case 35: return "click_window_owner_inspector";
    case 36: return "click_qwidget_ancestry_dark";
    case 37: return "click_qwidget_ancestry_clear";
    case 38: return "click_childat_dump";
    case 39: return "click_childat_forced_dark";
    case 40: return "click_childat_forced_clear";
    case 41: return "click_childat_delegate_dark";
    case 42: return "click_childat_delegate_clear";
    case 43: return "click_childat_palette_dark";
    case 44: return "click_childat_palette_clear";
    case 45: return "auto_palette_dark";
    case 46: return "auto_palette_clear";
    case 47: return "windows_dark_mode_on";
    case 48: return "windows_dark_mode_off";
    default: return "unknown";
  }
}

void* find_main_window_qwidget(const QtLookup& lookup, std::string& description) {
  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  if (!lookup.widget_find_proc) {
    description = "QWidget::find symbol missing. " + describe_qt_lookup(lookup);
    return nullptr;
  }

  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();
  description = "candidates=" + std::to_string(candidates.size());
  QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc);
  for (const NativeWindowCandidate& candidate : candidates) {
    void* widget = widget_find(reinterpret_cast<std::uintptr_t>(candidate.hwnd));
    if (widget) {
      const std::string inherits = qobject_inherits_summary(lookup, widget);
      description = describe_native_window(candidate) + " widget=" + pointer_hex(widget) + " " + inherits;
      return widget;
    }
  }

  return nullptr;
}

struct WidgetStyleTarget {
  void* widget = nullptr;
  std::string description;
};

void add_unique_style_target(std::vector<WidgetStyleTarget>& targets, void* widget, const std::string& description) {
  if (!widget) {
    return;
  }

  const auto existing = std::find_if(targets.begin(), targets.end(), [widget](const WidgetStyleTarget& target) {
    return target.widget == widget;
  });
  if (existing == targets.end()) {
    targets.push_back({widget, description});
  }
}

void add_viewport_ancestor_qwidgets(const QtLookup& lookup, std::vector<void*>& targets) {
  const std::vector<HWND> viewport_hwnds = find_viewport_marker_hwnds();

  for (size_t root_index = 0; root_index < viewport_hwnds.size() && root_index < 8; ++root_index) {
    HWND viewport_hwnd = viewport_hwnds[root_index];

    size_t ancestor_index = 0;
    for (HWND current = viewport_hwnd; current && ancestor_index < 16; current = GetParent(current), ++ancestor_index) {
      if (!hwnd_is_current_process(current)) {
        continue;
      }

      void* widget = qwidget_for_hwnd(lookup, current);
      if (widget && pointer_looks_like_cpp_object(widget) && qt_object_inherits(lookup, widget, "QWidget")) {
        add_unique_pointer(targets, widget);
      }
    }
  }
}

struct TrayStyleTargetContext {
  const QtLookup* lookup = nullptr;
  std::vector<WidgetStyleTarget>* targets = nullptr;
};

bool text_looks_like_tray_marker(const std::string& title, const std::string& class_name) {
  const std::string lowered_title = lower_ascii(title);
  const std::string lowered_class = lower_ascii(class_name);
  return lowered_title.find("tray") != std::string::npos ||
    lowered_class.find("toolsavebits") != std::string::npos;
}

BOOL CALLBACK collect_tray_style_targets_proc(HWND hwnd, LPARAM lparam) {
  auto* context = reinterpret_cast<TrayStyleTargetContext*>(lparam);
  if (!context || !context->lookup || !context->targets || !context->lookup->widget_find_proc) {
    return TRUE;
  }

  const std::string title = hwnd_text(hwnd);
  const std::string class_name = hwnd_class_name(hwnd);
  if (!text_looks_like_tray_marker(title, class_name)) {
    return TRUE;
  }

  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(context->lookup->widget_find_proc);
  void* widget = widget_find(reinterpret_cast<std::uintptr_t>(hwnd));
  const std::string description = "tray-child " + describe_native_window({hwnd, title, class_name, 0}) +
    describe_hwnd_rect(hwnd) + " widget=" + pointer_hex(widget) +
    (widget ? " qt_class=" + qobject_class_name_quiet(*context->lookup, widget) : "");
  add_unique_style_target(*context->targets, widget, description);
  return TRUE;
}

std::vector<WidgetStyleTarget> collect_main_and_tray_style_targets(const QtLookup& lookup) {
  std::vector<WidgetStyleTarget> targets;
  std::string main_widget_description;
  void* main_widget = find_main_window_qwidget(lookup, main_widget_description);
  add_unique_style_target(targets, main_widget, "main-window " + main_widget_description);

  const std::vector<NativeWindowCandidate> candidates = process_top_level_windows();
  TrayStyleTargetContext context;
  context.lookup = &lookup;
  context.targets = &targets;
  for (size_t index = 0; index < candidates.size() && index < 12; ++index) {
    EnumChildWindows(candidates[index].hwnd, collect_tray_style_targets_proc, reinterpret_cast<LPARAM>(&context));
  }

  for (size_t index = 0; index < targets.size(); ++index) {
  }
  return targets;
}

std::vector<void*> collect_main_and_tray_top_level_qwidgets(const QtLookup& lookup) {
  std::vector<void*> top_level_widgets;

  std::string main_desc;
  void* main_widget = find_main_window_qwidget(lookup, main_desc);
  add_unique_pointer(top_level_widgets, main_widget);

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  for (HWND tray_root : tray_roots) {
    void* widget = qwidget_for_hwnd(lookup, tray_root);
    add_unique_pointer(top_level_widgets, widget);
  }

  const std::vector<NativeWindowCandidate> top_level = process_top_level_windows();
  for (const NativeWindowCandidate& candidate : top_level) {
    void* widget = qwidget_for_hwnd(lookup, candidate.hwnd);
    add_unique_pointer(top_level_widgets, widget);
  }

  add_viewport_ancestor_qwidgets(lookup, top_level_widgets);

  return top_level_widgets;
}

void add_cached_tray_mismatch_style_targets(const QtLookup& lookup, std::vector<WidgetStyleTarget>& targets) {
  if (!lookup.widget_find_proc || g_tray_mismatch_style_hwnds.empty()) {
    return;
  }

  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc);
  for (HWND hwnd : g_tray_mismatch_style_hwnds) {
    if (!IsWindow(hwnd)) {
      continue;
    }

    void* widget = widget_find(reinterpret_cast<std::uintptr_t>(hwnd));
    add_unique_style_target(targets, widget, "cached-tray-mismatch " + describe_hwnd_for_probe(lookup, hwnd));
  }
}

std::vector<WidgetStyleTarget> collect_tray_mismatch_style_targets(const QtLookup& lookup) {
  std::vector<WidgetStyleTarget> targets;
  g_tray_mismatch_style_hwnds.clear();
  g_tray_mismatch_leaf_hwnds.clear();
  if (!lookup.widget_find_proc) {
    return targets;
  }

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  if (tray_roots.empty()) {
    std::vector<HWND> fallback_hwnds = collect_right_side_tray_mismatch_hwnds(g_tray_mismatch_leaf_hwnds);
    g_tray_mismatch_style_hwnds = fallback_hwnds;

    using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
    QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc);
    for (HWND hwnd : fallback_hwnds) {
      void* widget = widget_find(reinterpret_cast<std::uintptr_t>(hwnd));
      add_unique_style_target(targets, widget, "fallback-tray-mismatch-owner " + describe_hwnd_for_probe(lookup, hwnd));
    }

    for (size_t index = 0; index < targets.size(); ++index) {
    }
    return targets;
  }

  HDC screen_dc = GetDC(nullptr);
  if (!screen_dc) {
    return targets;
  }

  std::vector<HWND> candidate_hwnds;
  for (size_t root_index = 0; root_index < tray_roots.size(); ++root_index) {
    HWND tray_root = tray_roots[root_index];
    RECT tray_rect = {};
    if (!GetWindowRect(tray_root, &tray_rect)) {
      continue;
    }

    std::vector<LightPixelStats> stats;
    const int step = 24;
    const int max_samples_per_root = 900;
    int root_samples = 0;
    int skipped_non_tray_hits = 0;
    for (LONG y = tray_rect.top; y < tray_rect.bottom && root_samples < max_samples_per_root; y += step) {
      for (LONG x = tray_rect.left; x < tray_rect.right && root_samples < max_samples_per_root; x += step) {
        COLORREF color = GetPixel(screen_dc, x, y);
        if (color == CLR_INVALID) {
          continue;
        }

        root_samples += 1;
        if (!is_tray_mismatch_color(color)) {
          continue;
        }

        POINT point = {x, y};
        HWND hit = WindowFromPoint(point);
        if (!hwnd_is_self_or_descendant(tray_root, hit)) {
          skipped_non_tray_hits += 1;
          continue;
        }

        LightPixelStats& stat = stats_for_hwnd(stats, hit);
        stat.samples += 1;
        stat.light += 1;
        stat.sum_red += static_cast<int>(GetRValue(color));
        stat.sum_green += static_cast<int>(GetGValue(color));
        stat.sum_blue += static_cast<int>(GetBValue(color));
      }
    }

    std::sort(stats.begin(), stats.end(), [](const LightPixelStats& left, const LightPixelStats& right) {
      if (left.light != right.light) {
        return left.light > right.light;
      }
      return left.samples > right.samples;
    });

    const size_t limit = std::min<size_t>(stats.size(), 3);
    for (size_t index = 0; index < limit; ++index) {
      if (stats[index].light <= 0) {
        break;
      }

      add_unique_hwnd(g_tray_mismatch_leaf_hwnds, stats[index].hwnd);
      for (HWND current = stats[index].hwnd; current; current = GetParent(current)) {
        add_unique_hwnd(candidate_hwnds, current);
        if (current == tray_root) {
          break;
        }
      }
    }
  }

  ReleaseDC(nullptr, screen_dc);
  g_tray_mismatch_style_hwnds = candidate_hwnds;

  using QtWidgetFindFn = void*(__cdecl*)(std::uintptr_t);
  QtWidgetFindFn widget_find = reinterpret_cast<QtWidgetFindFn>(lookup.widget_find_proc);
  for (HWND hwnd : candidate_hwnds) {
    void* widget = widget_find(reinterpret_cast<std::uintptr_t>(hwnd));
    add_unique_style_target(targets, widget, "tray-mismatch-owner " + describe_hwnd_for_probe(lookup, hwnd));
  }

  for (size_t index = 0; index < targets.size(); ++index) {
  }
  return targets;
}

std::vector<void*> collect_visible_tray_header_views(const QtLookup& lookup) {
  std::vector<void*> headers;
  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  for (HWND tray_root : tray_roots) {
    RECT rect = {};
    if (!GetWindowRect(tray_root, &rect)) {
      continue;
    }

    const LONG width = std::max<LONG>(1, rect.right - rect.left);
    const LONG height = std::max<LONG>(1, rect.bottom - rect.top);
    const LONG left = rect.left + std::min<LONG>(16, std::max<LONG>(0, width - 1));
    const LONG right = rect.right - std::min<LONG>(16, std::max<LONG>(0, width - 1));
    const LONG top = rect.top + std::min<LONG>(16, std::max<LONG>(0, height - 1));
    const LONG bottom = rect.bottom - std::min<LONG>(16, std::max<LONG>(0, height - 1));
    if (left >= right || top >= bottom) {
      continue;
    }

    const LONG sample_x[] = {
      left + (right - left) / 4,
      left + (right - left) / 2,
      left + ((right - left) * 3) / 4
    };
    bool found_header = false;
    for (LONG y = top; y < bottom && !found_header; y += 24) {
      for (LONG x : sample_x) {
        POINT screen_point = {x, y};
        for (HWND current = tray_root; current; current = GetParent(current)) {
          void* root_widget = qwidget_for_hwnd(lookup, current);
          if (!root_widget || !pointer_looks_like_cpp_object(root_widget) || !qt_object_inherits(lookup, root_widget, "QWidget")) {
            continue;
          }

          POINT local_point = screen_point;
          if (!ScreenToClient(current, &local_point)) {
            continue;
          }

          void* widget = qwidget_child_at(lookup, root_widget, local_point.x, local_point.y);
          for (int depth = 0; widget && depth < 12; ++depth) {
            if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QWidget")) {
              break;
            }
            if (qt_object_inherits(lookup, widget, "QHeaderView")) {
              add_unique_pointer(headers, widget);
              found_header = true;
              break;
            }
            widget = qwidget_parent_widget(lookup, widget);
          }
          if (found_header) {
            break;
          }
        }
        if (found_header) {
          break;
        }
      }
    }
  }

  return headers;
}

std::vector<void*> collect_auto_palette_targets(const QtLookup& lookup, int stage) {
  std::vector<void*> targets;

  const std::vector<WidgetStyleTarget> main_targets = collect_main_and_tray_style_targets(lookup);
  for (const WidgetStyleTarget& target : main_targets) {
    add_unique_pointer(targets, target.widget);
  }

  add_viewport_ancestor_qwidgets(lookup, targets);

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  for (size_t root_index = 0; root_index < tray_roots.size() && root_index < 4; ++root_index) {
    HWND tray_root = tray_roots[root_index];

    for (HWND current = tray_root; current; current = GetParent(current)) {
      void* widget = qwidget_for_hwnd(lookup, current);
      if (widget && pointer_looks_like_cpp_object(widget) && qt_object_inherits(lookup, widget, "QWidget")) {
        add_unique_pointer(targets, widget);
      }
    }

    RECT rect = {};
    if (!GetWindowRect(tray_root, &rect)) {
      continue;
    }

    const LONG width = std::max<LONG>(1, rect.right - rect.left);
    const LONG height = std::max<LONG>(1, rect.bottom - rect.top);
    const LONG sample_x = rect.left + std::min<LONG>(std::max<LONG>(24, width / 4), width - 8);
    const LONG upper_y = rect.top + std::min<LONG>(std::max<LONG>(72, height / 5), height - 8);
    const LONG middle_y = rect.top + std::min<LONG>(std::max<LONG>(96, height / 2), height - 8);
    const LONG lower_y = rect.top + std::min<LONG>(std::max<LONG>(120, height - 48), height - 8);
    const POINT samples[] = {
      {sample_x, upper_y},
      {sample_x, middle_y},
      {sample_x, lower_y}
    };

    for (const POINT& sample : samples) {
      if (sample.x < rect.left || sample.x >= rect.right || sample.y < rect.top || sample.y >= rect.bottom) {
        continue;
      }
      std::vector<void*> sample_targets = collect_clicked_childat_qwidgets(lookup, sample);
      for (void* widget : sample_targets) {
        add_unique_pointer(targets, widget);
      }
    }
  }

  if (lookup.object_children_proc) {
    std::vector<void*> tray_widgets = collect_tray_qobject_qwidgets(lookup, stage, true);
    for (void* widget : tray_widgets) {
      add_unique_pointer(targets, widget);
    }
  }

  const std::vector<void*> header_widgets = collect_visible_tray_header_views(lookup);
  for (void* widget : header_widgets) {
    add_unique_pointer(targets, widget);
  }

  for (size_t index = 0; index < targets.size() && index < 80; ++index) {
  }
  return targets;
}

std::uint32_t read_unaligned_u32(const unsigned char* address) {
  std::uint32_t value = 0;
  std::memcpy(&value, address, sizeof(value));
  return value;
}

std::uintptr_t read_unaligned_pointer(const unsigned char* address) {
  std::uintptr_t value = 0;
  std::memcpy(&value, address, sizeof(value));
  return value;
}

std::string msvc_rtti_type_name(const void* object) {
  if (!object || !memory_readable(object, sizeof(void*))) {
    return {};
  }
  const auto* vtable = reinterpret_cast<const unsigned char*>(read_unaligned_pointer(reinterpret_cast<const unsigned char*>(object)));
  if (!vtable || !memory_readable(vtable - sizeof(void*), sizeof(void*))) {
    return {};
  }
  const auto* locator = reinterpret_cast<const unsigned char*>(read_unaligned_pointer(vtable - sizeof(void*)));
  if (!locator || !memory_readable(locator, 32)) {
    return {};
  }
  const std::uint32_t signature = read_unaligned_u32(locator);
  const unsigned char* type_descriptor = nullptr;
  if (signature == 0) {
    type_descriptor = reinterpret_cast<const unsigned char*>(read_unaligned_pointer(locator + 16));
  } else if (signature == 1) {
    const std::uint32_t self_rva = read_unaligned_u32(locator + 20);
    const std::uint32_t type_descriptor_rva = read_unaligned_u32(locator + 12);
    const std::uintptr_t locator_address = reinterpret_cast<std::uintptr_t>(locator);
    if (self_rva > locator_address) {
      return {};
    }
    const auto* image_base = locator - self_rva;
    type_descriptor = image_base + type_descriptor_rva;
  } else {
    return {};
  }
  if (!type_descriptor || !memory_readable(type_descriptor, 16)) {
    return {};
  }
  const auto* type_name = reinterpret_cast<const char*>(type_descriptor + 16);
  std::string name;
  for (size_t index = 0; index < 256; ++index) {
    if (!memory_readable(type_name + index, 1)) {
      return {};
    }
    if (type_name[index] == '\0') {
      return name;
    }
    name.push_back(type_name[index]);
  }
  return {};
}

struct ImageReadSection {
  const unsigned char* begin = nullptr;
  size_t size = 0;
};

std::vector<const void*> find_vtables_for_type(const char* type_name) {
  std::vector<const void*> vtables;
  if (!type_name || type_name[0] == '\0') {
    return vtables;
  }

  HMODULE executable = GetModuleHandleW(nullptr);
  if (!executable) {
    return vtables;
  }

  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base);
  if (!memory_readable(dos_header, sizeof(*dos_header)) || dos_header->e_magic != IMAGE_DOS_SIGNATURE || dos_header->e_lfanew <= 0) {
    return vtables;
  }

  const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew);
  if (!memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE || nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return vtables;
  }

  const size_t image_size = nt_headers->OptionalHeader.SizeOfImage;
  if (image_size == 0 || !memory_readable(image_base, image_size)) {
    return vtables;
  }

  const size_t type_name_length = std::strlen(type_name);
  const auto* type_name_address = std::search(image_base, image_base + image_size, type_name, type_name + type_name_length);
  if (type_name_address == image_base + image_size || type_name_address < image_base + 16) {
    return vtables;
  }
  const auto* type_descriptor = type_name_address - 16;
  const std::uint32_t type_descriptor_rva = static_cast<std::uint32_t>(type_descriptor - image_base);

  std::vector<ImageReadSection> read_sections;
  const auto* section = IMAGE_FIRST_SECTION(nt_headers);
  for (WORD index = 0; index < nt_headers->FileHeader.NumberOfSections; ++index) {
    const IMAGE_SECTION_HEADER& current = section[index];
    if ((current.Characteristics & IMAGE_SCN_MEM_READ) == 0 || (current.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 || current.VirtualAddress >= image_size) {
      continue;
    }

    const size_t section_size = std::min<size_t>(std::max(current.Misc.VirtualSize, current.SizeOfRawData), image_size - current.VirtualAddress);
    const auto* section_begin = image_base + current.VirtualAddress;
    if (section_size >= 24 && memory_readable(section_begin, section_size)) {
      read_sections.push_back({section_begin, section_size});
    }
  }

  std::vector<const unsigned char*> complete_object_locators;
  for (const ImageReadSection& read_section : read_sections) {
    for (size_t offset = 0; offset + 24 <= read_section.size; ++offset) {
      const auto* candidate = read_section.begin + offset;
      const std::uint32_t candidate_rva = static_cast<std::uint32_t>(candidate - image_base);
      if (read_unaligned_u32(candidate) <= 1 &&
          read_unaligned_u32(candidate + 12) == type_descriptor_rva &&
          read_unaligned_u32(candidate + 20) == candidate_rva) {
        complete_object_locators.push_back(candidate);
      }
    }
  }

  for (const auto* locator : complete_object_locators) {
    for (const ImageReadSection& read_section : read_sections) {
      for (size_t offset = 0; offset + sizeof(void*) <= read_section.size; ++offset) {
        const auto* candidate = read_section.begin + offset;
        if (read_unaligned_pointer(candidate) != reinterpret_cast<std::uintptr_t>(locator)) {
          continue;
        }

        const void* vtable = candidate + sizeof(void*);
        if (memory_readable(vtable, sizeof(void*)) && std::find(vtables.begin(), vtables.end(), vtable) == vtables.end()) {
          vtables.push_back(vtable);
        }
      }
    }
  }

  return vtables;
}

std::vector<const void*> find_vcb_edit_vtables() {
  return find_vtables_for_type(".?AVStatusVCBEdit@@");
}

std::vector<const void*> find_vcb_value_button_vtables() {
  return find_vtables_for_type(".?AVCToolBarVCBValueButton@@");
}

const std::vector<const void*>& find_tag_dialog_header_vtables() {
  static const std::vector<const void*> vtables = find_vtables_for_type(".?AVTagDialogHeaderDelegate@@");
  return vtables;
}

const std::vector<const void*>& find_component_navigator_delegate_vtables() {
  static const std::vector<const void*> vtables = find_vtables_for_type(".?AVComponentNavigatorTreeItemDelegate@@");
  return vtables;
}

std::vector<const void*> find_content_browser_list_vtables() {
  return find_vtables_for_type(".?AVContentBrowserListCtrl@@");
}

std::vector<void**> find_main_import_iat_slots_for_target(void* target_proc) {
  std::vector<void**> slots;
  if (!target_proc) {
    return slots;
  }

  auto add_slot = [&slots](void** slot) {
    if (slot && std::find(slots.begin(), slots.end(), slot) == slots.end()) {
      slots.push_back(slot);
    }
  };

  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base && memory_readable(image_base, sizeof(IMAGE_DOS_HEADER))
    ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base)
    : nullptr;
  const auto* nt_headers = dos_header && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  if (!nt_headers || !memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return slots;
  }

  const IMAGE_DATA_DIRECTORY& imports = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (imports.VirtualAddress == 0 || imports.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
      !memory_readable(image_base + imports.VirtualAddress, imports.Size)) {
    return slots;
  }

  const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image_base + imports.VirtualAddress);
  const size_t descriptor_count = imports.Size / sizeof(*descriptors);
  for (size_t descriptor_index = 0; descriptor_index < descriptor_count; ++descriptor_index) {
    const IMAGE_IMPORT_DESCRIPTOR& descriptor = descriptors[descriptor_index];
    if (descriptor.FirstThunk == 0) {
      break;
    }

    auto* functions = reinterpret_cast<IMAGE_THUNK_DATA64*>(const_cast<unsigned char*>(image_base) + descriptor.FirstThunk);
    for (size_t function_index = 0; memory_readable(functions + function_index, sizeof(*functions)); ++function_index) {
      if (functions[function_index].u1.Function == 0) {
        break;
      }
      if (functions[function_index].u1.Function == reinterpret_cast<ULONGLONG>(target_proc)) {
        add_slot(reinterpret_cast<void**>(&functions[function_index].u1.Function));
      }
    }
  }

  const size_t image_size = nt_headers->OptionalHeader.SizeOfImage;
  const auto* sections = IMAGE_FIRST_SECTION(nt_headers);
  for (WORD section_index = 0; section_index < nt_headers->FileHeader.NumberOfSections; ++section_index) {
    const IMAGE_SECTION_HEADER& section = sections[section_index];
    if ((section.Characteristics & (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE)) != (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE) ||
        (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 || section.VirtualAddress >= image_size) {
      continue;
    }
    const size_t section_size = std::min<size_t>(
      std::max(section.Misc.VirtualSize, section.SizeOfRawData),
      image_size - section.VirtualAddress
    );
    const auto* section_begin = image_base + section.VirtualAddress;
    if (section_size < sizeof(void*) || !memory_readable(section_begin, section_size)) {
      continue;
    }
    for (size_t offset = 0; offset + sizeof(void*) <= section_size; offset += sizeof(void*)) {
      auto* slot = reinterpret_cast<void**>(const_cast<unsigned char*>(section_begin + offset));
      if (read_unaligned_pointer(reinterpret_cast<const unsigned char*>(slot)) == reinterpret_cast<std::uintptr_t>(target_proc)) {
        add_slot(slot);
      }
    }
  }
  return slots;
}

std::vector<void**> find_main_import_iat_slots_for_symbols(const std::vector<std::string>& target_symbols) {
  std::vector<void**> slots;
  if (target_symbols.empty()) {
    return slots;
  }

  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base && memory_readable(image_base, sizeof(IMAGE_DOS_HEADER))
    ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base)
    : nullptr;
  const auto* nt_headers = dos_header && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  if (!nt_headers || !memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return slots;
  }

  const IMAGE_DATA_DIRECTORY& imports = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (imports.VirtualAddress == 0 || imports.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
      !memory_readable(image_base + imports.VirtualAddress, imports.Size)) {
    return slots;
  }

  const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image_base + imports.VirtualAddress);
  const size_t descriptor_count = imports.Size / sizeof(*descriptors);
  for (size_t descriptor_index = 0; descriptor_index < descriptor_count; ++descriptor_index) {
    const IMAGE_IMPORT_DESCRIPTOR& descriptor = descriptors[descriptor_index];
    if (descriptor.OriginalFirstThunk == 0 || descriptor.FirstThunk == 0) {
      continue;
    }

    const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(image_base + descriptor.OriginalFirstThunk);
    auto* functions = reinterpret_cast<IMAGE_THUNK_DATA64*>(const_cast<unsigned char*>(image_base) + descriptor.FirstThunk);
    for (size_t function_index = 0;
         memory_readable(names + function_index, sizeof(*names)) &&
         memory_readable(functions + function_index, sizeof(*functions));
         ++function_index) {
      const ULONGLONG address_of_data = names[function_index].u1.AddressOfData;
      if (address_of_data == 0) {
        break;
      }
      if ((address_of_data & IMAGE_ORDINAL_FLAG64) != 0 || address_of_data >= nt_headers->OptionalHeader.SizeOfImage) {
        continue;
      }
      const auto* symbol = reinterpret_cast<const char*>(image_base + address_of_data + sizeof(WORD));
      if (!memory_readable(symbol, 1)) {
        continue;
      }
      bool matches = false;
      for (const std::string& target_symbol : target_symbols) {
        if (target_symbol == symbol) {
          matches = true;
          break;
        }
      }
      if (matches) {
        slots.push_back(reinterpret_cast<void**>(&functions[function_index].u1.Function));
      }
    }
  }
  return slots;
}

std::vector<void**> find_function_indirect_call_slots(void* function_proc, const std::vector<void**>& target_slots) {
  std::vector<void**> slots;
  if (!function_proc || target_slots.empty()) {
    return slots;
  }

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  using RtlLookupFunctionEntryFn = PRUNTIME_FUNCTION(WINAPI*)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
  const auto rtl_lookup_function_entry = ntdll ? reinterpret_cast<RtlLookupFunctionEntryFn>(GetProcAddress(ntdll, "RtlLookupFunctionEntry")) : nullptr;
  if (!rtl_lookup_function_entry) {
    return slots;
  }

  DWORD64 image_base = 0;
  const PRUNTIME_FUNCTION function_entry = rtl_lookup_function_entry(
    reinterpret_cast<DWORD64>(function_proc), &image_base, nullptr);
  if (!function_entry || image_base == 0 || function_entry->EndAddress <= function_entry->BeginAddress) {
    return slots;
  }

  const auto* function_begin = reinterpret_cast<const unsigned char*>(image_base + function_entry->BeginAddress);
  const auto* function_end = reinterpret_cast<const unsigned char*>(image_base + function_entry->EndAddress);
  if (!memory_readable(function_begin, static_cast<size_t>(function_end - function_begin))) {
    return slots;
  }

  for (const auto* instruction = function_begin; instruction + 6 <= function_end; ++instruction) {
    void** slot = nullptr;
    if (instruction[0] == 0xff && instruction[1] == 0x15) {
      slot = reinterpret_cast<void**>(const_cast<unsigned char*>(rip_relative_target(instruction, 6)));
    } else if (instruction[0] == 0xe8) {
      std::int32_t displacement = 0;
      std::memcpy(&displacement, instruction + 1, sizeof(displacement));
      const auto* thunk = reinterpret_cast<const unsigned char*>(
        reinterpret_cast<std::intptr_t>(instruction + 5) + displacement);
      if (memory_readable(thunk, 6) && thunk[0] == 0xff && thunk[1] == 0x25) {
        slot = reinterpret_cast<void**>(const_cast<unsigned char*>(rip_relative_target(thunk, 6)));
      }
    }
    if (!slot || !memory_readable(slot, sizeof(*slot)) ||
        std::find(target_slots.begin(), target_slots.end(), slot) == target_slots.end() ||
        std::find(slots.begin(), slots.end(), slot) != slots.end()) {
      continue;
    }
    slots.push_back(slot);
  }
  return slots;
}

bool tag_dialog_header_complete_object(void* widget, void*& object, std::uint32_t& object_offset) {
  object = nullptr;
  object_offset = 0;
  if (!pointer_looks_like_cpp_object(widget)) {
    return false;
  }

  const void* vtable = *reinterpret_cast<const void* const*>(widget);
  const auto* vtable_bytes = reinterpret_cast<const unsigned char*>(vtable);
  if (!memory_readable(vtable_bytes - sizeof(void*), sizeof(void*))) {
    return false;
  }

  const void* locator = reinterpret_cast<const void*>(read_unaligned_pointer(vtable_bytes - sizeof(void*)));
  if (!memory_readable(locator, 24)) {
    return false;
  }

  const std::uint32_t signature = read_unaligned_u32(reinterpret_cast<const unsigned char*>(locator));
  if (signature != 0 && signature != 1) {
    return false;
  }

  object_offset = read_unaligned_u32(reinterpret_cast<const unsigned char*>(locator) + 4);
  if (object_offset > 0x10000) {
    return false;
  }

  auto* complete_object = reinterpret_cast<unsigned char*>(widget) - object_offset;
  if (!pointer_looks_like_cpp_object(complete_object)) {
    return false;
  }
  object = complete_object;
  return true;
}

std::vector<void*> find_visible_tag_dialog_header_widgets(const QtLookup& lookup, int stage) {
  std::vector<void*> matches;
  const std::vector<const void*>& vtables = find_tag_dialog_header_vtables();
  if (vtables.empty()) {
    return matches;
  }

  const std::vector<void*> headers = collect_visible_tray_header_views(lookup);
  for (void* header : headers) {
    if (!pointer_looks_like_cpp_object(header)) {
      continue;
    }
    const void* vtable = *reinterpret_cast<const void* const*>(header);
    if (std::find(vtables.begin(), vtables.end(), vtable) != vtables.end()) {
      add_unique_pointer(matches, header);
    }
  }
  if (matches.empty() && lookup.object_children_proc && lookup.widget_is_visible_proc) {
    using QtBoolWidgetFn = bool(__cdecl*)(const void*);
    const std::vector<void*> tray_widgets = collect_tray_qobject_qwidgets(lookup, stage, true);
    size_t fallback_candidates = 0;
    for (void* widget : tray_widgets) {
      if (!pointer_looks_like_cpp_object(widget) ||
          !reinterpret_cast<QtBoolWidgetFn>(lookup.widget_is_visible_proc)(widget)) {
        continue;
      }
      const void* vtable = *reinterpret_cast<const void* const*>(widget);
      if (std::find(vtables.begin(), vtables.end(), vtable) == vtables.end()) {
        continue;
      }
      fallback_candidates += 1;
      add_unique_pointer(matches, widget);
    }
  }
  return matches;
}

bool set_qcolor_rgba(const QtLookup& lookup, void* color, int red, int green, int blue, int alpha) {
  if (!color || !lookup.qcolor_set_rgb_proc) {
    return false;
  }

  using QtColorSetRgbFn = void(__cdecl*)(void*, int, int, int, int);
  __try {
    reinterpret_cast<QtColorSetRgbFn>(lookup.qcolor_set_rgb_proc)(color, red, green, blue, alpha);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

constexpr size_t kTagHeaderColorOffsets[] = {0x28, 0x38, 0x48, 0x58, 0x68, 0x78};
constexpr int kTagHeaderDarkColors[][4] = {
  {43, 43, 43, 255},
  {67, 67, 67, 255},
  {67, 67, 67, 255},
  {54, 54, 54, 255},
  {54, 54, 54, 255},
  {235, 235, 235, 255}
};

bool apply_tag_dialog_header_colors_to_header(const QtLookup& lookup, void* header, bool request_repaint) {
  const std::vector<const void*>& vtables = find_tag_dialog_header_vtables();
  if (!pointer_looks_like_cpp_object(header) || vtables.empty() || !lookup.qcolor_set_rgb_proc ||
      std::find(vtables.begin(), vtables.end(), *reinterpret_cast<const void* const*>(header)) == vtables.end()) {
    return false;
  }

  void* object = nullptr;
  std::uint32_t object_offset = 0;
  if (!tag_dialog_header_complete_object(header, object, object_offset)) {
    return false;
  }

  auto backup = std::find_if(
    g_tag_header_color_backups.begin(),
    g_tag_header_color_backups.end(),
    [object](const std::unique_ptr<TagHeaderColorBackup>& candidate) { return candidate->object == object; }
  );
  TagHeaderColorBackup* active_backup = nullptr;
  if (backup == g_tag_header_color_backups.end()) {
    auto captured = std::make_unique<TagHeaderColorBackup>();
    captured->widget = header;
    captured->object = object;
    bool captured_all = true;
    for (size_t index = 0; index < std::size(kTagHeaderColorOffsets); ++index) {
      const auto* color = reinterpret_cast<const unsigned char*>(object) + kTagHeaderColorOffsets[index];
      if (!memory_readable(color, sizeof(captured->color_storage[index]))) {
        captured_all = false;
        break;
      }
      std::memcpy(captured->color_storage[index], color, sizeof(captured->color_storage[index]));
    }
    if (!captured_all) {
      return false;
    }
    active_backup = captured.get();
    g_tag_header_color_backups.push_back(std::move(captured));
  } else {
    active_backup = backup->get();
  }

  bool changed = true;
  for (size_t index = 0; index < std::size(kTagHeaderColorOffsets); ++index) {
    auto* color = reinterpret_cast<unsigned char*>(object) + kTagHeaderColorOffsets[index];
    if (!memory_readable(color, sizeof(active_backup->color_storage[index])) ||
        !set_qcolor_rgba(lookup, color,
          kTagHeaderDarkColors[index][0], kTagHeaderDarkColors[index][1], kTagHeaderDarkColors[index][2], kTagHeaderDarkColors[index][3])) {
      changed = false;
      break;
    }
  }
  if (!changed) {
    for (size_t index = 0; index < std::size(kTagHeaderColorOffsets); ++index) {
      auto* color = reinterpret_cast<unsigned char*>(object) + kTagHeaderColorOffsets[index];
      if (memory_readable(color, sizeof(active_backup->color_storage[index]))) {
        std::memcpy(color, active_backup->color_storage[index], sizeof(active_backup->color_storage[index]));
      }
    }
    return false;
  }
  if (request_repaint && lookup.widget_update_proc) {
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(header);
  }
  if (request_repaint && lookup.widget_repaint_proc) {
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(header);
  }
  return true;
}

size_t apply_tag_dialog_header_colors(const QtLookup& lookup, bool dark, int stage) {
  const std::vector<const void*>& vtables = find_tag_dialog_header_vtables();
  if (vtables.empty() || !lookup.qcolor_set_rgb_proc) {
    return 0;
  }

  size_t applied = 0;
  if (!dark) {
    for (const std::unique_ptr<TagHeaderColorBackup>& backup_holder : g_tag_header_color_backups) {
      TagHeaderColorBackup& backup = *backup_holder;
      void* current_object = nullptr;
      std::uint32_t object_offset = 0;
      const bool valid_widget = pointer_looks_like_cpp_object(backup.widget) &&
        std::find(vtables.begin(), vtables.end(), *reinterpret_cast<const void* const*>(backup.widget)) != vtables.end() &&
        tag_dialog_header_complete_object(backup.widget, current_object, object_offset) &&
        current_object == backup.object;
      bool restored = true;
      for (size_t index = 0; index < std::size(kTagHeaderColorOffsets); ++index) {
        auto* color = reinterpret_cast<unsigned char*>(backup.object) + kTagHeaderColorOffsets[index];
        if (!valid_widget || !memory_readable(color, sizeof(backup.color_storage[index]))) {
          restored = false;
          continue;
        }
        std::memcpy(color, backup.color_storage[index], sizeof(backup.color_storage[index]));
      }
      if (restored) {
        applied += 1;
      }
    }
    g_tag_header_color_backups.clear();
    return applied;
  }

  const std::vector<void*> headers = find_visible_tag_dialog_header_widgets(lookup, stage);
  for (void* header : headers) {
    if (apply_tag_dialog_header_colors_to_header(lookup, header, true)) {
      applied += 1;
    }
  }

  return applied;
}

using TagHeaderPaintSectionFn = void(__fastcall*)(void*, void*, const void*, int);

void __fastcall tag_header_paint_section_hook(void* header, void* painter, const void* rect, int section) {
  TagHeaderPaintSectionFn original_proc = reinterpret_cast<TagHeaderPaintSectionFn>(g_tag_header_paint_hook.original_proc);
  if (!original_proc) {
    return;
  }

  if (g_tag_header_paint_dark && header) {
    apply_tag_dialog_header_colors_to_header(resolve_qt_lookup(), header, false);
  }
  original_proc(header, painter, rect, section);
}

void remove_tag_header_paint_hook() {
  g_tag_header_paint_dark = false;
  if (g_tag_header_paint_hook.slot && g_tag_header_paint_hook.original_proc &&
      memory_readable(g_tag_header_paint_hook.slot, sizeof(*g_tag_header_paint_hook.slot))) {
    DWORD original_protection = 0;
    if (VirtualProtect(g_tag_header_paint_hook.slot, sizeof(*g_tag_header_paint_hook.slot), PAGE_READWRITE, &original_protection)) {
      if (*g_tag_header_paint_hook.slot == reinterpret_cast<void*>(&tag_header_paint_section_hook)) {
        *g_tag_header_paint_hook.slot = g_tag_header_paint_hook.original_proc;
      }
      DWORD unused_protection = 0;
      VirtualProtect(g_tag_header_paint_hook.slot, sizeof(*g_tag_header_paint_hook.slot), original_protection, &unused_protection);
    }
  }
  g_tag_header_paint_hook = {};
}

bool install_tag_header_paint_hook(bool dark) {
  if (!dark) {
    remove_tag_header_paint_hook();
    return true;
  }

  g_tag_header_paint_dark = true;
  if (g_tag_header_paint_hook.slot) {
    return true;
  }

  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base) : nullptr;
  const auto* nt_headers = memory_readable(dos_header, sizeof(*dos_header)) && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  const size_t image_size = nt_headers && memory_readable(nt_headers, sizeof(*nt_headers)) && nt_headers->Signature == IMAGE_NT_SIGNATURE
    ? nt_headers->OptionalHeader.SizeOfImage
    : 0;
  constexpr size_t section_paint_slot = 89;
  for (const void* vtable : find_tag_dialog_header_vtables()) {
    const auto* vtable_bytes = reinterpret_cast<const unsigned char*>(vtable);
    const void* locator = memory_readable(vtable_bytes - sizeof(void*), sizeof(void*))
      ? reinterpret_cast<const void*>(read_unaligned_pointer(vtable_bytes - sizeof(void*)))
      : nullptr;
    if (!locator || !memory_readable(locator, 24) || read_unaligned_u32(reinterpret_cast<const unsigned char*>(locator) + 4) != 0) {
      continue;
    }
    void** slots = const_cast<void**>(reinterpret_cast<void* const*>(vtable));
    const auto* section_paint = memory_readable(slots, (section_paint_slot + 1) * sizeof(*slots))
      ? reinterpret_cast<const unsigned char*>(slots[section_paint_slot])
      : nullptr;
    if (!section_paint || !image_base || section_paint < image_base || section_paint >= image_base + image_size) {
      continue;
    }
    DWORD original_protection = 0;
    if (!VirtualProtect(&slots[section_paint_slot], sizeof(*slots), PAGE_READWRITE, &original_protection)) {
      continue;
    }
    void* original_proc = slots[section_paint_slot];
    slots[section_paint_slot] = reinterpret_cast<void*>(&tag_header_paint_section_hook);
    DWORD unused_protection = 0;
    VirtualProtect(&slots[section_paint_slot], sizeof(*slots), original_protection, &unused_protection);
    g_tag_header_paint_hook = {&slots[section_paint_slot], original_proc};
    return true;
  }
  g_tag_header_paint_dark = false;
  return false;
}

using ComponentNavigatorPaintFn = void(__fastcall*)(void*, void*, void*, void*);
using ComponentNavigatorDrawTextOptionFn = void(__cdecl*)(void*, const void*, const void*, const void*);
using ComponentNavigatorDrawTextFlagsFn = void(__cdecl*)(void*, const void*, int, const void*, void*);

void __fastcall component_navigator_paint_hook(void* delegate, void* painter, void* option, void* index) {
  ComponentNavigatorPaintFn original_proc = reinterpret_cast<ComponentNavigatorPaintFn>(g_component_navigator_paint_hook.original_proc);
  if (!original_proc) {
    return;
  }

  if (g_component_navigator_paint_dark) {
    g_component_navigator_paint_scope_depth += 1;
  }

  if (InterlockedIncrement(&g_component_navigator_paint_hook_invocation_count) == 1) {
  }

  original_proc(delegate, painter, option, index);

  if (g_component_navigator_paint_dark && g_component_navigator_paint_scope_depth > 0) {
    g_component_navigator_paint_scope_depth -= 1;
  }
}

bool apply_component_navigator_light_text_pen(void* painter) {
  if (g_component_navigator_paint_dark && g_component_navigator_paint_scope_depth > 0 && painter &&
      g_component_navigator_qcolor_rgb_ctor_proc && g_component_navigator_set_pen_color_proc) {
    alignas(16) unsigned char color_storage[64] = {};
    g_component_navigator_qcolor_rgb_ctor_proc(color_storage, 230, 232, 238, 255);
    g_component_navigator_set_pen_color_proc(painter, color_storage);
    return true;
  }
  return false;
}

void __cdecl component_navigator_draw_text_option_hook(void* painter, const void* rectangle, const void* text, const void* option) {
  const auto original_proc = g_component_navigator_draw_text_option_hooks.empty()
    ? nullptr
    : reinterpret_cast<ComponentNavigatorDrawTextOptionFn>(g_component_navigator_draw_text_option_hooks.front().original_proc);
  if (!original_proc) {
    return;
  }

  if (apply_component_navigator_light_text_pen(painter) &&
      InterlockedIncrement(&g_component_navigator_text_draw_hook_invocation_count) == 1) {
  }

  original_proc(painter, rectangle, text, option);
}

void __cdecl component_navigator_draw_text_flags_hook(void* painter, const void* rectangle, int flags, const void* text, void* bounding_rectangle) {
  const auto original_proc = g_component_navigator_draw_text_flags_hooks.empty()
    ? nullptr
    : reinterpret_cast<ComponentNavigatorDrawTextFlagsFn>(g_component_navigator_draw_text_flags_hooks.front().original_proc);
  if (!original_proc) {
    return;
  }

  if (apply_component_navigator_light_text_pen(painter) &&
      InterlockedIncrement(&g_component_navigator_text_draw_hook_invocation_count) == 1) {
  }

  original_proc(painter, rectangle, flags, text, bounding_rectangle);
}

using ContentBrowserPaintEventFn = void(__fastcall*)(void*, void*);

size_t clear_content_browser_cached_images(void* widget) {
  if (!pointer_looks_like_cpp_object(widget) || !g_content_browser_qimage_default_ctor_proc ||
      !g_content_browser_qimage_destructor_proc) {
    return 0;
  }

  auto* widget_bytes = reinterpret_cast<unsigned char*>(widget);
  if (!memory_readable(widget_bytes + 0xf8, 2 * sizeof(void*))) {
    return 0;
  }

  void** rows_begin = *reinterpret_cast<void***>(widget_bytes + 0xf8);
  void** rows_end = *reinterpret_cast<void***>(widget_bytes + 0x100);
  const std::uintptr_t begin_value = reinterpret_cast<std::uintptr_t>(rows_begin);
  const std::uintptr_t end_value = reinterpret_cast<std::uintptr_t>(rows_end);
  if (!rows_begin || end_value < begin_value || (end_value - begin_value) % sizeof(void*) != 0) {
    return 0;
  }

  const size_t row_count = (end_value - begin_value) / sizeof(void*);
  if (row_count > 100000 || !memory_readable(rows_begin, row_count * sizeof(void*))) {
    return 0;
  }

  size_t reset_count = 0;
  for (void** current = rows_begin; current != rows_end; ++current) {
    auto* row = reinterpret_cast<unsigned char*>(*current);
    if (!row || !memory_readable(row + 0x60, 24)) {
      continue;
    }
    __try {
      g_content_browser_qimage_destructor_proc(row + 0x60);
      g_content_browser_qimage_default_ctor_proc(row + 0x60);
      reset_count += 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
  }
  return reset_count;
}

bool content_browser_image_generator_on_stack() {
  if (!g_content_browser_image_generator_begin || !g_content_browser_image_generator_end) {
    return false;
  }
  void* frames[8] = {};
  const USHORT frame_count = CaptureStackBackTrace(0, static_cast<DWORD>(std::size(frames)), frames, nullptr);
  for (USHORT index = 0; index < frame_count; ++index) {
    const auto* frame = reinterpret_cast<const unsigned char*>(frames[index]);
    if (frame >= g_content_browser_image_generator_begin && frame < g_content_browser_image_generator_end) {
      return true;
    }
  }
  return false;
}

void __fastcall content_browser_paint_scope_hook(void* widget, void* event) {
  const auto original_proc = reinterpret_cast<ContentBrowserPaintEventFn>(g_content_browser_paint_hook.original_proc);
  if (!original_proc) {
    return;
  }

  if (g_content_browser_text_dark) {
    g_content_browser_paint_scope_depth += 1;
    if (g_content_browser_image_cache_reset_widgets.insert(widget).second) {
      static_cast<void>(clear_content_browser_cached_images(widget));
    }
    if (InterlockedIncrement(&g_content_browser_paint_hook_invocation_count) == 1) {
    }
  }

  original_proc(widget, event);

  if (g_content_browser_text_dark && g_content_browser_paint_scope_depth > 0) {
    g_content_browser_paint_scope_depth -= 1;
  }
}

void __cdecl content_browser_qimage_fill_color_hook(void* image, const void* color) {
  if (!g_content_browser_qimage_fill_color_proc) {
    return;
  }

  const auto* return_address = reinterpret_cast<const unsigned char*>(_ReturnAddress());
  const bool resource_image_renderer =
    g_content_browser_resource_image_renderer_begin && g_content_browser_resource_image_renderer_end &&
    return_address >= g_content_browser_resource_image_renderer_begin &&
    return_address < g_content_browser_resource_image_renderer_end;
  const bool image_generator_on_stack = resource_image_renderer && content_browser_image_generator_on_stack();
  const unsigned int rgb = color && g_content_browser_qcolor_rgb_proc
    ? g_content_browser_qcolor_rgb_proc(color) & 0x00ffffffu
    : 0xffffffffu;  const bool resource_image_fill = g_content_browser_text_dark && resource_image_renderer && image_generator_on_stack;
  const bool white = resource_image_fill && rgb == 0x00ffffffu;
  if (white && g_content_browser_qcolor_rgb_ctor_proc) {
    alignas(16) unsigned char dark_color_storage[64] = {};
    g_content_browser_qcolor_rgb_ctor_proc(dark_color_storage, 43, 43, 43, 255);
    g_content_browser_qimage_fill_color_proc(image, dark_color_storage);
    if (InterlockedIncrement(&g_content_browser_folder_image_fill_hook_invocation_count) == 1) {
    }
    return;
  }
  g_content_browser_qimage_fill_color_proc(image, color);
}

bool apply_content_browser_light_text_pen(void* painter) {
  if (!g_content_browser_text_dark || g_content_browser_paint_scope_depth <= 0 || !painter ||
      !g_content_browser_qcolor_rgb_ctor_proc || !g_content_browser_set_pen_color_proc) {
    return false;
  }

  alignas(16) unsigned char color_storage[64] = {};
  g_content_browser_qcolor_rgb_ctor_proc(color_storage, 230, 232, 238, 255);
  g_content_browser_set_pen_color_proc(painter, color_storage);
  return true;
}

void __fastcall content_browser_draw_text_option_hook(void* painter, const void* rectangle, const void* text, const void* option) {
  const auto original_proc = g_content_browser_draw_text_option_hooks.empty()
    ? nullptr
    : reinterpret_cast<ContentBrowserDrawTextOptionFn>(g_content_browser_draw_text_option_hooks.front().original_proc);
  if (!original_proc) {
    return;
  }

  if (apply_content_browser_light_text_pen(painter) &&
      InterlockedIncrement(&g_content_browser_text_draw_hook_invocation_count) == 1) {
  }
  original_proc(painter, rectangle, text, option);
}

void __fastcall content_browser_draw_text_flags_hook(void* painter, const void* rectangle, int flags, const void* text, void* bounding_rectangle) {
  const auto original_proc = g_content_browser_draw_text_flags_hooks.empty()
    ? nullptr
    : reinterpret_cast<ContentBrowserDrawTextFlagsFn>(g_content_browser_draw_text_flags_hooks.front().original_proc);
  if (!original_proc) {
    return;
  }

  if (apply_content_browser_light_text_pen(painter) &&
      InterlockedIncrement(&g_content_browser_text_draw_hook_invocation_count) == 1) {
  }
  original_proc(painter, rectangle, flags, text, bounding_rectangle);
}

void remove_content_browser_text_hooks() {
  g_content_browser_text_dark = false;
  const QtLookup lookup = resolve_qt_lookup();
  size_t reset_count = 0;
  if (lookup.widget_repaint_proc) {
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    for (void* widget : g_content_browser_image_cache_reset_widgets) {
      if (!pointer_looks_like_cpp_object(widget)) {
        continue;
      }
      reset_count += clear_content_browser_cached_images(widget);
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    }
  }
  if (reset_count > 0) {
  }
  g_content_browser_image_cache_reset_widgets.clear();
  auto restore_hooks = [](const std::vector<ContentBrowserTextDrawHook>& hooks, void* hook_proc) {
    for (const ContentBrowserTextDrawHook& hook : hooks) {
      if (!hook.slot || !hook.original_proc || !memory_readable(hook.slot, sizeof(*hook.slot))) {
        continue;
      }
      DWORD original_protection = 0;
      if (VirtualProtect(hook.slot, sizeof(*hook.slot), PAGE_READWRITE, &original_protection)) {
        if (*hook.slot == hook_proc) {
          *hook.slot = hook.original_proc;
        }
        DWORD unused_protection = 0;
        VirtualProtect(hook.slot, sizeof(*hook.slot), original_protection, &unused_protection);
      }
    }
  };
  restore_hooks(g_content_browser_draw_text_option_hooks, reinterpret_cast<void*>(&content_browser_draw_text_option_hook));
  restore_hooks(g_content_browser_draw_text_flags_hooks, reinterpret_cast<void*>(&content_browser_draw_text_flags_hook));
  restore_hooks(g_content_browser_qimage_fill_color_hooks, reinterpret_cast<void*>(&content_browser_qimage_fill_color_hook));
  g_content_browser_draw_text_option_hooks.clear();
  g_content_browser_draw_text_flags_hooks.clear();
  g_content_browser_qimage_fill_color_hooks.clear();
  g_content_browser_qcolor_rgb_ctor_proc = nullptr;
  g_content_browser_qcolor_rgb_proc = nullptr;
  g_content_browser_qimage_default_ctor_proc = nullptr;
  g_content_browser_qimage_destructor_proc = nullptr;
  g_content_browser_qimage_fill_color_proc = nullptr;
  g_content_browser_set_pen_color_proc = nullptr;
  g_content_browser_image_generator_begin = nullptr;
  g_content_browser_image_generator_end = nullptr;
  g_content_browser_resource_image_renderer_begin = nullptr;
  g_content_browser_resource_image_renderer_end = nullptr;

  if (g_content_browser_paint_hook.slot && g_content_browser_paint_hook.original_proc &&
      memory_readable(g_content_browser_paint_hook.slot, sizeof(*g_content_browser_paint_hook.slot))) {
    DWORD original_protection = 0;
    if (VirtualProtect(g_content_browser_paint_hook.slot, sizeof(*g_content_browser_paint_hook.slot), PAGE_READWRITE, &original_protection)) {
      if (*g_content_browser_paint_hook.slot == reinterpret_cast<void*>(&content_browser_paint_scope_hook)) {
        *g_content_browser_paint_hook.slot = g_content_browser_paint_hook.original_proc;
      }
      DWORD unused_protection = 0;
      VirtualProtect(g_content_browser_paint_hook.slot, sizeof(*g_content_browser_paint_hook.slot), original_protection, &unused_protection);
    }
  }
  g_content_browser_paint_hook = {};
  g_content_browser_paint_scope_depth = 0;
}

bool install_content_browser_text_hooks(bool dark) {
  if (!dark) {
    remove_content_browser_text_hooks();
    return true;
  }

  g_content_browser_text_dark = true;
  InterlockedExchange(&g_content_browser_text_draw_hook_invocation_count, 0);
  InterlockedExchange(&g_content_browser_paint_hook_invocation_count, 0);
  InterlockedExchange(&g_content_browser_folder_image_fill_hook_invocation_count, 0);
  if (g_content_browser_paint_hook.slot &&
      (!g_content_browser_draw_text_option_hooks.empty() || !g_content_browser_draw_text_flags_hooks.empty())) {
    return true;
  }

  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base) : nullptr;
  const auto* nt_headers = memory_readable(dos_header, sizeof(*dos_header)) && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  const size_t image_size = nt_headers && memory_readable(nt_headers, sizeof(*nt_headers)) && nt_headers->Signature == IMAGE_NT_SIGNATURE
    ? nt_headers->OptionalHeader.SizeOfImage
    : 0;
  if (!image_base || image_size == 0) {
    g_content_browser_text_dark = false;
    return false;
  }

  const QtLookup lookup = resolve_qt_lookup();
  std::string draw_text_option_module;
  std::string draw_text_option_symbol;
  const FARPROC draw_text_option_proc = find_export(lookup.gui_modules, {
    "?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z"
  }, draw_text_option_module, draw_text_option_symbol);
  std::string draw_text_flags_module;
  std::string draw_text_flags_symbol;
  const FARPROC draw_text_flags_proc = find_export(lookup.gui_modules, {
    "?drawText@QPainter@@QEAAXAEBVQRectF@@HAEBVQString@@PEAV2@@Z"
  }, draw_text_flags_module, draw_text_flags_symbol);
  std::string draw_text_rect_flags_module;
  std::string draw_text_rect_flags_symbol;
  const FARPROC draw_text_rect_flags_proc = find_export(lookup.gui_modules, {
    "?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z"
  }, draw_text_rect_flags_module, draw_text_rect_flags_symbol);
  std::string qimage_fill_color_module;
  std::string qimage_fill_color_symbol;
  const FARPROC qimage_fill_color_proc = find_export(lookup.gui_modules, {
    "?fill@QImage@@QEAAXAEBVQColor@@@Z"
  }, qimage_fill_color_module, qimage_fill_color_symbol);
  std::string qimage_default_ctor_module;
  std::string qimage_default_ctor_symbol;
  const FARPROC qimage_default_ctor_proc = find_export(lookup.gui_modules, {
    "??0QImage@@QEAA@XZ"
  }, qimage_default_ctor_module, qimage_default_ctor_symbol);
  std::string qimage_destructor_module;
  std::string qimage_destructor_symbol;
  const FARPROC qimage_destructor_proc = find_export(lookup.gui_modules, {
    "??1QImage@@UEAA@XZ"
  }, qimage_destructor_module, qimage_destructor_symbol);
  if (!lookup.qcolor_rgb_ctor_proc || !lookup.qcolor_rgb_proc || !qimage_default_ctor_proc ||
      !qimage_destructor_proc || !qimage_fill_color_proc ||
      !lookup.qpainter_set_pen_color_proc ||
      (!draw_text_option_proc && !draw_text_flags_proc && !draw_text_rect_flags_proc)) {
    g_content_browser_text_dark = false;
    return false;
  }
  g_content_browser_qcolor_rgb_ctor_proc = reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc);
  g_content_browser_qcolor_rgb_proc = reinterpret_cast<QtColorRgbFn>(lookup.qcolor_rgb_proc);
  g_content_browser_qimage_default_ctor_proc = reinterpret_cast<QtImageDefaultCtorFn>(qimage_default_ctor_proc);
  g_content_browser_qimage_destructor_proc = reinterpret_cast<QtImageDestructorFn>(qimage_destructor_proc);
  g_content_browser_qimage_fill_color_proc = reinterpret_cast<QtImageFillColorFn>(qimage_fill_color_proc);
  g_content_browser_set_pen_color_proc = reinterpret_cast<QtPainterSetPenColorFn>(lookup.qpainter_set_pen_color_proc);

  const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt_headers);
  auto is_executable_address = [image_base, image_size, nt_headers, sections](const unsigned char* address) {
    if (!address || address < image_base || address >= image_base + image_size) {
      return false;
    }
    const size_t rva = static_cast<size_t>(address - image_base);
    for (WORD index = 0; index < nt_headers->FileHeader.NumberOfSections; ++index) {
      const IMAGE_SECTION_HEADER& section = sections[index];
      const size_t section_size = std::max<size_t>(section.Misc.VirtualSize, section.SizeOfRawData);
      if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 && rva >= section.VirtualAddress && rva < section.VirtualAddress + section_size) {
        return true;
      }
    }
    return false;
  };

  const std::vector<const void*> content_browser_vtables = find_content_browser_list_vtables();
  void** paint_slot = nullptr;
  constexpr size_t paint_event_slot = 27;
  for (const void* vtable : content_browser_vtables) {
    void** slots = const_cast<void**>(reinterpret_cast<void* const*>(vtable));
    if (!memory_readable(slots, (paint_event_slot + 1) * sizeof(*slots))) {
      continue;
    }
    const auto* candidate = reinterpret_cast<const unsigned char*>(slots[paint_event_slot]);
    if (is_executable_address(candidate)) {
      paint_slot = &slots[paint_event_slot];
      break;
    }
  }
  if (!paint_slot) {
    g_content_browser_text_dark = false;
    return false;
  }

  DWORD original_protection = 0;
  if (!VirtualProtect(paint_slot, sizeof(*paint_slot), PAGE_READWRITE, &original_protection)) {
    g_content_browser_text_dark = false;
    return false;
  }
  const void* original_paint_proc = *paint_slot;
  const std::vector<void**> draw_text_option_slots = find_function_indirect_call_slots(
    const_cast<void*>(original_paint_proc), {reinterpret_cast<void*>(draw_text_option_proc)});
  const std::vector<void**> draw_text_flags_slots = find_function_indirect_call_slots(
    const_cast<void*>(original_paint_proc), {
      reinterpret_cast<void*>(draw_text_flags_proc),
      reinterpret_cast<void*>(draw_text_rect_flags_proc)
    });
  constexpr size_t resource_image_renderer_rva = 0x656b90;
  void* resource_image_renderer_proc = const_cast<unsigned char*>(image_base + resource_image_renderer_rva);
  const std::vector<void**> qimage_fill_color_slots = find_function_indirect_call_slots(
    resource_image_renderer_proc, {reinterpret_cast<void*>(qimage_fill_color_proc)});
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  using RtlLookupFunctionEntryFn = PRUNTIME_FUNCTION(WINAPI*)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
  const auto rtl_lookup_function_entry = ntdll
    ? reinterpret_cast<RtlLookupFunctionEntryFn>(GetProcAddress(ntdll, "RtlLookupFunctionEntry"))
    : nullptr;
  auto resolve_function_range = [rtl_lookup_function_entry](void* function_proc,
      const unsigned char*& function_begin, const unsigned char*& function_end) {
    DWORD64 function_image_base = 0;
    const PRUNTIME_FUNCTION function_entry = rtl_lookup_function_entry
      ? rtl_lookup_function_entry(reinterpret_cast<DWORD64>(function_proc), &function_image_base, nullptr)
      : nullptr;
    if (function_entry && function_image_base != 0 && function_entry->EndAddress > function_entry->BeginAddress) {
      function_begin = reinterpret_cast<const unsigned char*>(function_image_base + function_entry->BeginAddress);
      function_end = reinterpret_cast<const unsigned char*>(function_image_base + function_entry->EndAddress);
    }
  };
  constexpr size_t image_generator_rva = 0x595590;
  void* image_generator_proc = const_cast<unsigned char*>(image_base + image_generator_rva);
  resolve_function_range(image_generator_proc,
    g_content_browser_image_generator_begin, g_content_browser_image_generator_end);
  resolve_function_range(resource_image_renderer_proc,
    g_content_browser_resource_image_renderer_begin, g_content_browser_resource_image_renderer_end);
  DWORD unused_protection = 0;
  if (draw_text_option_slots.empty() && draw_text_flags_slots.empty()) {
    VirtualProtect(paint_slot, sizeof(*paint_slot), original_protection, &unused_protection);
    g_content_browser_text_dark = false;
    return false;
  }
  *paint_slot = reinterpret_cast<void*>(&content_browser_paint_scope_hook);
  VirtualProtect(paint_slot, sizeof(*paint_slot), original_protection, &unused_protection);
  g_content_browser_paint_hook = {paint_slot, const_cast<void*>(original_paint_proc)};

  auto install_import_hooks = [](const std::vector<void**>& slots, void* hook_proc,
      std::vector<ContentBrowserTextDrawHook>& hooks) {
    for (void** slot : slots) {
      if (!memory_readable(slot, sizeof(*slot)) || !*slot) {
        continue;
      }
      DWORD original_protection = 0;
      if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
        continue;
      }
      void* original_proc = *slot;
      *slot = hook_proc;
      DWORD unused_protection = 0;
      VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
      hooks.push_back({slot, original_proc});
    }
  };
  install_import_hooks(draw_text_option_slots, reinterpret_cast<void*>(&content_browser_draw_text_option_hook),
    g_content_browser_draw_text_option_hooks);
  install_import_hooks(draw_text_flags_slots, reinterpret_cast<void*>(&content_browser_draw_text_flags_hook),
    g_content_browser_draw_text_flags_hooks);
  install_import_hooks(qimage_fill_color_slots, reinterpret_cast<void*>(&content_browser_qimage_fill_color_hook),
    g_content_browser_qimage_fill_color_hooks);
  if (g_content_browser_draw_text_option_hooks.empty() && g_content_browser_draw_text_flags_hooks.empty()) {
    remove_content_browser_text_hooks();
    return false;
  }

  size_t repaint_count = 0;
  if (lookup.widget_repaint_proc) {
    using QtBoolWidgetFn = bool(__cdecl*)(const void*);
    using QtWidgetNoArgFn = void(__cdecl*)(void*);
    for (void* widget : collect_tray_qobject_qwidgets(lookup, 74, true)) {
      if (!pointer_looks_like_cpp_object(widget) ||
          std::find(content_browser_vtables.begin(), content_browser_vtables.end(), *reinterpret_cast<const void* const*>(widget)) == content_browser_vtables.end()) {
        continue;
      }
      const bool visible = !lookup.widget_is_visible_proc ||
        reinterpret_cast<QtBoolWidgetFn>(lookup.widget_is_visible_proc)(widget);
      if (visible) {
        reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
        repaint_count += 1;
      }
    }
  }

  return true;
}

void remove_component_navigator_paint_hook() {
  g_component_navigator_paint_dark = false;
  auto restore_hooks = [](const std::vector<ComponentNavigatorTextDrawHook>& hooks, void* hook_proc) {
    for (const ComponentNavigatorTextDrawHook& hook : hooks) {
      if (!hook.slot || !hook.original_proc || !memory_readable(hook.slot, sizeof(*hook.slot))) {
        continue;
      }
      DWORD original_protection = 0;
      if (VirtualProtect(hook.slot, sizeof(*hook.slot), PAGE_READWRITE, &original_protection)) {
        if (*hook.slot == hook_proc) {
          *hook.slot = hook.original_proc;
        }
        DWORD unused_protection = 0;
        VirtualProtect(hook.slot, sizeof(*hook.slot), original_protection, &unused_protection);
      }
    }
  };
  restore_hooks(g_component_navigator_draw_text_option_hooks, reinterpret_cast<void*>(&component_navigator_draw_text_option_hook));
  restore_hooks(g_component_navigator_draw_text_flags_hooks, reinterpret_cast<void*>(&component_navigator_draw_text_flags_hook));
  g_component_navigator_draw_text_option_hooks.clear();
  g_component_navigator_draw_text_flags_hooks.clear();
  g_component_navigator_qcolor_rgb_ctor_proc = nullptr;
  g_component_navigator_set_pen_color_proc = nullptr;
  if (g_component_navigator_paint_hook.slot && g_component_navigator_paint_hook.original_proc &&
      memory_readable(g_component_navigator_paint_hook.slot, sizeof(*g_component_navigator_paint_hook.slot))) {
    DWORD original_protection = 0;
    if (VirtualProtect(g_component_navigator_paint_hook.slot, sizeof(*g_component_navigator_paint_hook.slot), PAGE_READWRITE, &original_protection)) {
      if (*g_component_navigator_paint_hook.slot == reinterpret_cast<void*>(&component_navigator_paint_hook)) {
        *g_component_navigator_paint_hook.slot = g_component_navigator_paint_hook.original_proc;
      }
      DWORD unused_protection = 0;
      VirtualProtect(g_component_navigator_paint_hook.slot, sizeof(*g_component_navigator_paint_hook.slot), original_protection, &unused_protection);
    }
  }
  g_component_navigator_paint_hook = {};
  g_component_navigator_paint_scope_depth = 0;
}

bool install_component_navigator_paint_hook(bool dark) {
  if (!dark) {
    remove_component_navigator_paint_hook();
    return true;
  }

  g_component_navigator_paint_dark = true;
  InterlockedExchange(&g_component_navigator_paint_hook_invocation_count, 0);
  InterlockedExchange(&g_component_navigator_text_draw_hook_invocation_count, 0);
  if (g_component_navigator_paint_hook.slot &&
      (!g_component_navigator_draw_text_option_hooks.empty() || !g_component_navigator_draw_text_flags_hooks.empty())) {
    return true;
  }

  const QtLookup lookup = resolve_qt_lookup();
  const std::vector<void**> draw_text_option_iat_slots = find_main_import_iat_slots_for_symbols({
    "?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z"
  });
  const std::vector<void**> draw_text_flags_iat_slots = find_main_import_iat_slots_for_symbols({
    "?drawText@QPainter@@QEAAXAEBVQRectF@@HAEBVQString@@PEAV2@@Z",
    "?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z"
  });
  if ((draw_text_option_iat_slots.empty() && draw_text_flags_iat_slots.empty()) ||
      !lookup.qcolor_rgb_ctor_proc || !lookup.qpainter_set_pen_color_proc) {
    g_component_navigator_paint_dark = false;
    return false;
  }

  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base) : nullptr;
  const auto* nt_headers = memory_readable(dos_header, sizeof(*dos_header)) && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  const size_t image_size = nt_headers && memory_readable(nt_headers, sizeof(*nt_headers)) && nt_headers->Signature == IMAGE_NT_SIGNATURE
    ? nt_headers->OptionalHeader.SizeOfImage
    : 0;
  if (!image_base || image_size == 0) {
    g_component_navigator_paint_dark = false;
    return false;
  }

  constexpr size_t max_delegate_vtable_slots = 64;
  void** paint_slot = nullptr;
  std::vector<void**> draw_text_option_slots;
  std::vector<void**> draw_text_flags_slots;
  for (const void* vtable : find_component_navigator_delegate_vtables()) {
    void** slots = const_cast<void**>(reinterpret_cast<void* const*>(vtable));
    if (!slots || !memory_readable(slots, max_delegate_vtable_slots * sizeof(*slots))) {
      continue;
    }
    for (size_t slot_index = 0; slot_index < max_delegate_vtable_slots; ++slot_index) {
      void* candidate = slots[slot_index];
      const auto* candidate_bytes = reinterpret_cast<const unsigned char*>(candidate);
      if (!candidate_bytes || candidate_bytes < image_base || candidate_bytes >= image_base + image_size) {
        continue;
      }
      const std::vector<void**> candidate_draw_text_option_slots = find_function_indirect_call_slots(candidate, draw_text_option_iat_slots);
      const std::vector<void**> candidate_draw_text_flags_slots = find_function_indirect_call_slots(candidate, draw_text_flags_iat_slots);
      if (candidate_draw_text_option_slots.empty() && candidate_draw_text_flags_slots.empty()) {
        continue;
      }
      paint_slot = &slots[slot_index];
      draw_text_option_slots = candidate_draw_text_option_slots;
      draw_text_flags_slots = candidate_draw_text_flags_slots;
      break;
    }
    if (paint_slot) {
      break;
    }
  }
  if (!paint_slot || (draw_text_option_slots.empty() && draw_text_flags_slots.empty())) {
    g_component_navigator_paint_dark = false;
    return false;
  }

  DWORD original_protection = 0;
  if (!VirtualProtect(paint_slot, sizeof(*paint_slot), PAGE_READWRITE, &original_protection)) {
    g_component_navigator_paint_dark = false;
    return false;
  }
  void* original_paint_proc = *paint_slot;
  *paint_slot = reinterpret_cast<void*>(&component_navigator_paint_hook);
  DWORD unused_protection = 0;
  VirtualProtect(paint_slot, sizeof(*paint_slot), original_protection, &unused_protection);
  g_component_navigator_paint_hook = {paint_slot, original_paint_proc};
  g_component_navigator_qcolor_rgb_ctor_proc = reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc);
  g_component_navigator_set_pen_color_proc = reinterpret_cast<QtPainterSetPenColorFn>(lookup.qpainter_set_pen_color_proc);

  auto install_hooks = [](const std::vector<void**>& slots, void* hook_proc,
      std::vector<ComponentNavigatorTextDrawHook>& hooks) {
    for (void** slot : slots) {
      if (!slot || !memory_readable(slot, sizeof(*slot))) {
        continue;
      }
      DWORD original_protection = 0;
      if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
        continue;
      }
      void* original_proc = *slot;
      *slot = hook_proc;
      DWORD unused_protection = 0;
      VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
      hooks.push_back({slot, original_proc});
    }
  };
  install_hooks(draw_text_option_slots, reinterpret_cast<void*>(&component_navigator_draw_text_option_hook),
    g_component_navigator_draw_text_option_hooks);
  install_hooks(draw_text_flags_slots, reinterpret_cast<void*>(&component_navigator_draw_text_flags_hook),
    g_component_navigator_draw_text_flags_hooks);
  if (g_component_navigator_draw_text_option_hooks.empty() && g_component_navigator_draw_text_flags_hooks.empty()) {
    remove_component_navigator_paint_hook();
    return false;
  }

  return true;
}

std::vector<void*> find_vcb_edit_widgets(const QtLookup& lookup) {
  std::vector<void*> matches;
  const std::vector<const void*> vtables = find_vcb_edit_vtables();
  if (vtables.empty() || !lookup.object_children_proc) {
    return matches;
  }

  std::string main_window_description;
  void* main_window = find_main_window_qwidget(lookup, main_window_description);
  std::vector<void*> widgets;
  std::unordered_set<std::uintptr_t> visited;
  if (main_window) {
    collect_qwidget_descendants_from_qobject(lookup, main_window, 0, 32, widgets, 1200, visited, "VCB edit", true);
  } else {
  }

  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }
    const void* vtable = *reinterpret_cast<const void* const*>(widget);
    if (std::find(vtables.begin(), vtables.end(), vtable) != vtables.end()) {
      add_unique_pointer(matches, widget);
    }
  }

  if (matches.empty() && lookup.widget_find_proc) {
    const std::vector<HWND> hwnds = process_windows_including_descendants();
    std::vector<void*> hwnd_widgets;
    std::unordered_set<std::uintptr_t> hwnd_visited;
    for (HWND hwnd : hwnds) {
      void* root = qwidget_for_hwnd(lookup, hwnd);
      if (!root || !pointer_looks_like_cpp_object(root)) {
        continue;
      }
      collect_qwidget_descendants_from_qobject(lookup, root, 0, 16, hwnd_widgets, 1200, hwnd_visited, "VCB edit hwnd", true);
    }
    for (void* widget : hwnd_widgets) {
      const void* vtable = *reinterpret_cast<const void* const*>(widget);
      if (std::find(vtables.begin(), vtables.end(), vtable) != vtables.end()) {
        add_unique_pointer(matches, widget);
      }
    }
  }

  return matches;
}

std::vector<void*> find_entity_info_dialog_widgets(const QtLookup& lookup) {
  std::vector<void*> matches;
  const std::vector<const void*> vtables = find_vtables_for_type(".?AVCEntityInfoDlg@@");
  if (!lookup.object_children_proc) {
    return matches;
  }

  std::string main_window_description;
  void* main_window = find_main_window_qwidget(lookup, main_window_description);
  if (!main_window) {
    return matches;
  }

  std::vector<void*> widgets;
  std::unordered_set<std::uintptr_t> visited;
  collect_qwidget_descendants_from_qobject(lookup, main_window, 0, 32, widgets, 1200, visited, "Entity Info", true);
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }
    const void* vtable = *reinterpret_cast<const void* const*>(widget);
    if (std::find(vtables.begin(), vtables.end(), vtable) != vtables.end() ||
        msvc_rtti_type_name(widget) == ".?AVCEntityInfoDlg@@") {
      add_unique_pointer(matches, widget);
    }
  }

  return matches;
}

using EntityInfoInputPaintFn = void(__fastcall*)(void*, void*);

struct EntityInfoInputPaintHook {
  void** slot = nullptr;
  void* original_proc = nullptr;
};

std::vector<EntityInfoInputPaintHook> g_entity_info_input_paint_hooks;
bool g_entity_info_input_paint_dark = false;
LONG g_entity_info_input_paint_hook_invocation_count = 0;
thread_local bool g_entity_info_input_palette_applying = false;

bool is_entity_info_input_widget(const QtLookup& lookup, void* widget) {
  for (int depth = 0; widget && depth < 16; ++depth) {
    if (msvc_rtti_type_name(widget) == ".?AVCEntityInfoDlg@@") {
      return true;
    }
    widget = qwidget_parent_widget(lookup, widget);
  }
  return false;
}

bool apply_entity_info_line_edit_palette(const QtLookup& lookup, void* widget, bool dark) {
  if (!widget || !lookup.widget_palette_proc || !lookup.widget_set_palette_proc || !lookup.qpalette_copy_ctor_proc ||
      !lookup.qpalette_destructor_proc) {
    return false;
  }

  using QtWidgetPaletteFn = const void*(__cdecl*)(const void*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtWidgetSetRoleFn = void(__cdecl*)(void*, int);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteReturnFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  if (!dark && lookup.app_palette_widget_proc) {
    reinterpret_cast<QtPaletteReturnFn>(lookup.app_palette_widget_proc)(palette_storage, widget);
  } else {
    const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
    if (!source_palette) {
      return false;
    }
    reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
  }

  if (dark) {
    apply_blender_palette_roles(lookup, palette_storage);
    if (lookup.widget_set_background_role_proc) {
      reinterpret_cast<QtWidgetSetRoleFn>(lookup.widget_set_background_role_proc)(widget, 9);
    }
    if (lookup.widget_set_foreground_role_proc) {
      reinterpret_cast<QtWidgetSetRoleFn>(lookup.widget_set_foreground_role_proc)(widget, 0);
    }
  }

  reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(widget, palette_storage);
  reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  return true;
}

bool apply_entity_info_line_edit_stylesheet(const QtLookup& lookup, void* widget, bool dark) {
  if (!widget || !lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc || !lookup.qstring_destructor_proc) {
    return false;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtDestructorFn = void(__cdecl*)(void*);

  const char* stylesheet = dark ? R"QSS(
QtFocusReturnerLineEdit {
  background-color: #1f1f1f;
  color: #c5cad3;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
  border: 1px solid #3a3a3a;
}
QtFocusReturnerLineEdit:read-only {
  color: #a5abb5;
  selection-color: #d8dbe2;
}
)QSS" : "";
  alignas(16) unsigned char qstring_storage[256] = {};
  const size_t stylesheet_length = std::strlen(stylesheet);
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<long long>(stylesheet_length));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<int>(stylesheet_length));
  }
  reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(widget, qstring_storage);
  reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  return true;
}

bool function_matches_or_forwards_to(const void* candidate, const void* expected, int depth = 0) {
  if (candidate == expected) {
    return true;
  }
  if (!candidate || !expected || depth >= 2) {
    return false;
  }

  const auto* code = reinterpret_cast<const unsigned char*>(candidate);
  if (!memory_readable(code, 6)) {
    return false;
  }

  std::int32_t displacement = 0;
  if (code[0] == 0xff && code[1] == 0x25) {
    std::memcpy(&displacement, code + 2, sizeof(displacement));
    const void* iat_slot = code + 6 + displacement;
    if (!memory_readable(iat_slot, sizeof(void*))) {
      return false;
    }
    void* forwarded_target = nullptr;
    std::memcpy(&forwarded_target, iat_slot, sizeof(forwarded_target));
    return function_matches_or_forwards_to(forwarded_target, expected, depth + 1);
  }
  if (code[0] == 0xe9) {
    std::memcpy(&displacement, code + 1, sizeof(displacement));
    return function_matches_or_forwards_to(code + 5 + displacement, expected, depth + 1);
  }
  return false;
}

EntityInfoInputPaintFn original_entity_info_input_paint_proc(void* widget) {
  if (!widget || !memory_readable(widget, sizeof(void*))) {
    return nullptr;
  }
  void** vtable = *reinterpret_cast<void***>(widget);
  if (!vtable) {
    return nullptr;
  }
  for (size_t slot_index = 0; slot_index < 96; ++slot_index) {
    void** slot = &vtable[slot_index];
    const auto hook = std::find_if(g_entity_info_input_paint_hooks.begin(), g_entity_info_input_paint_hooks.end(), [slot](const EntityInfoInputPaintHook& entry) {
      return entry.slot == slot;
    });
    if (hook != g_entity_info_input_paint_hooks.end()) {
      return reinterpret_cast<EntityInfoInputPaintFn>(hook->original_proc);
    }
  }
  return nullptr;
}

void __fastcall entity_info_input_paint_hook(void* widget, void* event) {
  EntityInfoInputPaintFn original_proc = original_entity_info_input_paint_proc(widget);
  if (!original_proc) {
    return;
  }

  if (g_entity_info_input_paint_dark && !g_entity_info_input_palette_applying) {
    const QtLookup lookup = resolve_qt_lookup();
    if (is_entity_info_input_widget(lookup, widget)) {
      g_entity_info_input_palette_applying = true;
      if (lookup.widget_set_attribute_proc) {
        using QtWidgetSetAttributeFn = void(__cdecl*)(void*, int, bool);
        reinterpret_cast<QtWidgetSetAttributeFn>(lookup.widget_set_attribute_proc)(widget, 93, true);
      }
      if (lookup.widget_set_auto_fill_background_proc) {
        using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);
        reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(widget, true);
      }
      static_cast<void>(apply_entity_info_line_edit_stylesheet(lookup, widget, true));
      static_cast<void>(apply_entity_info_line_edit_palette(lookup, widget, true));
      if (InterlockedIncrement(&g_entity_info_input_paint_hook_invocation_count) == 1) {
      }
      original_proc(widget, event);
      g_entity_info_input_palette_applying = false;
      return;
    }
  }

  original_proc(widget, event);
}

bool install_entity_info_input_paint_hooks(const QtLookup& lookup) {
  std::string paint_module;
  std::string paint_symbol;
  FARPROC qlineedit_paint_proc = find_export(lookup.widgets_modules, {
    "?paintEvent@QLineEdit@@MEAAXPEAVQPaintEvent@@@Z"
  }, paint_module, paint_symbol);
  if (!qlineedit_paint_proc) {
    return false;
  }

  size_t installed = 0;
  for (const void* vtable_address : find_vtables_for_type(".?AVQtFocusReturnerLineEdit@@")) {
    void** vtable = const_cast<void**>(reinterpret_cast<void* const*>(vtable_address));
    if (!vtable || !memory_readable(vtable, 96 * sizeof(*vtable))) {
      continue;
    }
    for (size_t slot_index = 0; slot_index < 96; ++slot_index) {
      void** slot = &vtable[slot_index];
      if (!function_matches_or_forwards_to(*slot, reinterpret_cast<void*>(qlineedit_paint_proc)) ||
          std::any_of(g_entity_info_input_paint_hooks.begin(), g_entity_info_input_paint_hooks.end(), [slot](const EntityInfoInputPaintHook& entry) {
            return entry.slot == slot;
          })) {
        continue;
      }
      DWORD original_protection = 0;
      if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
        continue;
      }
      void* original_proc = *slot;
      *slot = reinterpret_cast<void*>(&entity_info_input_paint_hook);
      DWORD unused_protection = 0;
      VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
      g_entity_info_input_paint_hooks.push_back({slot, original_proc});
      installed += 1;
    }
  }

  return installed > 0 || !g_entity_info_input_paint_hooks.empty();
}

void remove_entity_info_input_paint_hooks() {
  g_entity_info_input_paint_dark = false;
  for (const EntityInfoInputPaintHook& hook : g_entity_info_input_paint_hooks) {
    if (!hook.slot || !hook.original_proc || !memory_readable(hook.slot, sizeof(*hook.slot))) {
      continue;
    }
    DWORD original_protection = 0;
    if (VirtualProtect(hook.slot, sizeof(*hook.slot), PAGE_READWRITE, &original_protection)) {
      if (*hook.slot == reinterpret_cast<void*>(&entity_info_input_paint_hook)) {
        *hook.slot = hook.original_proc;
      }
      DWORD unused_protection = 0;
      VirtualProtect(hook.slot, sizeof(*hook.slot), original_protection, &unused_protection);
    }
  }
  g_entity_info_input_paint_hooks.clear();
}

size_t apply_entity_info_input_surface(const QtLookup& lookup, bool dark) {
  if (!lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc || !lookup.qstring_destructor_proc) {
    return 0;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtDestructorFn = void(__cdecl*)(void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  using QtWidgetSetAttributeFn = void(__cdecl*)(void*, int, bool);
  using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);

  const char* stylesheet = dark ? R"QSS(
QtFocusReturnerLineEdit {
  background-color: #1f1f1f;
  color: #e6e8ee;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
}
QtFocusReturnerLineEdit:read-only,
QtFocusReturnerLineEdit:disabled {
  background-color: #1f1f1f;
  color: #c5cad3;
  selection-background-color: #4772b3;
  selection-color: #ffffff;
}
)QSS" : "";
  if (dark) {
    g_entity_info_input_paint_dark = true;
    InterlockedExchange(&g_entity_info_input_paint_hook_invocation_count, 0);
    install_entity_info_input_paint_hooks(lookup);
  } else {
    remove_entity_info_input_paint_hooks();
  }
  const std::vector<void*> dialogs = find_entity_info_dialog_widgets(lookup);
  size_t direct_palette_count = 0;
  size_t direct_stylesheet_count = 0;
  alignas(16) unsigned char qstring_storage[256] = {};
  const size_t stylesheet_length = std::strlen(stylesheet);
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<long long>(stylesheet_length));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<int>(stylesheet_length));
  }

  for (void* dialog : dialogs) {
    reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(dialog, qstring_storage);

    std::vector<void*> descendants;
    std::unordered_set<std::uintptr_t> visited;
    collect_qwidget_descendants_from_qobject(lookup, dialog, 0, 16, descendants, 256, visited, "Entity Info inputs", true);
    for (size_t index = 0; index < descendants.size(); ++index) {
      void* widget = descendants[index];
      if (!qt_object_inherits(lookup, widget, "QLineEdit")) {
        continue;
      }
      if (lookup.widget_set_attribute_proc) {
        reinterpret_cast<QtWidgetSetAttributeFn>(lookup.widget_set_attribute_proc)(widget, 93, dark);
      }
      if (lookup.widget_set_auto_fill_background_proc) {
        reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(widget, dark);
      }
      if (apply_entity_info_line_edit_stylesheet(lookup, widget, dark)) {
        direct_stylesheet_count += 1;
      }
      if (apply_entity_info_line_edit_palette(lookup, widget, dark)) {
        direct_palette_count += 1;
      }
      if (lookup.widget_update_proc) {
        reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
      }
      if (lookup.widget_repaint_proc) {
        reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
      }
    }

    if (lookup.widget_update_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(dialog);
    }
    if (lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(dialog);
    }
  }
  reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);

  return dialogs.size();
}

std::vector<void*> find_overlays_dialog_widgets(const QtLookup& lookup) {
  std::vector<void*> matches;
  const std::vector<const void*> vtables = find_vtables_for_type(".?AVCModelServiceDialog@@");
  if (!lookup.object_children_proc) {
    return matches;
  }

  std::string main_window_description;
  void* main_window = find_main_window_qwidget(lookup, main_window_description);
  if (!main_window) {
    return matches;
  }

  std::vector<void*> widgets;
  std::unordered_set<std::uintptr_t> visited;
  collect_qwidget_descendants_from_qobject(lookup, main_window, 0, 32, widgets, 1200, visited, "Overlays dialog", true);
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }
    const void* vtable = *reinterpret_cast<const void* const*>(widget);
    if (std::find(vtables.begin(), vtables.end(), vtable) != vtables.end() ||
        msvc_rtti_type_name(widget) == ".?AVCModelServiceDialog@@") {
      add_unique_pointer(matches, widget);
    }
  }

  return matches;
}

std::vector<void*> find_overlays_table_list_views(const QtLookup& lookup) {
  std::vector<void*> list_views;
  const std::vector<void*> dialogs = find_overlays_dialog_widgets(lookup);
  for (void* dialog : dialogs) {
    std::vector<void*> widgets;
    std::unordered_set<std::uintptr_t> visited;
    collect_qwidget_descendants_from_qobject(lookup, dialog, 0, 16, widgets, 256, visited, "Overlays table", true);
    for (void* widget : widgets) {
      if (qt_object_inherits(lookup, widget, "QListView")) {
        add_unique_pointer(list_views, widget);
      }
    }
  }

  return list_views;
}

size_t apply_overlays_native_surface(const QtLookup& lookup, bool dark) {
  if (!lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc || !lookup.qstring_destructor_proc) {
    return 0;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtDestructorFn = void(__cdecl*)(void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);

  const char* stylesheet = dark ? R"QSS(
QWidget {
  background-color: #2b2b2b;
  color: #d8dbe2;
}
QAbstractScrollArea, QAbstractScrollArea::viewport {
  background-color: #2b2b2b;
  color: #d8dbe2;
}
)QSS" : "";
  const std::vector<void*> dialogs = find_overlays_dialog_widgets(lookup);
  alignas(16) unsigned char qstring_storage[256] = {};
  const size_t stylesheet_length = std::strlen(stylesheet);
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<long long>(stylesheet_length));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<int>(stylesheet_length));
  }

  for (void* dialog : dialogs) {
    reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(dialog, qstring_storage);
    if (lookup.widget_update_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(dialog);
    }
    if (lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(dialog);
    }
  }
  reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);

  return dialogs.size();
}

void* item_delegate_for_item_view(const QtLookup& lookup, const void* item_view) {
  if (!item_view || !lookup.abstract_item_view_item_delegate_proc) {
    return nullptr;
  }

  using QtItemDelegateFn = void*(__cdecl*)(const void*);
  void* delegate = nullptr;
  __try {
    delegate = reinterpret_cast<QtItemDelegateFn>(lookup.abstract_item_view_item_delegate_proc)(item_view);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    delegate = nullptr;
  }
  return delegate;
}

std::vector<void*> find_component_navigator_tree_widgets(const QtLookup& lookup, int stage) {
  std::vector<void*> tree_widgets;
  const std::vector<const void*>& delegate_vtables = find_component_navigator_delegate_vtables();
  if (delegate_vtables.empty() || !lookup.abstract_item_view_item_delegate_proc) {
    return tree_widgets;
  }

  using QtBoolWidgetFn = bool(__cdecl*)(const void*);
  size_t item_view_count = 0;
  size_t delegate_match_count = 0;
  std::vector<void*> tray_widgets;
  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  for (size_t root_index = 0; root_index < tray_roots.size() && root_index < 4; ++root_index) {
    void* tray_widget = qwidget_for_hwnd(lookup, tray_roots[root_index]);
    std::unordered_set<std::uintptr_t> visited;
    collect_qwidget_descendants_from_qobject(
      lookup,
      tray_widget,
      0,
      8,
      tray_widgets,
      256,
      visited,
      "stage " + std::to_string(stage) + " Components",
      true
    );
  }
  for (void* widget : tray_widgets) {
    if (!pointer_looks_like_cpp_object(widget) ||
        !qt_object_inherits(lookup, widget, "QAbstractItemView") ||
        (lookup.widget_is_visible_proc && !reinterpret_cast<QtBoolWidgetFn>(lookup.widget_is_visible_proc)(widget))) {
      continue;
    }

    item_view_count += 1;
    void* delegate = item_delegate_for_item_view(lookup, widget);
    if (!pointer_looks_like_cpp_object(delegate)) {
      continue;
    }

    const void* delegate_vtable = *reinterpret_cast<const void* const*>(delegate);
    if (std::find(delegate_vtables.begin(), delegate_vtables.end(), delegate_vtable) == delegate_vtables.end()) {
      continue;
    }
    delegate_match_count += 1;
    add_unique_pointer(tree_widgets, widget);
  }
  return tree_widgets;
}

size_t apply_component_navigator_tree_palette(const QtLookup& lookup, bool dark, int stage) {
  const std::vector<void*> tree_widgets = find_component_navigator_tree_widgets(lookup, stage);
  size_t applied = 0;
  for (size_t index = 0; index < tree_widgets.size(); ++index) {
    if (apply_clicked_palette_to_widget(lookup, tree_widgets[index], dark)) {
      applied += 1;
    }
    if (lookup.widget_update_proc) {
      using QtWidgetNoArgFn = void(__cdecl*)(void*);
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(tree_widgets[index]);
    }
    if (lookup.widget_repaint_proc) {
      using QtWidgetNoArgFn = void(__cdecl*)(void*);
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(tree_widgets[index]);
    }
  }
  return applied;
}

bool apply_vcb_edit_palette(const QtLookup& lookup, void* widget, bool dark) {
  if (!widget || !lookup.widget_set_palette_proc || !lookup.widget_palette_proc || !lookup.qpalette_copy_ctor_proc || !lookup.qpalette_destructor_proc) {
    return false;
  }

  using QtWidgetPaletteFn = const void*(__cdecl*)(const void*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtWidgetSetRoleFn = void(__cdecl*)(void*, int);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteReturnFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  if (!dark && lookup.app_palette_widget_proc) {
    reinterpret_cast<QtPaletteReturnFn>(lookup.app_palette_widget_proc)(palette_storage, widget);
  } else {
    const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
    if (!source_palette) {
      return false;
    }
    reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
  }

  if (dark) {
    const PaletteRoleColor colors[] = {
      {0, 240, 242, 247},
      {6, 240, 242, 247},
      {8, 240, 242, 247},
      {9, 0, 0, 0},
      {10, 0, 0, 0},
      {16, 0, 0, 0},
      {18, 0, 0, 0}
    };
    for (const PaletteRoleColor& color : colors) {
      set_qpalette_role_color(lookup, palette_storage, color);
    }
    if (lookup.widget_set_background_role_proc) {
      reinterpret_cast<QtWidgetSetRoleFn>(lookup.widget_set_background_role_proc)(widget, 9);
    }
    if (lookup.widget_set_foreground_role_proc) {
      reinterpret_cast<QtWidgetSetRoleFn>(lookup.widget_set_foreground_role_proc)(widget, 6);
    }
  }

  reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(widget, palette_storage);
  reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  return true;
}

using VcbEditPaintFn = void(__fastcall*)(void*, void*);
constexpr size_t kVcbEditTypedInputBackgroundOffset = 0x40;

VcbEditPaintFn original_vcb_edit_paint_proc(void* widget) {
  if (!widget || !memory_readable(widget, sizeof(void*))) {
    return nullptr;
  }
  void** vtable = *reinterpret_cast<void***>(widget);
  if (!vtable || !memory_readable(vtable, 28 * sizeof(*vtable))) {
    return nullptr;
  }
  void** paint_slot = &vtable[27];
  const auto hook = std::find_if(g_vcb_edit_paint_hooks.begin(), g_vcb_edit_paint_hooks.end(), [paint_slot](const VcbStatusPaintHook& entry) {
    return entry.slot == paint_slot;
  });
  return hook == g_vcb_edit_paint_hooks.end()
    ? nullptr
    : reinterpret_cast<VcbEditPaintFn>(hook->original_proc);
}

void __fastcall vcb_edit_paint_hook(void* widget, void* event) {
  VcbEditPaintFn original_proc = original_vcb_edit_paint_proc(widget);
  if (!original_proc) {
    return;
  }
  if (g_vcb_edit_paint_dark) {
    static_cast<void>(apply_vcb_edit_palette(resolve_qt_lookup(), widget, true));
  }
  auto* typed_input_background = reinterpret_cast<unsigned char*>(widget) + kVcbEditTypedInputBackgroundOffset;
  const bool suppress_direct_white = g_vcb_edit_paint_dark && memory_readable(typed_input_background, sizeof(*typed_input_background));
  unsigned char original_typed_input_background = 0;
  if (suppress_direct_white) {
    original_typed_input_background = *typed_input_background;
    *typed_input_background = 0;
  }
  if (InterlockedIncrement(&g_vcb_edit_paint_hook_invocation_count) == 1) {
  }

  original_proc(widget, event);

  if (suppress_direct_white && memory_readable(typed_input_background, sizeof(*typed_input_background))) {
    *typed_input_background = original_typed_input_background;
  }
}

bool install_vcb_edit_paint_hook(void* widget) {
  if (!widget || !memory_readable(widget, sizeof(void*))) {
    return false;
  }
  void** vtable = *reinterpret_cast<void***>(widget);
  if (!vtable || !memory_readable(vtable, 28 * sizeof(*vtable))) {
    return false;
  }
  void** paint_slot = &vtable[27];
  if (std::any_of(g_vcb_edit_paint_hooks.begin(), g_vcb_edit_paint_hooks.end(), [paint_slot](const VcbStatusPaintHook& entry) {
        return entry.slot == paint_slot;
      })) {
    return true;
  }

  DWORD original_protection = 0;
  if (!VirtualProtect(paint_slot, sizeof(*paint_slot), PAGE_READWRITE, &original_protection)) {
    return false;
  }
  void* original_proc = *paint_slot;
  *paint_slot = reinterpret_cast<void*>(&vcb_edit_paint_hook);
  DWORD unused_protection = 0;
  VirtualProtect(paint_slot, sizeof(*paint_slot), original_protection, &unused_protection);
  g_vcb_edit_paint_hooks.push_back({paint_slot, original_proc});
  return true;
}

void remove_vcb_edit_paint_hooks() {
  g_vcb_edit_paint_dark = false;
  for (const VcbStatusPaintHook& hook : g_vcb_edit_paint_hooks) {
    if (!hook.slot || !hook.original_proc || !memory_readable(hook.slot, sizeof(*hook.slot))) {
      continue;
    }
    DWORD original_protection = 0;
    if (VirtualProtect(hook.slot, sizeof(*hook.slot), PAGE_READWRITE, &original_protection)) {
      if (*hook.slot == reinterpret_cast<void*>(&vcb_edit_paint_hook)) {
        *hook.slot = hook.original_proc;
      }
      DWORD unused_protection = 0;
      VirtualProtect(hook.slot, sizeof(*hook.slot), original_protection, &unused_protection);
    }
  }
  g_vcb_edit_paint_hooks.clear();
}

size_t apply_vcb_edit_black_surface(const QtLookup& lookup, bool dark) {
  if (!dark) {
    remove_vcb_edit_paint_hooks();
  } else {
    g_vcb_edit_paint_dark = true;
    InterlockedExchange(&g_vcb_edit_paint_hook_invocation_count, 0);
  }
  if (!lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc || !lookup.qstring_destructor_proc) {
    return 0;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  using QtDestructorFn = void(__cdecl*)(void*);

  const char* stylesheet = dark ? "QWidget { background-color: #000000; color: #f0f2f7; }" : "";
  const size_t stylesheet_length = std::strlen(stylesheet);
  const std::vector<void*> widgets = find_vcb_edit_widgets(lookup);
  size_t applied = 0;
  size_t paint_hooks = 0;
  for (void* widget : widgets) {
    alignas(16) unsigned char qstring_storage[256] = {};
    if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
      reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<long long>(stylesheet_length));
    } else {
      reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(qstring_storage, stylesheet, static_cast<int>(stylesheet_length));
    }

    reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(widget, qstring_storage);
    reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
    if (lookup.widget_set_auto_fill_background_proc) {
      reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(widget, dark);
    }
    if (apply_vcb_edit_palette(lookup, widget, dark)) {
      applied += 1;
    }
    if (dark && install_vcb_edit_paint_hook(widget)) {
      paint_hooks += 1;
    }
    if (lookup.widget_update_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    }
    if (lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    }
  }

  return applied;
}

bool apply_vcb_status_paint_palette(const QtLookup& lookup, void* widget) {
  if (!widget || !lookup.widget_palette_proc || !lookup.widget_set_palette_proc || !lookup.qpalette_copy_ctor_proc ||
      !lookup.qpalette_destructor_proc || !lookup.qcolor_rgb_ctor_proc ||
      (!lookup.qpalette_set_color_group_proc && !lookup.qpalette_set_color_role_proc)) {
    return false;
  }

  if (g_vcb_status_paint_palette_applied && g_vcb_status_paint_widget == widget) {
    return true;
  }

  using QtWidgetPaletteFn = const void*(__cdecl*)(const void*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
  if (!source_palette) {
    return false;
  }

  reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
  const PaletteRoleColor background_colors[] = {
    {10, 0, 0, 0},
    {18, 0, 0, 0}
  };
  bool applied = true;
  for (const PaletteRoleColor& color : background_colors) {
    applied = set_qpalette_role_color(lookup, palette_storage, color) && applied;
  }
  if (applied) {
    reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(widget, palette_storage);
    g_vcb_status_paint_widget = widget;
    g_vcb_status_paint_palette_applied = true;
  }
  reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  return applied;
}

void restore_vcb_status_paint_palette(const QtLookup& lookup) {
  if (!g_vcb_status_paint_palette_applied || !g_vcb_status_paint_widget || !lookup.app_palette_class_proc ||
      !lookup.widget_set_palette_proc || !lookup.qpalette_destructor_proc) {
    g_vcb_status_paint_widget = nullptr;
    g_vcb_status_paint_palette_applied = false;
    return;
  }

  using QtAppPaletteClassFn = void(__cdecl*)(void*, const char*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  reinterpret_cast<QtAppPaletteClassFn>(lookup.app_palette_class_proc)(palette_storage, nullptr);
  reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(g_vcb_status_paint_widget, palette_storage);
  reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  if (lookup.widget_update_proc) {
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(g_vcb_status_paint_widget);
  }
  if (lookup.widget_repaint_proc) {
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(g_vcb_status_paint_widget);
  }
  g_vcb_status_paint_widget = nullptr;
  g_vcb_status_paint_palette_applied = false;
}

using VcbStatusPaintFn = void(__fastcall*)(void*, void*);

void __fastcall vcb_status_paint_hook(void* widget, void* event) {
  VcbStatusPaintFn original_proc = nullptr;
  if (!g_vcb_status_paint_hooks.empty()) {
    original_proc = reinterpret_cast<VcbStatusPaintFn>(g_vcb_status_paint_hooks.front().original_proc);
  }
  if (!original_proc) {
    return;
  }

  if (InterlockedIncrement(&g_vcb_status_paint_hook_invocation_count) == 1) {
  }

  unsigned char original_fill_color[16] = {};
  bool fill_color_overridden = false;
  if (g_vcb_status_paint_dark && widget) {
    const QtLookup lookup = resolve_qt_lookup();
    constexpr size_t fill_color_offset = 0x48;
    if (lookup.qcolor_set_rgb_proc && memory_readable(reinterpret_cast<unsigned char*>(widget) + fill_color_offset, sizeof(original_fill_color))) {
      std::memcpy(original_fill_color, reinterpret_cast<unsigned char*>(widget) + fill_color_offset, sizeof(original_fill_color));
      using QtColorSetRgbFn = void(__cdecl*)(void*, int, int, int, int);
      reinterpret_cast<QtColorSetRgbFn>(lookup.qcolor_set_rgb_proc)(
        reinterpret_cast<unsigned char*>(widget) + fill_color_offset, 0, 0, 0, 255);
      fill_color_overridden = true;
    }
  }

  original_proc(widget, event);

  if (fill_color_overridden) {
    std::memcpy(reinterpret_cast<unsigned char*>(widget) + 0x48, original_fill_color, sizeof(original_fill_color));
  }
}

void remove_vcb_status_paint_hooks(const QtLookup& lookup) {
  g_vcb_status_paint_dark = false;
  restore_vcb_status_paint_palette(lookup);
  for (const VcbStatusPaintHook& hook : g_vcb_status_paint_hooks) {
    if (!hook.slot || !hook.original_proc || !memory_readable(hook.slot, sizeof(*hook.slot))) {
      continue;
    }
    DWORD original_protection = 0;
    if (VirtualProtect(hook.slot, sizeof(*hook.slot), PAGE_READWRITE, &original_protection)) {
      if (*hook.slot == reinterpret_cast<void*>(&vcb_status_paint_hook)) {
        *hook.slot = hook.original_proc;
      }
      DWORD unused_protection = 0;
      VirtualProtect(hook.slot, sizeof(*hook.slot), original_protection, &unused_protection);
    }
  }
  g_vcb_status_paint_hooks.clear();
}

bool install_vcb_status_paint_hooks(const QtLookup& lookup, bool dark) {
  if (!dark) {
    remove_vcb_status_paint_hooks(lookup);
    return true;
  }

  g_vcb_status_paint_dark = true;
  InterlockedExchange(&g_vcb_status_paint_hook_invocation_count, 0);
  if (!g_vcb_status_paint_hooks.empty()) {
    return true;
  }

  HMODULE executable = GetModuleHandleW(nullptr);
  if (!executable) {
    return false;
  }

  constexpr size_t paint_event_slot = 27;
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base);
  const auto* nt_headers = memory_readable(dos_header, sizeof(*dos_header)) && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  const size_t image_size = nt_headers && memory_readable(nt_headers, sizeof(*nt_headers)) && nt_headers->Signature == IMAGE_NT_SIGNATURE
    ? nt_headers->OptionalHeader.SizeOfImage
    : 0;
  const std::vector<const void*> vtables = find_vcb_value_button_vtables();
  if (image_size == 0 || vtables.empty()) {
  }

  for (const void* vtable : vtables) {
    void** slots = const_cast<void**>(reinterpret_cast<void* const*>(vtable));
    if (!memory_readable(slots, (paint_event_slot + 1) * sizeof(*slots))) {
      continue;
    }

    const auto* paint_event = reinterpret_cast<const unsigned char*>(slots[paint_event_slot]);
    if (!paint_event || paint_event < image_base || paint_event >= image_base + image_size) {
      continue;
    }

    DWORD original_protection = 0;
    if (!VirtualProtect(&slots[paint_event_slot], sizeof(*slots), PAGE_READWRITE, &original_protection)) {
    } else {
      void* original_proc = slots[paint_event_slot];
      slots[paint_event_slot] = reinterpret_cast<void*>(&vcb_status_paint_hook);
      DWORD unused_protection = 0;
      VirtualProtect(&slots[paint_event_slot], sizeof(*slots), original_protection, &unused_protection);
      g_vcb_status_paint_hooks.push_back({&slots[paint_event_slot], original_proc});
    }
    break;
  }

  if (g_vcb_status_paint_hooks.empty()) {
    g_vcb_status_paint_dark = false;
    return false;
  }
  return true;
}

std::string cef_string_ascii_lower(const CefString* value) {
  if (!value || !value->str || value->length == 0 || value->length > 32768) {
    return "";
  }

  std::string text;
  text.reserve(value->length);
  for (size_t index = 0; index < value->length; ++index) {
    const char16_t character = value->str[index];
    text.push_back(character <= 0x7f ? static_cast<char>(character) : '?');
  }
  return lower_ascii(text);
}

bool cef_struct_has_member(const CefBaseRefCounted* object, size_t member_end) {
  return object && memory_readable(object, sizeof(object->size)) &&
    object->size >= member_end && object->size <= 4096;
}

bool is_model_services_url(const std::string& url) {
  return url.find("modelservice.html") != std::string::npos;
}

bool is_model_services_url(const CefString* url) {
  return is_model_services_url(cef_string_ascii_lower(url));
}

std::string cef_frame_url_lower(CefFrame* frame) {
  if (!frame || !cef_struct_has_member(&frame->base, offsetof(CefFrame, get_url) + sizeof(frame->get_url)) || !frame->get_url) {
    return "";
  }

  CefString* url = frame->get_url(frame);
  if (!url) {
    return "";
  }

  const std::string value = cef_string_ascii_lower(url);
  HMODULE libcef = GetModuleHandleW(L"libcef.dll");
  using CefStringUserfreeFreeFn = void(__cdecl*)(CefString*);
  const auto free_userfree = libcef
    ? reinterpret_cast<CefStringUserfreeFreeFn>(GetProcAddress(libcef, "cef_string_userfree_utf16_free"))
    : nullptr;
  if (free_userfree) {
    free_userfree(url);
  }
  return value;
}

std::u16string utf16_from_ascii(const char* value) {
  std::u16string result;
  if (!value) {
    return result;
  }
  for (const char* character = value; *character; ++character) {
    result.push_back(static_cast<char16_t>(*character));
  }
  return result;
}

const char* model_services_style_script(bool enabled) {
  return enabled
    ? R"DARKMODE((function(){var key='__darkModeSkpModelServicesObserver';var probeKey='__darkModeSkpModelServicesProbe';if(!window[probeKey]){window[probeKey]=true;if(window.sketchup&&typeof window.sketchup.get_l10n_strings==='function'){window.sketchup.get_l10n_strings({onCompleted:function(){}});}}var apply=function(){var background='rgb(43,43,43)';var headerBackground='rgb(48,48,48)';var divider='rgb(68,68,68)';var text='rgb(216,219,226)';var muted='rgb(160,164,172)';var roots=[document.documentElement,document.body,document.getElementById('app')];for(var rootIndex=0;rootIndex<roots.length;rootIndex++){var root=roots[rootIndex];if(root){root.style.setProperty('background-color',background,'important');root.style.setProperty('color',text,'important');}}var rows=document.querySelectorAll('.list-group.list-group-condensed .list-group-item');for(var rowIndex=0;rowIndex<rows.length;rowIndex++){rows[rowIndex].style.setProperty('background-color',background,'important');rows[rowIndex].style.setProperty('color',text,'important');}var header=document.querySelector('.su-service-header');if(header){header.style.setProperty('background-color',headerBackground,'important');header.style.setProperty('border-bottom-color',divider,'important');header.style.setProperty('color',text,'important');}var headerCells=document.querySelectorAll('.su-service-header .align-items-center > div');for(var headerIndex=0;headerIndex<headerCells.length;headerIndex++){headerCells[headerIndex].style.setProperty('background-color',headerBackground,'important');headerCells[headerIndex].style.setProperty('border-left-color',divider,'important');headerCells[headerIndex].style.setProperty('border-right-color',divider,'important');headerCells[headerIndex].style.setProperty('color',text,'important');}var mutedItems=document.querySelectorAll('.su-source,.su-drag-handle span');for(var mutedIndex=0;mutedIndex<mutedItems.length;mutedIndex++){mutedItems[mutedIndex].style.setProperty('color',muted,'important');}};window.setTimeout(function(){apply();if(!window[key]&&window.MutationObserver&&document.body){var observer=new MutationObserver(function(){apply();});observer.observe(document.body,{childList:true,subtree:true});window[key]=observer;}},0);})();)DARKMODE"
    : R"DARKMODE((function(){var key='__darkModeSkpModelServicesObserver';if(window[key]){window[key].disconnect();window[key]=null;}var roots=[document.documentElement,document.body,document.getElementById('app')];for(var rootIndex=0;rootIndex<roots.length;rootIndex++){var root=roots[rootIndex];if(root){root.style.removeProperty('background-color');root.style.removeProperty('color');}}var items=document.querySelectorAll('.list-group.list-group-condensed .list-group-item,.su-service-header,.su-service-header .align-items-center > div,.su-source,.su-drag-handle span');for(var itemIndex=0;itemIndex<items.length;itemIndex++){items[itemIndex].style.removeProperty('background-color');items[itemIndex].style.removeProperty('border-bottom-color');items[itemIndex].style.removeProperty('border-left-color');items[itemIndex].style.removeProperty('border-right-color');items[itemIndex].style.removeProperty('color');}})();)DARKMODE";
}

bool execute_model_services_style(CefFrame* frame, bool enabled) {
  if (!frame || !cef_struct_has_member(&frame->base, offsetof(CefFrame, execute_java_script) + sizeof(frame->execute_java_script)) ||
      !frame->execute_java_script) {
    return false;
  }

  const char* script = model_services_style_script(enabled);
  std::u16string script_utf16 = utf16_from_ascii(script);
  std::u16string source_utf16 = u"dark-mode-skp://model-services";
  CefString script_string = {script_utf16.data(), script_utf16.size(), nullptr};
  CefString source_string = {source_utf16.data(), source_utf16.size(), nullptr};
  frame->execute_java_script(frame, &script_string, &source_string, 1);
  return true;
}

bool is_main_executable_code_address(const void* address) {
  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base && memory_readable(image_base, sizeof(IMAGE_DOS_HEADER))
    ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base)
    : nullptr;
  const auto* nt_headers = dos_header && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  if (!nt_headers || !memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return false;
  }

  const auto address_value = reinterpret_cast<std::uintptr_t>(address);
  const auto image_value = reinterpret_cast<std::uintptr_t>(image_base);
  if (address_value < image_value || address_value >= image_value + nt_headers->OptionalHeader.SizeOfImage) {
    return false;
  }

  const auto* section = IMAGE_FIRST_SECTION(nt_headers);
  for (WORD index = 0; index < nt_headers->FileHeader.NumberOfSections; ++index) {
    const IMAGE_SECTION_HEADER& current = section[index];
    const size_t section_size = std::max<size_t>(current.Misc.VirtualSize, current.SizeOfRawData);
    const auto section_begin = image_value + current.VirtualAddress;
    if (address_value >= section_begin && address_value < section_begin + section_size) {
      return (current.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    }
  }
  return false;
}

const unsigned char* find_model_services_update_services_literal(const unsigned char* image_base, size_t image_size) {
  static constexpr char literal[] = "update_services";
  if (!image_base || image_size < sizeof(literal) - 1) {
    return nullptr;
  }
  const auto* result = std::search(image_base, image_base + image_size, literal, literal + sizeof(literal) - 1);
  return result == image_base + image_size ? nullptr : result;
}

bool function_references_model_services_literal(
  const unsigned char* function_begin,
  const unsigned char* function_end,
  const unsigned char* literal
) {
  if (!function_begin || !function_end || function_end <= function_begin || !literal) {
    return false;
  }
  for (const auto* instruction = function_begin; instruction + 7 <= function_end; ++instruction) {
    if ((instruction[0] == 0x48 || instruction[0] == 0x4c) && instruction[1] == 0x8d &&
        (instruction[2] & 0xc7) == 0x05 && rip_relative_target(instruction, 7) == literal) {
      return true;
    }
  }
  return false;
}

const unsigned char* find_model_services_script_bridge_call(
  const unsigned char* instruction,
  const unsigned char* function_end
) {
  static constexpr unsigned char prefix[] = {
    0x48, 0x8b, 0x4e, 0x08, 0x48, 0x85, 0xc9, 0x74
  };
  if (!instruction || !function_end || function_end - instruction < 14 ||
      !std::equal(std::begin(prefix), std::end(prefix), instruction)) {
    return nullptr;
  }

  const auto* lea = instruction + 9;
  size_t lea_size = 0;
  if (lea + 5 <= function_end && lea[0] == 0x48 && lea[1] == 0x8d && lea[2] == 0x54 && lea[3] == 0x24) {
    lea_size = 5;
  } else if (lea + 8 <= function_end && lea[0] == 0x48 && lea[1] == 0x8d && lea[2] == 0x94 && lea[3] == 0x24) {
    lea_size = 8;
  } else {
    return nullptr;
  }

  const auto* call = lea + lea_size;
  return call + 5 <= function_end && call[0] == 0xe8 ? call : nullptr;
}

void* find_atlast_webview_javascript_bridge() {
  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base && memory_readable(image_base, sizeof(IMAGE_DOS_HEADER))
    ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base)
    : nullptr;
  const auto* nt_headers = dos_header && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  if (!nt_headers || !memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return nullptr;
  }

  const size_t image_size = nt_headers->OptionalHeader.SizeOfImage;
  const auto* update_services_literal = find_model_services_update_services_literal(image_base, image_size);
  if (!update_services_literal) {
    return nullptr;
  }

  const IMAGE_DATA_DIRECTORY& exception_directory = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
  if (exception_directory.VirtualAddress == 0 || exception_directory.Size < sizeof(RUNTIME_FUNCTION) ||
      exception_directory.VirtualAddress >= image_size ||
      exception_directory.Size > image_size - exception_directory.VirtualAddress) {
    return nullptr;
  }

  const auto* functions = reinterpret_cast<const RUNTIME_FUNCTION*>(image_base + exception_directory.VirtualAddress);
  const size_t function_count = exception_directory.Size / sizeof(*functions);
  if (!memory_readable(functions, function_count * sizeof(*functions))) {
    return nullptr;
  }

  void* bridge = nullptr;
  size_t callers = 0;
  size_t matches = 0;
  for (size_t index = 0; index < function_count; ++index) {
    const RUNTIME_FUNCTION& function = functions[index];
    if (function.BeginAddress >= image_size || function.EndAddress <= function.BeginAddress ||
        function.EndAddress > image_size || function.EndAddress - function.BeginAddress < 14) {
      continue;
    }
    const auto* function_begin = image_base + function.BeginAddress;
    const auto* function_end = image_base + function.EndAddress;
    if (!function_references_model_services_literal(function_begin, function_end, update_services_literal)) {
      continue;
    }
    callers += 1;
    for (const auto* instruction = function_begin; instruction + 14 <= function_end; ++instruction) {
      const auto* call = find_model_services_script_bridge_call(instruction, function_end);
      if (!call) {
        continue;
      }
      std::int32_t displacement = 0;
      std::memcpy(&displacement, call + 1, sizeof(displacement));
      const auto* candidate = reinterpret_cast<const unsigned char*>(
        reinterpret_cast<std::intptr_t>(call + 5) + displacement);
      if (!is_main_executable_code_address(candidate)) {
        continue;
      }
      bridge = const_cast<unsigned char*>(candidate);
      matches += 1;
    }
  }

  return matches == 1 ? bridge : nullptr;
}

std::vector<void*> find_model_services_atlast_webviews(const QtLookup& lookup) {
  constexpr size_t object_scan_bytes = 0x200;
  std::vector<void*> webviews;
  const std::vector<const void*> webview_vtables = find_vtables_for_type(".?AVWebView@webengine@atlast@@");
  const std::vector<void*> dialogs = find_overlays_dialog_widgets(lookup);
  for (void* dialog : dialogs) {
    if (!pointer_looks_like_cpp_object(dialog)) {
      continue;
    }
    const auto* dialog_bytes = reinterpret_cast<const unsigned char*>(dialog);
    for (size_t offset = 0; offset <= object_scan_bytes; offset += sizeof(void*)) {
      const auto* field = dialog_bytes + offset;
      if (!memory_readable(field, sizeof(void*))) {
        continue;
      }
      void* webview = *reinterpret_cast<void* const*>(field);
      if (!pointer_looks_like_cpp_object(webview)) {
        continue;
      }
      const void* vtable = *reinterpret_cast<const void* const*>(webview);
      if (std::find(webview_vtables.begin(), webview_vtables.end(), vtable) != webview_vtables.end()) {
        add_unique_pointer(webviews, webview);
      }
    }
  }

  return webviews;
}

bool execute_atlast_webview_javascript(void* bridge, void* webview, const std::u16string& script) {
  using AtlastWebViewExecuteJavaScriptFn = void(__fastcall*)(void*, const void*);
  if (!bridge || !pointer_looks_like_cpp_object(webview) || script.empty() || !is_main_executable_code_address(bridge)) {
    return false;
  }
  __try {
    reinterpret_cast<AtlastWebViewExecuteJavaScriptFn>(bridge)(webview, &script);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

size_t apply_model_services_style_to_live_webviews(const QtLookup& lookup, bool enabled) {
  const std::vector<void*> dialogs = find_overlays_dialog_widgets(lookup);
  const std::vector<void*> webviews = find_model_services_atlast_webviews(lookup);
  void* bridge = find_atlast_webview_javascript_bridge();
  if (!bridge) {
    return 0;
  }

  const std::u16string script = utf16_from_ascii(model_services_style_script(enabled));
  size_t applied = 0;
  for (void* webview : webviews) {
    applied += execute_atlast_webview_javascript(bridge, webview, script) ? 1 : 0;
  }

  return applied;
}

void track_model_services_frame(CefFrame* frame) {
  if (!frame || !cef_struct_has_member(&frame->base, sizeof(CefBaseRefCounted)) || !frame->base.add_ref) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
  if (std::find(g_model_services_frames.begin(), g_model_services_frames.end(), frame) != g_model_services_frames.end()) {
    return;
  }
  frame->base.add_ref(&frame->base);
  g_model_services_frames.push_back(frame);
}

size_t apply_model_services_style_to_tracked_frames(bool enabled) {
  std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
  size_t applied = 0;
  auto frame = g_model_services_frames.begin();
  while (frame != g_model_services_frames.end()) {
    CefFrame* current = *frame;
    const bool valid = current && cef_struct_has_member(&current->base, offsetof(CefFrame, is_valid) + sizeof(current->is_valid)) &&
      current->is_valid && reinterpret_cast<int(__fastcall*)(CefFrame*)>(current->is_valid)(current) != 0;
    if (!valid) {
      if (current && cef_struct_has_member(&current->base, sizeof(CefBaseRefCounted)) && current->base.release) {
        current->base.release(&current->base);
      }
      frame = g_model_services_frames.erase(frame);
      continue;
    }
    applied += execute_model_services_style(current, enabled) ? 1 : 0;
    ++frame;
  }
  return applied;
}

void __fastcall model_services_load_end_hook(CefLoadHandler* handler, void* browser, CefFrame* frame, int http_status_code) {
  void(__fastcall* original_proc)(CefLoadHandler*, void*, CefFrame*, int) = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
    const auto hook = std::find_if(g_model_services_load_end_hooks.begin(), g_model_services_load_end_hooks.end(), [handler](const CefLoadEndHook& entry) {
      return entry.handler == handler;
    });
    if (hook != g_model_services_load_end_hooks.end()) {
      original_proc = reinterpret_cast<void(__fastcall*)(CefLoadHandler*, void*, CefFrame*, int)>(hook->original_proc);
    }
  }
  if (original_proc) {
    original_proc(handler, browser, frame, http_status_code);
  }
  const bool main_frame = frame &&
    cef_struct_has_member(&frame->base, offsetof(CefFrame, is_main) + sizeof(frame->is_main)) &&
    frame->is_main && frame->is_main(frame) != 0;
  const std::string frame_url = main_frame ? cef_frame_url_lower(frame) : "";
  if (!main_frame || !is_model_services_url(frame_url)) {
    return;
  }
  track_model_services_frame(frame);
  if (InterlockedCompareExchange(&g_model_services_cef_observer_only, 0, 0) != 0) {
    return;
  }
  static_cast<void>(execute_model_services_style(frame, InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) != 0));
}

bool hook_model_services_load_handler(CefClient* client) {
  if (!client || !cef_struct_has_member(&client->base, offsetof(CefClient, get_load_handler) + sizeof(client->get_load_handler)) || !client->get_load_handler) {
    return false;
  }

  CefLoadHandler* handler = client->get_load_handler(client);
  if (!handler || !cef_struct_has_member(&handler->base, offsetof(CefLoadHandler, on_load_end) + sizeof(handler->on_load_end)) || !handler->on_load_end) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
  const auto existing = std::find_if(g_model_services_load_end_hooks.begin(), g_model_services_load_end_hooks.end(), [handler](const CefLoadEndHook& entry) {
    return entry.handler == handler;
  });
  if (existing != g_model_services_load_end_hooks.end()) {
    return true;
  }

  DWORD original_protection = 0;
  if (!VirtualProtect(&handler->on_load_end, sizeof(handler->on_load_end), PAGE_READWRITE, &original_protection)) {
    return false;
  }
  void* original_proc = reinterpret_cast<void*>(handler->on_load_end);
  handler->on_load_end = &model_services_load_end_hook;
  DWORD unused_protection = 0;
  VirtualProtect(&handler->on_load_end, sizeof(handler->on_load_end), original_protection, &unused_protection);
  g_model_services_load_end_hooks.push_back({handler, original_proc});
  return true;
}

void** find_cef_import_iat_slot(const char* target_function_name) {
  if (!target_function_name || target_function_name[0] == '\0') {
    return nullptr;
  }
  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base && memory_readable(image_base, sizeof(IMAGE_DOS_HEADER))
    ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base)
    : nullptr;
  const auto* nt_headers = dos_header && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  if (!nt_headers || !memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return nullptr;
  }

  const IMAGE_DATA_DIRECTORY& imports = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (imports.VirtualAddress == 0 || imports.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
      !memory_readable(image_base + imports.VirtualAddress, imports.Size)) {
    return nullptr;
  }

  const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image_base + imports.VirtualAddress);
  const size_t descriptor_count = imports.Size / sizeof(*descriptors);
  for (size_t descriptor_index = 0; descriptor_index < descriptor_count; ++descriptor_index) {
    const IMAGE_IMPORT_DESCRIPTOR& descriptor = descriptors[descriptor_index];
    if (descriptor.Name == 0 || descriptor.FirstThunk == 0) {
      break;
    }
    const char* module_name = reinterpret_cast<const char*>(image_base + descriptor.Name);
    if (!memory_readable(module_name, 1) || lower_ascii(module_name) != "libcef.dll" || descriptor.OriginalFirstThunk == 0) {
      continue;
    }

    const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(image_base + descriptor.OriginalFirstThunk);
    auto* functions = reinterpret_cast<IMAGE_THUNK_DATA64*>(const_cast<unsigned char*>(image_base) + descriptor.FirstThunk);
    for (size_t function_index = 0; memory_readable(names + function_index, sizeof(*names)) && memory_readable(functions + function_index, sizeof(*functions)); ++function_index) {
      const ULONGLONG name_entry = names[function_index].u1.AddressOfData;
      if (name_entry == 0) {
        break;
      }
      if (IMAGE_SNAP_BY_ORDINAL64(name_entry)) {
        continue;
      }
      const auto* import_name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(image_base + name_entry);
      if (memory_readable(import_name, sizeof(*import_name)) && std::strcmp(reinterpret_cast<const char*>(import_name->Name), target_function_name) == 0) {
        return reinterpret_cast<void**>(&functions[function_index].u1.Function);
      }
    }
  }
  return nullptr;
}

int __fastcall cef_browser_host_create_browser_hook(
  const void* window_info,
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
) {
  if (client) {
  }
  return g_original_cef_browser_create
    ? g_original_cef_browser_create(window_info, client, url, settings, extra_info, request_context)
    : 0;
}

void* __fastcall cef_browser_host_create_browser_sync_hook(
  const void* window_info,
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
) {
  if (client) {
  }
  return g_original_cef_browser_create_sync
    ? g_original_cef_browser_create_sync(window_info, client, url, settings, extra_info, request_context)
    : nullptr;
}

void* __fastcall cef_browser_view_create_hook(
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
) {
  if (client) {
  }
  return g_original_cef_browser_view_create
    ? g_original_cef_browser_view_create(client, url, settings, extra_info, request_context)
    : nullptr;
}

bool install_model_services_cef_hook() {
  std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
  if (g_cef_browser_create_iat_slot && g_original_cef_browser_create &&
      g_cef_browser_create_sync_iat_slot && g_original_cef_browser_create_sync &&
      g_cef_browser_view_create_iat_slot && g_original_cef_browser_view_create) {
    return true;
  }

  auto install_iat_hook = [](void** slot, void* replacement) -> bool {
    if (!slot || !memory_readable(slot, sizeof(*slot)) || !*slot) {
      return false;
    }
    DWORD original_protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
      return false;
    }
    *slot = replacement;
    DWORD unused_protection = 0;
    VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
    return true;
  };

  bool async_installed = g_cef_browser_create_iat_slot && g_original_cef_browser_create;
  if (!async_installed) {
    void** slot = find_cef_import_iat_slot("cef_browser_host_create_browser");
    if (slot && memory_readable(slot, sizeof(*slot)) && *slot) {
      g_original_cef_browser_create = reinterpret_cast<CefBrowserHostCreateBrowserFn>(*slot);
    }
    async_installed = install_iat_hook(slot, reinterpret_cast<void*>(&cef_browser_host_create_browser_hook));
    if (async_installed) {
      g_cef_browser_create_iat_slot = slot;
    } else {
      g_original_cef_browser_create = nullptr;
    }
  }

  bool sync_installed = g_cef_browser_create_sync_iat_slot && g_original_cef_browser_create_sync;
  if (!sync_installed) {
    void** slot = find_cef_import_iat_slot("cef_browser_host_create_browser_sync");
    if (slot && memory_readable(slot, sizeof(*slot)) && *slot) {
      g_original_cef_browser_create_sync = reinterpret_cast<CefBrowserHostCreateBrowserSyncFn>(*slot);
    }
    sync_installed = install_iat_hook(slot, reinterpret_cast<void*>(&cef_browser_host_create_browser_sync_hook));
    if (sync_installed) {
      g_cef_browser_create_sync_iat_slot = slot;
    } else {
      g_original_cef_browser_create_sync = nullptr;
    }
  }

  bool view_installed = g_cef_browser_view_create_iat_slot && g_original_cef_browser_view_create;
  if (!view_installed) {
    void** slot = find_cef_import_iat_slot("cef_browser_view_create");
    if (slot && memory_readable(slot, sizeof(*slot)) && *slot) {
      g_original_cef_browser_view_create = reinterpret_cast<CefBrowserViewCreateFn>(*slot);
    }
    view_installed = install_iat_hook(slot, reinterpret_cast<void*>(&cef_browser_view_create_hook));
    if (view_installed) {
      g_cef_browser_view_create_iat_slot = slot;
    } else {
      g_original_cef_browser_view_create = nullptr;
    }
  }

  return async_installed || sync_installed || view_installed;
}

bool probe_qt_stage(int stage) {
  using QtInstanceFn = void*(__cdecl*)();
  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtWidgetSetAttributeFn = void(__cdecl*)(void*, int, bool);
  using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);

  if (stage != 45 && stage != 46 && stage != 47 && stage != 48) {
    set_theme_info("Dark Mode SKP helper: unsupported production stage " + std::to_string(stage));
    return false;
  }

  const QtLookup lookup = resolve_qt_lookup();

  if (stage == 47 || stage == 48) {
    const size_t changed_count = apply_windows_dark_mode(stage == 47);
    set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + " " + std::string(changed_count > 0 ? "OK: " : "failed: ") + "win32 dark hwnd count=" + std::to_string(changed_count));
    return changed_count > 0;
  }

  if (lookup.core_modules.empty() || lookup.widgets_modules.empty()) {
    set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + ": Qt Core/Widgets modules are not loaded. " + describe_qt_lookup(lookup));
    return false;
  }

  if (!lookup.instance_proc || !lookup.from_utf8_proc) {
    set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + ": Qt application/string symbols incomplete. " + describe_qt_lookup(lookup));
    return false;
  }

  void* application = reinterpret_cast<QtInstanceFn>(lookup.instance_proc)();
  if (!application) {
    set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + ": QCoreApplication::instance returned null.");
    return false;
  }

  if (!lookup.widget_find_proc || !lookup.widget_set_palette_proc || !lookup.qpalette_destructor_proc ||
      (!lookup.app_palette_widget_proc && (!lookup.widget_palette_proc || !lookup.qpalette_copy_ctor_proc)) ||
      (stage == 45 && (!lookup.qcolor_rgb_ctor_proc || (!lookup.qpalette_set_color_group_proc && !lookup.qpalette_set_color_role_proc)))) {
    set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + ": palette symbols incomplete. " + describe_qt_lookup(lookup));
    return false;
  }

  auto build_qstring = [&](const char* text, void* storage) -> bool {
    const size_t text_len = std::strlen(text);
    if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
      reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(storage, text, static_cast<long long>(text_len));
    } else {
      reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(storage, text, static_cast<int>(text_len));
    }
    return true;
  };

  alignas(16) unsigned char qstring_storage[256] = {};
  build_qstring("", qstring_storage);

  const char* separator_stylesheet = (stage == 45) ? separator_splitter_dark_qt_stylesheet() : "";
  const size_t separator_stylesheet_len = std::strlen(separator_stylesheet);
  alignas(16) unsigned char separator_qstring_storage[256] = {};
  if (separator_stylesheet_len > 0) {
    build_qstring(separator_stylesheet, separator_qstring_storage);
  }

  const char* header_stylesheet = (stage == 45)
    ? "QHeaderView { background-color: #2b2b2b; color: #d8dbe2; } "
      "QHeaderView::section { background-color: #2b2b2b; color: #d8dbe2; border: none; "
      "border-right: 1px solid #3a3a3a; border-bottom: 1px solid #3a3a3a; }"
    : "";
  const size_t header_stylesheet_len = std::strlen(header_stylesheet);
  alignas(16) unsigned char header_qstring_storage[256] = {};
  if (header_stylesheet_len > 0) {
    build_qstring(header_stylesheet, header_qstring_storage);
  }

  static_cast<void>(apply_application_palette(lookup, stage == 45));
  const size_t vcb_edit_count = apply_vcb_edit_black_surface(lookup, stage == 45);

  std::vector<void*> targets = collect_auto_palette_targets(lookup, stage);
  if (targets.empty()) {
    set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + ": no auto palette QWidget targets found.");
    return false;
  }

  size_t applied_count = 0;
  size_t separator_applied = 0;
  size_t header_applied = 0;

  if (separator_stylesheet_len > 0 && lookup.widget_set_stylesheet_proc) {
    std::vector<void*> top_level_targets = collect_main_and_tray_top_level_qwidgets(lookup);
    for (void* top_widget : top_level_targets) {
      reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(top_widget, separator_qstring_storage);
      separator_applied += 1;
    }
  }

  for (size_t index = 0; index < targets.size(); ++index) {
    if (lookup.widget_set_stylesheet_proc && stage == 46) {
      reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(targets[index], qstring_storage);
    }
    if (header_stylesheet_len > 0 && lookup.widget_set_stylesheet_proc && qt_object_inherits(lookup, targets[index], "QHeaderView")) {
      reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(targets[index], header_qstring_storage);
      header_applied += 1;
    }
    if (lookup.widget_set_attribute_proc && lookup.widget_set_auto_fill_background_proc) {
      reinterpret_cast<QtWidgetSetAttributeFn>(lookup.widget_set_attribute_proc)(targets[index], 93, stage == 45);
      reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(targets[index], stage == 45);
    }
    if (apply_clicked_palette_to_widget(lookup, targets[index], stage == 45)) {
      applied_count += 1;
    }
    if (lookup.widget_update_proc && lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(targets[index]);
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(targets[index]);
    }
  }

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  for (HWND tray_root : tray_roots) {
    for (HWND current = tray_root; current; current = GetParent(current)) {
      RedrawWindow(current, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
  }

  set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + " OK: auto palette applied=" + std::to_string(applied_count) + " separator=" + std::to_string(separator_applied) + " header=" + std::to_string(header_applied) + " VCB=" + std::to_string(vcb_edit_count) + " targets=" + std::to_string(targets.size()));
  return applied_count > 0;
}

bool call_qt_stylesheet(bool enabled) {
  const int palette_stage = enabled ? 45 : 46;
  const int windows_stage = enabled ? 47 : 48;
  const QtLookup tags_header_lookup = resolve_qt_lookup();
  size_t tag_header_color_count = 0;
  size_t entity_info_input_surface_count = 0;
  if (!enabled) {
    InterlockedExchange(&g_model_services_cef_observer_only, 1);
    tag_header_color_count = apply_tag_dialog_header_colors(tags_header_lookup, false, 66);
  }
  const bool tag_header_paint_hook_ok = install_tag_header_paint_hook(enabled);
  const bool component_text_hook_ok = install_content_browser_text_hooks(enabled);
  const bool outliner_text_hook_ok = install_component_navigator_paint_hook(enabled);
  const bool vcb_ok = set_vcb_typed_input_background(!enabled);

  const bool palette_ok = probe_qt_stage(palette_stage);
  const bool vcb_paint_hook_ok = install_vcb_status_paint_hooks(resolve_qt_lookup(), enabled);
  flush_qt_pending_events(resolve_qt_lookup());
  const bool windows_ok = probe_qt_stage(windows_stage);
  if (enabled && palette_ok && windows_ok) {
    tag_header_color_count = apply_tag_dialog_header_colors(tags_header_lookup, true, 64);
  }
  if (palette_ok && windows_ok) {
    entity_info_input_surface_count = apply_entity_info_input_surface(tags_header_lookup, enabled);
  }
  const bool model_services_hook_ok = InterlockedCompareExchange(&g_model_services_cef_hook_installed, 0, 0) != 0;
  const size_t measurements_entry_pad_count = install_measurements_entry_pad_subclasses(enabled);
  const bool applied = palette_ok && windows_ok;
  size_t model_services_frame_count = 0;
  size_t model_services_live_view_count = 0;
  size_t overlays_native_surface_count = 0;
  size_t overlays_webview_style_count = 0;
  size_t model_services_refresh_count = 0;
  size_t about_sketchup_palette_count = 0;
  InterlockedExchange(&g_dark_mode_enabled, enabled ? 1 : 0);
  model_services_live_view_count = apply_model_services_style_to_live_webviews(tags_header_lookup, enabled);
  if (applied) {
    InterlockedExchange(&g_model_services_cef_observer_only, 0);
    model_services_frame_count = apply_model_services_style_to_tracked_frames(enabled);
    about_sketchup_palette_count = apply_about_sketchup_palette(enabled) ? 1 : 0;
  }
  set_theme_info("Dark Mode SKP helper main " + std::string(enabled ? "dark" : "clear") +
    ": palette=" + std::to_string(palette_ok ? 1 : 0) +
    " windows=" + std::to_string(windows_ok ? 1 : 0) +
    " vcb=" + std::to_string(vcb_ok ? 1 : 0) +
    " vcb-paint=" + std::to_string(vcb_paint_hook_ok ? 1 : 0) +
    " overlays-hook=" + std::to_string(model_services_hook_ok ? 1 : 0) +
    " overlays-creation-style=" + std::to_string(InterlockedCompareExchange(&g_model_services_cef_observer_only, 0, 0) == 0 ? 1 : 0) +
    " overlays-existing=0" +
    " overlays-native=" + std::to_string(overlays_native_surface_count) +
    " overlays-webview=" + std::to_string(overlays_webview_style_count) +
    " overlays-refresh=" + std::to_string(model_services_refresh_count) +
    " overlays-frames=" + std::to_string(model_services_frame_count) +
    " overlays-live-view=" + std::to_string(model_services_live_view_count) +
    " about=" + std::to_string(about_sketchup_palette_count) +
    " entity-info-inputs=" + std::to_string(entity_info_input_surface_count) +
    " tags-header=" + std::to_string(tag_header_color_count) +
    " tags-paint=" + std::to_string(tag_header_paint_hook_ok ? 1 : 0) +
    " components-text=" + std::to_string(component_text_hook_ok ? 1 : 0) +
    " outliner-text=" + std::to_string(outliner_text_hook_ok ? 1 : 0) +
    " measurements=" + std::to_string(measurements_entry_pad_count));
  return applied;
}

} // namespace

extern "C" __declspec(dllexport) int darkmode_skp_initialize() {
  InterlockedExchange(&g_model_services_cef_observer_only, 1);
  const bool model_services_observer_installed = install_model_services_cef_hook();
  InterlockedExchange(&g_model_services_cef_hook_installed, model_services_observer_installed ? 1 : 0);
  const bool about_observer_installed = install_about_sketchup_window_hook();
  return about_observer_installed ? 1 : 0;
}

extern "C" __declspec(dllexport) int darkmode_skp_set_dark_mode(int enabled) {
  return call_qt_stylesheet(enabled != 0) ? 1 : 0;
}

extern "C" __declspec(dllexport) int darkmode_skp_dark_mode_applied() {
  return application_palette_is_dark() ? 1 : 0;
}

extern "C" __declspec(dllexport) int darkmode_skp_theme_info(char* buffer, int buffer_len) {
  if (!buffer || buffer_len <= 0) {
    return 0;
  }

  const std::string info = theme_info();
  const size_t copy_len = std::min(static_cast<size_t>(buffer_len - 1), info.size());
  if (copy_len > 0) {
    std::memcpy(buffer, info.data(), copy_len);
  }
  buffer[copy_len] = '\0';
  return static_cast<int>(copy_len);
}
