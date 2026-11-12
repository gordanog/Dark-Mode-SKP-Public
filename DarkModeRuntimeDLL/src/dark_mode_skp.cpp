#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <richedit.h>
#include <intrin.h>
#include <capstone/capstone.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
HWINEVENTHOOK g_qmenu_window_hook = nullptr;
DWORD g_qmenu_window_hook_thread_id = 0;
HWINEVENTHOOK g_page_tabs_search_refresh_hook = nullptr;
DWORD g_page_tabs_search_refresh_hook_thread_id = 0;
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
std::vector<ContentBrowserTextDrawHook> g_content_browser_folder_color_hooks;
std::vector<ContentBrowserTextDrawHook> g_content_browser_folder_outline_hooks;
const unsigned char* g_content_browser_folder_color_image_base = nullptr;
struct ContentBrowserFolderColor {
  size_t return_rva;
  int native_shade;
  int dark_shade;
  size_t outline_return_rva;
};
std::vector<ContentBrowserFolderColor> g_content_browser_folder_colors;
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
std::mutex g_icon_theme_mutex;
volatile LONG g_icon_theme_enabled = 0;
volatile LONG g_icon_theme_resource_color_count = 0;
volatile LONG g_icon_theme_chevron_resource_color_count = 0;
volatile LONG g_icon_theme_default_env_color_count = 0;
volatile LONG g_icon_theme_tray_chevron_pixmap_count = 0;
volatile LONG g_icon_theme_tray_chevron_widget_count = 0;
volatile LONG g_icon_theme_tray_chevron_header_count = 0;
volatile LONG g_icon_theme_component_navigator_search_icon_count = 0;
volatile LONG g_icon_theme_page_tabs_search_icon_count = 0;
std::filesystem::path g_icon_theme_inventory_directory;
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

using QMenuExecFn = void*(__cdecl*)(void*, const void*, void*);
using QMenuPopupFn = void(__cdecl*)(void*, const void*, void*);
using QWidgetChangeEventFn = void(__fastcall*)(void*, void*);
using QIconAddFileFn = void(__fastcall*)(void*, const void*, const void*, int, int);

std::mutex g_qmenu_exec_mutex;
QMenuExecFn g_original_qmenu_exec = nullptr;
std::vector<void**> g_qmenu_exec_iat_slots;
thread_local bool g_qmenu_exec_styling = false;

std::mutex g_qmenu_popup_mutex;
QMenuPopupFn g_original_qmenu_popup = nullptr;
std::vector<void**> g_qmenu_popup_iat_slots;

std::mutex g_focus_returner_double_spin_box_change_event_mutex;
QWidgetChangeEventFn g_original_focus_returner_double_spin_box_change_event = nullptr;
void** g_focus_returner_double_spin_box_change_event_slot = nullptr;
thread_local bool g_focus_returner_double_spin_box_palette_applying = false;

std::mutex g_tray_hide_icon_mutex;
QIconAddFileFn g_original_tray_hide_icon_add_file = nullptr;
std::vector<void**> g_tray_hide_icon_iat_slots;
thread_local bool g_tray_hide_icon_patching = false;
volatile LONG g_icon_theme_tray_hide_native_patch_count = 0;
volatile LONG g_icon_theme_tray_hide_native_rebuild_count = 0;

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

  std::string out(static_cast<size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), needed, nullptr, nullptr);
  out.pop_back();
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
  background-color: #6f94ce;
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

QTabWidget::pane {
  background-color: #242424;
  border: 1px solid #3a3a3a;
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
  selection-background-color: #6f94ce;
  selection-color: #ffffff;
  border: 1px solid #3a3a3a;
}
QComboBox QAbstractItemView::item:hover,
QComboBox QAbstractItemView::item:selected {
  background-color: #6f94ce;
  color: #ffffff;
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
  FARPROC gui_style_hints_proc = nullptr;
  FARPROC style_hints_set_color_scheme_proc = nullptr;
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
  std::string gui_style_hints_module;
  std::string gui_style_hints_symbol;
  std::string style_hints_set_color_scheme_module;
  std::string style_hints_set_color_scheme_symbol;
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

QtLookup build_qt_lookup() {
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

  lookup.gui_style_hints_proc = find_export(lookup.gui_modules, {
    "?styleHints@QGuiApplication@@SAPEAVQStyleHints@@XZ"
  }, lookup.gui_style_hints_module, lookup.gui_style_hints_symbol);

  lookup.style_hints_set_color_scheme_proc = find_export(lookup.gui_modules, {
    "?setColorScheme@QStyleHints@@QEAAXW4ColorScheme@Qt@@@Z"
  }, lookup.style_hints_set_color_scheme_module, lookup.style_hints_set_color_scheme_symbol);

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

const QtLookup& resolve_qt_lookup() {
  // SketchUp owns Qt for its application lifetime. Publish one immutable lookup
  // so show/paint callbacks need neither module snapshots, string copies nor a
  // mutex after initialization. Retry if called before Qt finishes loading.
  static QtLookup cached_lookup;
  static std::atomic<bool> lookup_ready{false};
  if (lookup_ready.load(std::memory_order_acquire)) {
    return cached_lookup;
  }

  static std::mutex lookup_mutex;
  std::lock_guard<std::mutex> lock(lookup_mutex);
  if (lookup_ready.load(std::memory_order_relaxed)) {
    return cached_lookup;
  }

  QtLookup lookup = build_qt_lookup();
  if (!lookup.core_modules.empty() && !lookup.gui_modules.empty() && !lookup.widgets_modules.empty() &&
      lookup.instance_proc && lookup.from_utf8_proc && lookup.widget_find_proc &&
      lookup.object_inherits_proc && lookup.widget_set_palette_proc && lookup.qpalette_destructor_proc) {
    cached_lookup = std::move(lookup);
    lookup_ready.store(true, std::memory_order_release);
    return cached_lookup;
  }

  thread_local QtLookup incomplete_lookup;
  incomplete_lookup = std::move(lookup);
  return incomplete_lookup;
}

struct WidgetThemeApi {
  void*(__cdecl* property)(const void*, void*, const char*) = nullptr;
  bool(__cdecl* set_property)(void*, const char*, const void*) = nullptr;
  void(__cdecl* variant_destroy)(void*) = nullptr;
  void(__cdecl* variant_default)(void*) = nullptr;
  void(__cdecl* variant_bytes)(void*, const void*) = nullptr;
  void(__cdecl* bytes_ctor)(void*, const char*, long long) = nullptr;
  void*(__cdecl* to_bytes)(const void*, void*) = nullptr;
  const char*(__cdecl* type_name)(const void*) = nullptr;
  const void*(__cdecl* variant_data)(const void*) = nullptr;
  void*(__cdecl* palette_variant)(const void*, void*) = nullptr;
  long long(__cdecl* palette_key)(const void*) = nullptr;
};

const WidgetThemeApi& widget_theme_api() {
  static const auto api = [] {
    WidgetThemeApi result;
    const HMODULE core = GetModuleHandleW(L"Qt6Core.dll");
    const HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
    result.property = reinterpret_cast<decltype(result.property)>(GetProcAddress(core, "?property@QObject@@QEBA?AVQVariant@@PEBD@Z"));
    result.set_property = reinterpret_cast<decltype(result.set_property)>(GetProcAddress(core, "?setProperty@QObject@@QEAA_NPEBDAEBVQVariant@@@Z"));
    result.variant_destroy = reinterpret_cast<decltype(result.variant_destroy)>(GetProcAddress(core, "??1QVariant@@QEAA@XZ"));
    result.variant_default = reinterpret_cast<decltype(result.variant_default)>(GetProcAddress(core, "??0QVariant@@QEAA@XZ"));
    result.variant_bytes = reinterpret_cast<decltype(result.variant_bytes)>(GetProcAddress(core, "??0QVariant@@QEAA@AEBVQByteArray@@@Z"));
    result.bytes_ctor = reinterpret_cast<decltype(result.bytes_ctor)>(GetProcAddress(core, "??0QByteArray@@QEAA@PEBD_J@Z"));
    result.to_bytes = reinterpret_cast<decltype(result.to_bytes)>(GetProcAddress(core, "?toByteArray@QVariant@@QEBA?AVQByteArray@@XZ"));
    result.type_name = reinterpret_cast<decltype(result.type_name)>(GetProcAddress(core, "?typeName@QVariant@@QEBAPEBDXZ"));
    result.variant_data = reinterpret_cast<decltype(result.variant_data)>(GetProcAddress(core, "?constData@QVariant@@QEBAPEBXXZ"));
    result.palette_variant = reinterpret_cast<decltype(result.palette_variant)>(GetProcAddress(gui, "??BQPalette@@QEBA?AVQVariant@@XZ"));
    result.palette_key = reinterpret_cast<decltype(result.palette_key)>(GetProcAddress(gui, "?cacheKey@QPalette@@QEBA_JXZ"));
    return result;
  }();
  return api;
}

bool widget_theme_stamp(const QtLookup& lookup, void* widget, const char* name, bool store,
                        const void* style = nullptr, const void* parent = nullptr, size_t surface = 0) {
  const auto& api = widget_theme_api();
  if (!widget || !api.property || !api.set_property || !api.variant_destroy || !api.variant_bytes ||
      !api.bytes_ctor || !api.to_bytes || !api.palette_key || !lookup.widget_palette_proc ||
      !lookup.qbytearray_size_proc || !lookup.qbytearray_const_data_proc || !lookup.qbytearray_destructor_proc) {
    return false;
  }
  const void* palette = reinterpret_cast<const void*(__cdecl*)(const void*)>(lookup.widget_palette_proc)(widget);
  if (!palette) {
    return false;
  }
  const std::uintptr_t stamp[] = {static_cast<std::uintptr_t>(api.palette_key(palette)),
    reinterpret_cast<std::uintptr_t>(style), reinterpret_cast<std::uintptr_t>(parent), surface};
  alignas(16) unsigned char value[64] = {};
  alignas(16) unsigned char bytes[64] = {};
  bool matches = false;
  if (store) {
    api.bytes_ctor(bytes, reinterpret_cast<const char*>(stamp), sizeof(stamp));
    api.variant_bytes(value, bytes);
    api.set_property(widget, name, value);
    matches = true;
  } else {
    api.property(widget, value, name);
    api.to_bytes(value, bytes);
    const auto size = reinterpret_cast<long long(__cdecl*)(const void*)>(lookup.qbytearray_size_proc)(bytes);
    const auto data = reinterpret_cast<const char*(__cdecl*)(const void*)>(lookup.qbytearray_const_data_proc)(bytes);
    matches = size == sizeof(stamp) && data && std::memcmp(data, stamp, sizeof(stamp)) == 0;
  }
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qbytearray_destructor_proc)(bytes);
  api.variant_destroy(value);
  return matches;
}

bool apply_qt_color_scheme(const QtLookup& lookup, bool dark) {
  if (!lookup.gui_style_hints_proc || !lookup.style_hints_set_color_scheme_proc) {
    return false;
  }

  using QtGuiStyleHintsFn = void*(__cdecl*)();
  using QtStyleHintsSetColorSchemeFn = void(__cdecl*)(void*, int);
  void* style_hints = reinterpret_cast<QtGuiStyleHintsFn>(lookup.gui_style_hints_proc)();
  if (!style_hints) {
    return false;
  }

  reinterpret_cast<QtStyleHintsSetColorSchemeFn>(lookup.style_hints_set_color_scheme_proc)(
    style_hints, dark ? 2 : 0);
  return true;
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
    " QGuiApplication::styleHints=" + std::to_string(lookup.gui_style_hints_proc != nullptr) +
    " QStyleHints::setColorScheme=" + std::to_string(lookup.style_hints_set_color_scheme_proc != nullptr) +
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

const void* resolve_native_function_body(const void* function);

std::vector<void**> find_function_indirect_call_slots(void* function_proc, const std::vector<void*>& target_procs) {
  std::vector<void**> slots;
  if (!function_proc || target_procs.empty()) {
    return slots;
  }
  function_proc = const_cast<void*>(resolve_native_function_body(function_proc));
  if (!function_proc) {
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

  using QtStringReturnFn = void*(__cdecl*)(const void*, void*);
  using QtStringToUtf8Fn = void*(__cdecl*)(const void*, void*);
  using QtByteArrayConstDataFn = const char*(__cdecl*)(const void*);
  using QtByteArraySizeFn = long long(__cdecl*)(const void*);
  using QtDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char qstring_storage[64] = {};
  alignas(16) unsigned char bytearray_storage[64] = {};
  reinterpret_cast<QtStringReturnFn>(string_return_proc)(object, qstring_storage);
  reinterpret_cast<QtStringToUtf8Fn>(lookup.qstring_to_utf8_proc)(qstring_storage, bytearray_storage);

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

std::string qstring_to_utf8(const QtLookup& lookup, const void* string) {
  if (!string || !lookup.qstring_to_utf8_proc || !lookup.qbytearray_const_data_proc ||
      !lookup.qbytearray_size_proc || !lookup.qbytearray_destructor_proc) {
    return "";
  }

  using QtStringToUtf8Fn = void*(__cdecl*)(const void*, void*);
  using QtByteArrayConstDataFn = const char*(__cdecl*)(const void*);
  using QtByteArraySizeFn = long long(__cdecl*)(const void*);
  using QtDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char bytearray_storage[64] = {};
  reinterpret_cast<QtStringToUtf8Fn>(lookup.qstring_to_utf8_proc)(string, bytearray_storage);

  std::string result;
  const long long size = reinterpret_cast<QtByteArraySizeFn>(lookup.qbytearray_size_proc)(bytearray_storage);
  const char* data = reinterpret_cast<QtByteArrayConstDataFn>(lookup.qbytearray_const_data_proc)(bytearray_storage);
  if (data && size > 0 && size < 4096 && memory_readable(data, static_cast<size_t>(size))) {
    result.assign(data, data + size);
  }

  reinterpret_cast<QtDestructorFn>(lookup.qbytearray_destructor_proc)(bytearray_storage);
  return sanitize_log_text(result);
}

bool apply_qmenu_hover_stylesheet(void* menu) {
  if (!menu) {
    return false;
  }

  const QtLookup& lookup = resolve_qt_lookup();
  if (!lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc || !lookup.widget_stylesheet_proc ||
      !lookup.qstring_destructor_proc) {
    return false;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtDestructorFn = void(__cdecl*)(void*);

  static constexpr char menu_hover_rule[] =
    "QMenu::item:selected { background-color: #6f94ce; color: #ffffff; }\n";
  const std::string existing_stylesheet = qt_string_return_to_utf8(lookup, lookup.widget_stylesheet_proc, menu);
  if (existing_stylesheet.find(menu_hover_rule) != std::string::npos) {
    return true;
  }

  const std::string stylesheet = existing_stylesheet.empty()
    ? menu_hover_rule
    : existing_stylesheet + "\n" + menu_hover_rule;
  alignas(16) unsigned char qstring_storage[64] = {};
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(
      qstring_storage, stylesheet.c_str(), static_cast<long long>(stylesheet.size()));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(
      qstring_storage, stylesheet.c_str(), static_cast<int>(stylesheet.size()));
  }
  reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(menu, qstring_storage);
  reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  return true;
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

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_widget_list_releases = 0;
#endif

struct OwnedQtWidgetList {
  alignas(16) unsigned char storage[64] = {};
  using Deallocate = void(__cdecl*)(void*, std::intptr_t, std::intptr_t);
  Deallocate deallocate = nullptr;

  OwnedQtWidgetList() {
    static const auto release = reinterpret_cast<Deallocate>(GetProcAddress(GetModuleHandleW(L"Qt6Core.dll"),
      "?deallocate@QArrayData@@SAXPEAU1@_J1@Z"));
    deallocate = release;
  }
  OwnedQtWidgetList(const OwnedQtWidgetList&) = delete;
  OwnedQtWidgetList& operator=(const OwnedQtWidgetList&) = delete;
  ~OwnedQtWidgetList() {
    void* header = nullptr;
    std::memcpy(&header, storage, sizeof(header));
    if (header && deallocate && InterlockedDecrement(static_cast<volatile LONG*>(header)) == 0) {
      deallocate(header, sizeof(void*), alignof(void*));
#if defined(DARKMODE_RUNTIME_TESTING)
      ++g_test_widget_list_releases;
#endif
    }
  }
};

size_t dump_qwidget_list_return(FARPROC list_proc, std::vector<void*>& widgets, size_t log_limit) {
  if (!list_proc) {
    return 0;
  }

  using QtWidgetListReturnFn = void(__cdecl*)(void*);
  OwnedQtWidgetList list;
  if (!list.deallocate) {
    return 0;
  }
  reinterpret_cast<QtWidgetListReturnFn>(list_proc)(list.storage);
  QtPointerListView view = decode_qt_pointer_list(list.storage);

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

BOOL CALLBACK collect_owned_process_root_windows_proc(HWND hwnd, LPARAM lparam) {
  auto* hwnds = reinterpret_cast<std::vector<HWND>*>(lparam);
  if (!hwnds || !IsWindowVisible(hwnd)) {
    return TRUE;
  }

  DWORD window_pid = 0;
  GetWindowThreadProcessId(hwnd, &window_pid);
  if (window_pid == GetCurrentProcessId()) {
    add_unique_hwnd(*hwnds, hwnd);
  }
  return TRUE;
}

std::vector<HWND> process_root_windows_including_owned() {
  std::vector<HWND> hwnds;
  EnumWindows(collect_owned_process_root_windows_proc, reinterpret_cast<LPARAM>(&hwnds));
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

  std::vector<HWND> hwnds = process_windows_including_descendants();
  add_unique_hwnd(hwnds, g_about_sketchup_window);
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
      if (hwnd == g_about_sketchup_window && dwm_set_window_attribute) {
        HRESULT result = dwm_set_window_attribute(hwnd, 20, &dark_bool, sizeof(dark_bool));
        if (FAILED(result)) {
          dwm_set_window_attribute(hwnd, 19, &dark_bool, sizeof(dark_bool));
        }
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

bool qwidget_is_floating_toolbar(const QtLookup& lookup, void* widget) {
  if (!widget || !lookup.widget_is_window_proc || !qt_object_inherits(lookup, widget, "QToolBar")) {
    return false;
  }

  using QtWidgetIsWindowFn = bool(__cdecl*)(const void*);
  bool floating = false;
  __try {
    floating = reinterpret_cast<QtWidgetIsWindowFn>(lookup.widget_is_window_proc)(widget);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    floating = false;
  }
  return floating;
}

bool read_omnibar_stylesheet(const QtLookup& lookup, const void* widget, std::string& text) {
  if (!widget || !lookup.widget_stylesheet_proc || !lookup.qstring_to_utf8_proc ||
      !lookup.qbytearray_size_proc || !lookup.qbytearray_const_data_proc ||
      !lookup.qstring_destructor_proc || !lookup.qbytearray_destructor_proc) {
    return false;
  }
  alignas(16) unsigned char string[64] = {};
  alignas(16) unsigned char bytes[64] = {};
  reinterpret_cast<void*(__cdecl*)(const void*, void*)>(lookup.widget_stylesheet_proc)(widget, string);
  reinterpret_cast<void*(__cdecl*)(const void*, void*)>(lookup.qstring_to_utf8_proc)(string, bytes);
  const long long size = reinterpret_cast<long long(__cdecl*)(const void*)>(lookup.qbytearray_size_proc)(bytes);
  const char* data = reinterpret_cast<const char*(__cdecl*)(const void*)>(lookup.qbytearray_const_data_proc)(bytes);
  const bool readable = size >= 0 && size <= 1024 * 1024 &&
    (size == 0 || (data && memory_readable(data, static_cast<size_t>(size))));
  if (readable) {
    text.assign(size == 0 ? "" : data, static_cast<size_t>(size));
  }
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qbytearray_destructor_proc)(bytes);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(string);
  return readable;
}

bool apply_widget_color_rule(const QtLookup& lookup, void* widget, bool dark, const char* rule) {
  if (!widget || !rule || !*rule || !lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc ||
      !lookup.widget_stylesheet_proc || !lookup.qstring_destructor_proc) {
    return false;
  }

  std::string stylesheet;
  if (!read_omnibar_stylesheet(lookup, widget, stylesheet)) {
    return false;
  }
  const auto position = stylesheet.find(rule);
  if (dark == (position != std::string::npos)) {
    return true;
  }
  if (dark) {
    stylesheet += rule;
  } else {
    stylesheet.erase(position, std::strlen(rule));
  }
  alignas(16) unsigned char storage[64] = {};
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(
      storage, stylesheet.c_str(), static_cast<long long>(stylesheet.size()));
  } else {
    reinterpret_cast<void(__cdecl*)(void*, const char*, int)>(lookup.from_utf8_proc)(
      storage, stylesheet.c_str(), static_cast<int>(stylesheet.size()));
  }
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(widget, storage);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(storage);
  return true;
}

bool apply_omnibar_dialog_stylesheet(const QtLookup& lookup, void* widget, bool dark) {
  static constexpr char rule[] =
    "\n/* DarkModeSKP Omnibar colors */\n"
    "QFrame { background-color: #2b2b2b; color: #d8dbe2; }\n"
    "QLabel { background-color: transparent; color: #d8dbe2; }\n"
    "QLineEdit { background-color: #242424; color: #e6e8ee; selection-background-color: #4772b3; selection-color: #ffffff; }\n"
    "QTreeView { background-color: #242424; alternate-background-color: #303030; color: #e6e8ee; selection-background-color: #6f94ce; selection-color: #ffffff; }\n"
    "QTreeView::item:hover, QTreeView::item:selected { background-color: #6f94ce; color: #ffffff; }\n";
  return apply_widget_color_rule(lookup, widget, dark, rule);
}

enum class ModernWorkspaceSurface { None, Tabs, TrayTitle, TrayFrame, EntityInfo, Notification };

void prepare_modern_workspace_types();

std::string recolor_modern_workspace_stylesheet(std::string text, ModernWorkspaceSurface surface) {
  std::string selector;
  std::string property;
  size_t statement = 0;
  bool in_block = true;
  const auto lower = [](std::string value) {
    for (char& character : value) {
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
  };
  for (size_t cursor = 0; cursor < text.size();) {
    if (text.compare(cursor, 2, "/*") == 0) {
      const size_t end = text.find("*/", cursor + 2);
      if (end == std::string::npos) {
        break;
      }
      cursor = end + 2;
      continue;
    }
    if (text[cursor] == '\'' || text[cursor] == '"') {
      const char quote = text[cursor++];
      while (cursor < text.size()) {
        if (text[cursor] == '\\' && cursor + 1 < text.size()) {
          cursor += 2;
        } else if (text[cursor++] == quote) {
          break;
        }
      }
      continue;
    }
    if (text[cursor] == '{') {
      selector = lower(text.substr(statement, cursor - statement));
      in_block = true;
      property.clear();
      statement = ++cursor;
      continue;
    }
    if (text[cursor] == '}' || text[cursor] == ';') {
      if (text[cursor] == '}') {
        in_block = false;
      }
      property.clear();
      statement = ++cursor;
      continue;
    }
    if (in_block && property.empty() && text[cursor] == ':') {
      property = lower(text.substr(statement, cursor - statement));
      property.erase(std::remove_if(property.begin(), property.end(), [](unsigned char value) {
        return std::isspace(value) != 0;
      }), property.end());
      ++cursor;
      continue;
    }
    const bool foreground = property == "color";
    const bool background = property == "background" || property == "background-color";
    const bool border = property == "border" || property == "border-color" ||
      property == "border-top" || property == "border-bottom" || property == "border-left" || property == "border-right" ||
      property == "border-top-color" || property == "border-bottom-color" ||
      property == "border-left-color" || property == "border-right-color";
    if (!in_block || (!foreground && !background && !border)) {
      ++cursor;
      continue;
    }
    if (lower(text.substr(cursor, 4)) == "url(") {
      const size_t end = text.find(')', cursor + 4);
      if (end == std::string::npos) {
        break;
      }
      cursor = end + 1;
      continue;
    }
    int red = -1;
    int green = -1;
    int blue = -1;
    size_t length = 0;
    if (text[cursor] == '#') {
      size_t end = cursor + 1;
      while (end < text.size() && std::isxdigit(static_cast<unsigned char>(text[end]))) {
        ++end;
      }
      const size_t digits = end - cursor - 1;
      if ((digits == 3 || digits == 6) &&
          (end == text.size() || (!std::isalnum(static_cast<unsigned char>(text[end])) && text[end] != '_'))) {
        const unsigned long rgb = std::stoul(text.substr(cursor + 1, digits), nullptr, 16);
        red = digits == 3 ? static_cast<int>((rgb >> 8) & 15) * 17 : static_cast<int>((rgb >> 16) & 255);
        green = digits == 3 ? static_cast<int>((rgb >> 4) & 15) * 17 : static_cast<int>((rgb >> 8) & 255);
        blue = digits == 3 ? static_cast<int>(rgb & 15) * 17 : static_cast<int>(rgb & 255);
        length = digits + 1;
      }
    } else if (std::isalpha(static_cast<unsigned char>(text[cursor]))) {
      size_t end = cursor;
      while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '-')) {
        ++end;
      }
      const std::string token = lower(text.substr(cursor, end - cursor));
      if (token == "white" || token == "black") {
        red = green = blue = token == "white" ? 255 : 0;
        length = end - cursor;
      } else if (token == "rgb" && end < text.size() && text[end] == '(') {
        int consumed = 0;
        if (sscanf_s(text.c_str() + end, "( %d , %d , %d )%n", &red, &green, &blue, &consumed) == 3 &&
            consumed > 0 && red >= 0 && red <= 255 && green >= 0 && green <= 255 && blue >= 0 && blue <= 255) {
          length = end - cursor + static_cast<size_t>(consumed);
        }
      }
      if (!length) {
        cursor = end;
        continue;
      }
    }
    const bool neutral = red >= 0 && red == green && green == blue;
    const bool native_blue = (red == 14 && green == 65 && blue == 108) || (red == 0 && green == 154 && blue == 217);
    if (!length || (!neutral && !native_blue)) {
      cursor += std::max<size_t>(1, length);
      continue;
    }
    const char* replacement = nullptr;
    if (foreground) {
      replacement = surface == ModernWorkspaceSurface::Notification ?
        (selector.find(":disabled") != std::string::npos ? "#8e939b" : "#d8dbe2") : "palette(window-text)";
    } else if (neutral && red >= 128) {
      replacement = border ? "#3a3a3a" : surface == ModernWorkspaceSurface::Tabs || surface == ModernWorkspaceSurface::TrayTitle
        ? "#303030" : "#2b2b2b";
      if (background && surface == ModernWorkspaceSurface::Tabs) {
        if (selector.find(":selected") != std::string::npos || selector.find(":checked") != std::string::npos) {
          replacement = "#454545";
        } else if (selector.find(":hover") != std::string::npos) {
          replacement = "#3a3a3a";
        }
      }
    }
    if (replacement) {
      text.replace(cursor, length, replacement);
      cursor += std::strlen(replacement);
    } else {
      cursor += length;
    }
  }
  return text;
}

const void* omnibar_dialog_vtable();

bool apply_omnibar_dialog_style(const QtLookup& lookup, void* dialog, bool dark) {
  const void* expected = omnibar_dialog_vtable();
  if (!expected || !pointer_looks_like_cpp_object(dialog) ||
      *reinterpret_cast<const void* const*>(dialog) != expected) {
    return false;
  }
  return apply_omnibar_dialog_stylesheet(lookup, dialog, dark);
}

size_t apply_existing_omnibar_dialog_styles(const QtLookup& lookup, bool dark) {
  if (!lookup.app_all_widgets_proc) {
    return 0;
  }
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  size_t applied = 0;
  for (void* widget : widgets) {
    if (apply_omnibar_dialog_style(lookup, widget, dark)) {
      ++applied;
    }
  }
  return applied;
}

bool apply_floating_toolbar_dark_title_style(const QtLookup& lookup, void* widget) {
  if (!widget || !lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc ||
      !lookup.widget_stylesheet_proc || !lookup.qstring_destructor_proc) {
    return false;
  }

  static constexpr char title_rule[] =
    "QToolBar { background-color: #2b2b2b; color: #d8dbe2; border: 1px solid #303030; }\n"
    "QToolBar::handle { background-color: #303030; border: none; }\n"
    "QDockWidget, QDockWidget::title { background-color: #303030; color: #d8dbe2; }\n";
  const std::string existing_stylesheet = qt_string_return_to_utf8(lookup, lookup.widget_stylesheet_proc, widget);
  if (existing_stylesheet.find(title_rule) != std::string::npos) {
    return true;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtWidgetSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  using QtStringDestructorFn = void(__cdecl*)(void*);

  const std::string stylesheet = existing_stylesheet.empty()
    ? title_rule
    : existing_stylesheet + "\n" + title_rule;
  alignas(16) unsigned char qstring_storage[64] = {};
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(
      qstring_storage, stylesheet.c_str(), static_cast<long long>(stylesheet.size()));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(
      qstring_storage, stylesheet.c_str(), static_cast<int>(stylesheet.size()));
  }
  reinterpret_cast<QtWidgetSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(widget, qstring_storage);
  if (lookup.widget_update_proc) {
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
  }
  if (lookup.widget_repaint_proc) {
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
  }

  reinterpret_cast<QtStringDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  return true;
}

size_t apply_existing_floating_toolbar_dark_captions(const QtLookup& lookup) {
  size_t applied = 0;
  for (HWND hwnd : process_root_windows_including_owned()) {
    if (!IsWindow(hwnd) || !hwnd_is_current_process(hwnd) || GetAncestor(hwnd, GA_ROOT) != hwnd) {
      continue;
    }

    void* widget = qwidget_for_hwnd(lookup, hwnd);
    if (pointer_looks_like_cpp_object(widget) && qwidget_is_floating_toolbar(lookup, widget) &&
      apply_floating_toolbar_dark_title_style(lookup, widget)) {
      applied += 1;
    }
  }
  return applied;
}

int probe_focused_qt_widget() {
  const QtLookup& lookup = resolve_qt_lookup();
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

unsigned int qt_runtime_version() {
  static const unsigned int version = []() {
    HMODULE core = GetModuleHandleW(L"Qt6Core.dll");
    if (!core) {
      core = GetModuleHandleW(L"Qt5Core.dll");
    }
    FARPROC query = core ? GetProcAddress(core, "qVersion") : nullptr;
    if (!query && core) {
      query = GetProcAddress(core, "?qVersion@@YAPEBDXZ");
    }
    unsigned int major = 0;
    unsigned int minor = 0;
    unsigned int patch = 0;
    const char* text = query ? reinterpret_cast<const char*(__cdecl*)()>(query)() : nullptr;
    if (!text || sscanf_s(text, "%u.%u.%u", &major, &minor, &patch) != 3 ||
        major > 255 || minor > 255 || patch > 255) {
      return 0u;
    }
    return (major << 16) | (minor << 8) | patch;
  }();
  return version;
}

bool qt_palette_role_supported(int role, unsigned int version) {
  const unsigned int major = version >> 16;
  const unsigned int minor = (version >> 8) & 255;
  const int count = major == 6 && minor >= 6 ? 22 :
    major == 6 || (major == 5 && minor >= 12) ? 21 : 20;
  return role >= 0 && role < count;
}

bool set_qpalette_color_group_role_color(const QtLookup& lookup,
                                         void* palette,
                                         int color_group,
                                         const PaletteRoleColor& role_color) {
  if (!palette || !lookup.qcolor_rgb_ctor_proc || !lookup.qpalette_set_color_group_proc ||
      !qt_palette_role_supported(role_color.role, qt_runtime_version())) {
    return false;
  }

  using QtColorCtorFn = void(__cdecl*)(void*, int, int, int, int);
  using QtPaletteSetColorGroupFn = void(__cdecl*)(void*, int, int, const void*);

  alignas(16) unsigned char color_storage[64] = {};
  __try {
    reinterpret_cast<QtColorCtorFn>(lookup.qcolor_rgb_ctor_proc)(color_storage, role_color.red, role_color.green, role_color.blue, 255);
    reinterpret_cast<QtPaletteSetColorGroupFn>(lookup.qpalette_set_color_group_proc)(
      palette, color_group, role_color.role, color_storage);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }

  return true;
}

bool set_qpalette_role_color(const QtLookup& lookup, void* palette, const PaletteRoleColor& role_color) {
  if (!palette || !lookup.qcolor_rgb_ctor_proc || (!lookup.qpalette_set_color_group_proc && !lookup.qpalette_set_color_role_proc) ||
      !qt_palette_role_supported(role_color.role, qt_runtime_version())) {
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

  const PaletteRoleColor disabled_text_colors[] = {
    {0, 102, 102, 102},
    {6, 102, 102, 102},
    {8, 102, 102, 102},
    {20, 102, 102, 102}
  };
  for (const PaletteRoleColor& color : disabled_text_colors) {
    set_qpalette_color_group_role_color(lookup, palette, 1, color);
  }
}

bool apply_non_customizable_toolbar_hover_palette_to_widget(const QtLookup& lookup, void* widget, bool dark) {
  using QtWidgetPaletteFn = const void*(__cdecl*)(const void*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteReturnFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  bool palette_constructed = false;
  bool applied = false;
  __try {
    if (!dark && lookup.app_palette_widget_proc) {
      reinterpret_cast<QtPaletteReturnFn>(lookup.app_palette_widget_proc)(palette_storage, widget);
    } else {
      const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
      if (!source_palette) {
        return false;
      }
      reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
    }
    palette_constructed = true;
    if (dark) {
      set_qpalette_role_color(lookup, palette_storage, {12, 111, 148, 206});
      set_qpalette_role_color(lookup, palette_storage, {21, 111, 148, 206});
    }
    reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(widget, palette_storage);
    applied = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    applied = false;
  }
  if (palette_constructed) {
    reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  }
  return applied;
}

size_t apply_non_customizable_toolbar_hover_palette(const QtLookup& lookup, bool dark) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_palette_proc || !lookup.widget_set_palette_proc ||
      !lookup.qpalette_copy_ctor_proc || !lookup.qpalette_destructor_proc) {
    return 0;
  }

  size_t applied_count = 0;
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QToolButton")) {
      continue;
    }
    if (apply_non_customizable_toolbar_hover_palette_to_widget(lookup, widget, dark)) {
      applied_count += 1;
    }
  }
  return applied_count;
}

bool apply_clicked_palette_to_widget(const QtLookup& lookup, void* widget, bool dark);
bool apply_tab_bar_palette_to_widget(const QtLookup& lookup, void* widget, bool dark);

size_t apply_tab_bar_palette(const QtLookup& lookup, bool dark) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_palette_proc || !lookup.widget_set_palette_proc ||
      !lookup.qpalette_copy_ctor_proc || !lookup.qpalette_destructor_proc) {
    return 0;
  }

  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  size_t applied_count = 0;
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QTabBar")) {
      continue;
    }
    if (!apply_tab_bar_palette_to_widget(lookup, widget, dark)) {
      continue;
    }
    if (lookup.widget_update_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    }
    if (lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    }
    applied_count += 1;
  }
  return applied_count;
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

bool apply_tab_bar_palette_to_widget(const QtLookup& lookup, void* widget, bool dark) {
  if (!widget || !lookup.widget_palette_proc || !lookup.widget_set_palette_proc ||
      !lookup.qpalette_copy_ctor_proc || !lookup.qpalette_destructor_proc ||
      !lookup.qcolor_rgb_ctor_proc ||
      (!lookup.qpalette_set_color_group_proc && !lookup.qpalette_set_color_role_proc)) {
    return false;
  }

  if (!apply_clicked_palette_to_widget(lookup, widget, dark) || !dark) {
    return !dark;
  }

  using QtWidgetPaletteFn = const void*(__cdecl*)(const void*);
  using QtWidgetSetPaletteFn = void(__cdecl*)(void*, const void*);
  using QtPaletteCopyCtorFn = void(__cdecl*)(void*, const void*);
  using QtPaletteDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char palette_storage[256] = {};
  bool palette_constructed = false;
  bool applied = false;
  __try {
    const void* source_palette = reinterpret_cast<QtWidgetPaletteFn>(lookup.widget_palette_proc)(widget);
    if (!source_palette) {
      return false;
    }
    reinterpret_cast<QtPaletteCopyCtorFn>(lookup.qpalette_copy_ctor_proc)(palette_storage, source_palette);
    palette_constructed = true;
    if (set_qpalette_role_color(lookup, palette_storage, {9, 58, 58, 58})) {
      reinterpret_cast<QtWidgetSetPaletteFn>(lookup.widget_set_palette_proc)(widget, palette_storage);
      applied = true;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    applied = false;
  }

  if (palette_constructed) {
    reinterpret_cast<QtPaletteDestructorFn>(lookup.qpalette_destructor_proc)(palette_storage);
  }
  return applied;
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

  const QtLookup& lookup = resolve_qt_lookup();
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

size_t apply_scene_tab_search_dialog_palette(void* dialog);
const void* notification_vtable();
size_t apply_notification_style(const QtLookup& lookup, void* widget, bool dark,
                                const void* expected = notification_vtable());

bool install_non_customizable_toolbar_title_hook(const QtLookup& lookup, void* widget);
bool is_sketchup_toolbar(const QtLookup& lookup, const void* widget);
bool install_style_control_hook(const QtLookup& lookup, const void* widget);
bool install_style_control_slot(void** slot);
void remove_unused_style_control_hooks();

void CALLBACK qmenu_window_event(
  HWINEVENTHOOK,
  DWORD,
  HWND hwnd,
  LONG object_id,
  LONG child_id,
  DWORD,
  DWORD
) {
  if (object_id != OBJID_WINDOW || child_id != 0 || GetCurrentThreadId() != g_qmenu_window_hook_thread_id ||
      InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) == 0 ||
      !hwnd || !IsWindow(hwnd) || GetAncestor(hwnd, GA_ROOT) != hwnd) {
    return;
  }

  const QtLookup& lookup = resolve_qt_lookup();
  void* widget = qwidget_for_hwnd(lookup, hwnd);
  if (!widget || !pointer_looks_like_cpp_object(widget)) {
    return;
  }

  if (is_sketchup_toolbar(lookup, widget)) {
    if (install_non_customizable_toolbar_title_hook(lookup, widget) && lookup.widget_update_proc) {
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.widget_update_proc)(widget);
    }
    return;
  }

  if (qwidget_is_floating_toolbar(lookup, widget)) {
    static_cast<void>(apply_floating_toolbar_dark_title_style(lookup, widget));
    return;
  }

  if (apply_omnibar_dialog_style(lookup, widget, true)) {
    return;
  }

  if (apply_notification_style(lookup, widget, true) > 0) {
    return;
  }

  if (apply_scene_tab_search_dialog_palette(widget) > 0) {
    return;
  }

  if (!qt_object_inherits(lookup, widget, "QMenu") ||
      !apply_non_customizable_toolbar_hover_palette_to_widget(lookup, widget, true)) {
    return;
  }

  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  if (lookup.widget_update_proc) {
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
  }
  if (lookup.widget_repaint_proc) {
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
  }
}

bool install_qmenu_window_hook() {
  if (g_qmenu_window_hook) {
    return true;
  }

  g_qmenu_window_hook_thread_id = GetCurrentThreadId();
  g_qmenu_window_hook = SetWinEventHook(
    EVENT_OBJECT_SHOW,
    EVENT_OBJECT_SHOW,
    nullptr,
    qmenu_window_event,
    GetCurrentProcessId(),
    0,
    WINEVENT_OUTOFCONTEXT
  );
  return g_qmenu_window_hook != nullptr;
}

bool icon_theme_enabled();
bool refresh_page_tabs_search_icon(const QtLookup& lookup, void* shown_widget = nullptr);

void CALLBACK page_tabs_search_refresh_window_event(
  HWINEVENTHOOK,
  DWORD,
  HWND hwnd,
  LONG object_id,
  LONG child_id,
  DWORD,
  DWORD
) {
  if (object_id != OBJID_WINDOW || child_id != 0 || !hwnd || !IsWindow(hwnd) ||
      GetCurrentThreadId() != g_page_tabs_search_refresh_hook_thread_id ||
      (!icon_theme_enabled() && InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) == 0) ||
      InterlockedCompareExchange(&g_icon_theme_page_tabs_search_icon_count, 0, 0) != 0) {
    return;
  }

  const QtLookup& lookup = resolve_qt_lookup();
  void* widget = qwidget_for_hwnd(lookup, hwnd);
  if (!widget || InterlockedCompareExchange(&g_icon_theme_page_tabs_search_icon_count, -1, 0) != 0) {
    return;
  }

  // Limit late icon repair to the window that actually appeared. A missing or
  // version-specific icon must not cause an allWidgets scan on every event.
  const bool refreshed = refresh_page_tabs_search_icon(lookup, widget);
  InterlockedExchange(&g_icon_theme_page_tabs_search_icon_count, refreshed ? 1 : 0);
}

bool install_page_tabs_search_refresh_hook() {
  if (g_page_tabs_search_refresh_hook) {
    return true;
  }

  g_page_tabs_search_refresh_hook_thread_id = GetCurrentThreadId();
  g_page_tabs_search_refresh_hook = SetWinEventHook(
    EVENT_OBJECT_SHOW,
    EVENT_OBJECT_SHOW,
    nullptr,
    page_tabs_search_refresh_window_event,
    GetCurrentProcessId(),
    0,
    WINEVENT_OUTOFCONTEXT
  );
  return g_page_tabs_search_refresh_hook != nullptr;
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
  const QtLookup& lookup = resolve_qt_lookup();
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
  const QtLookup& lookup = resolve_qt_lookup();
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
  while (name.size() < 256) {
    const char* current = type_name + name.size();
    MEMORY_BASIC_INFORMATION region = {};
    if (VirtualQuery(current, &region, sizeof(region)) != sizeof(region) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
      return {};
    }
    const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    const auto address = reinterpret_cast<std::uintptr_t>(current);
    if (end <= address) {
      return {};
    }
    const size_t length = std::min<size_t>(256 - name.size(), end - address);
    const char* terminator = static_cast<const char*>(std::memchr(current, 0, length));
    name.append(current, terminator ? static_cast<size_t>(terminator - current) : length);
    if (terminator) {
      return name;
    }
  }
  return {};
}

struct ImageReadSection {
  const unsigned char* begin = nullptr;
  size_t size = 0;
};

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_rtti_discovery_count = 0;
#endif

std::vector<const void*> find_vtables_for_type(const char* type_name, HMODULE executable = GetModuleHandleW(nullptr)) {
#if defined(DARKMODE_RUNTIME_TESTING)
  ++g_test_rtti_discovery_count;
#endif
  std::vector<const void*> vtables;
  if (!type_name || type_name[0] == '\0') {
    return vtables;
  }

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
  const size_t section_offset = static_cast<size_t>(dos_header->e_lfanew) +
    offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt_headers->FileHeader.SizeOfOptionalHeader;
  const size_t section_table_size = static_cast<size_t>(nt_headers->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
  if (section_offset >= image_size || section_table_size > image_size - section_offset) {
    return vtables;
  }
  const auto* section = reinterpret_cast<const IMAGE_SECTION_HEADER*>(image_base + section_offset);
  if (!memory_readable(section, section_table_size)) {
    return vtables;
  }

  std::vector<ImageReadSection> read_sections;
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

  const size_t type_name_length = std::strlen(type_name);
  const unsigned char* type_descriptor = nullptr;
  for (const ImageReadSection& read_section : read_sections) {
    const auto* section_end = read_section.begin + read_section.size;
    const auto* type_name_address = std::search(read_section.begin + 16, section_end,
      type_name, type_name + type_name_length + 1);
    if (type_name_address != section_end) {
      type_descriptor = type_name_address - 16;
      break;
    }
  }
  if (!type_descriptor) {
    return vtables;
  }
  const std::uint32_t type_descriptor_rva = static_cast<std::uint32_t>(type_descriptor - image_base);

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

const void* primary_rtti_vtable(const std::vector<const void*>& vtables) {
  const void* result = nullptr;
  for (const void* vtable : vtables) {
    const auto* slots = static_cast<const void* const*>(vtable);
    if (!memory_readable(slots - 1, sizeof(void*))) {
      continue;
    }
    const auto* locator = static_cast<const unsigned char*>(slots[-1]);
    if (memory_readable(locator, 24) && read_unaligned_u32(locator) == 1 &&
        read_unaligned_u32(locator + 4) == 0) {
      if (result && result != vtable) {
        return nullptr;
      }
      result = vtable;
    }
  }
  return result;
}

const void* omnibar_dialog_vtable() {
  static const void* vtable = primary_rtti_vtable(find_vtables_for_type(".?AVOmnibarDialog@@"));
  return vtable;
}

const void* scene_tab_search_dialog_vtable() {
  static const void* vtable = primary_rtti_vtable(find_vtables_for_type(".?AVSearchSceneTabDlg@@"));
  return vtable;
}

const void* notification_vtable() {
  static const void* vtable = primary_rtti_vtable(find_vtables_for_type(".?AVCToastNotification@@"));
  return vtable;
}

const std::vector<const void*>& page_tabs_vtables() {
  static const auto vtables = find_vtables_for_type(".?AVCPageTabs@@");
  return vtables;
}

void prepare_dialog_show_types() {
  static_cast<void>(omnibar_dialog_vtable());
  static_cast<void>(scene_tab_search_dialog_vtable());
  static_cast<void>(notification_vtable());
  static_cast<void>(page_tabs_vtables());
  prepare_modern_workspace_types();
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

std::vector<const void*> find_panel_header_vtables() {
  return find_vtables_for_type(".?AVCPanelHeader@@");
}

using ToolbarTitleDrawFn = void(__cdecl*)(const void*, int, const void*, void*, const void*);

struct ToolbarTitlePaintApi {
  FARPROC fill_rect = nullptr;
  FARPROC clip_rect = nullptr;
  FARPROC draw_text = nullptr;
  FARPROC region_ctor = nullptr;
  FARPROC region_destructor = nullptr;
  FARPROC region_subtract = nullptr;
  FARPROC clip_region = nullptr;
  FARPROC dock_option_ctor = nullptr;
  FARPROC dock_option_destructor = nullptr;
  FARPROC option_init = nullptr;
  FARPROC string_assign = nullptr;
  FARPROC painter_font = nullptr;
  FARPROC painter_set_font = nullptr;
  FARPROC font_copy = nullptr;
  FARPROC font_destructor = nullptr;
  FARPROC font_weight = nullptr;
  FARPROC painter_font_metrics = nullptr;
  FARPROC font_metrics_destructor = nullptr;
  FARPROC elided_text = nullptr;
  FARPROC widget_geometry = nullptr;
  FARPROC painter_device = nullptr;
  FARPROC device_ratio = nullptr;
  FARPROC image_ctor = nullptr;
  FARPROC image_destructor = nullptr;
  FARPROC image_ratio = nullptr;
  FARPROC image_fill = nullptr;
  FARPROC image_is_null = nullptr;
  FARPROC painter_ctor = nullptr;
  FARPROC painter_destructor = nullptr;
  FARPROC painter_translate = nullptr;
  FARPROC painter_composition = nullptr;
  FARPROC painter_draw_image = nullptr;
  FARPROC panel_option_copy = nullptr;
  FARPROC panel_option_destructor = nullptr;
  FARPROC shade_panel = nullptr;
  FARPROC image_pixel = nullptr;
  FARPROC image_set_pixel = nullptr;
};

struct Qt6TitleBarOptionView {
  int version;
  int type;
  unsigned int state;
  int direction;
  int rect[4];
  void* font_metrics;
  unsigned char palette[16];
  void* style_object;
  unsigned int sub_controls;
  unsigned int active_sub_controls;
  unsigned char text[24];
  void* icon;
  int title_bar_state;
  unsigned int title_bar_flags;
};

static_assert(offsetof(Qt6TitleBarOptionView, sub_controls) == 64);
static_assert(offsetof(Qt6TitleBarOptionView, text) == 72);
static_assert(sizeof(Qt6TitleBarOptionView) == 112);

struct Qt6DockTitleOptionView {
  int version;
  int type;
  unsigned int state;
  int direction;
  int rect[4];
  void* font_metrics;
  unsigned char palette[16];
  void* style_object;
  unsigned char text[24];
  bool closable;
  bool movable;
  bool floatable;
  bool vertical_title;
};

static_assert(offsetof(Qt6DockTitleOptionView, text) == 64);
static_assert(sizeof(Qt6DockTitleOptionView) == 96);

ToolbarTitlePaintApi g_toolbar_title_api;
FARPROC g_dock_title_option_copy = nullptr;
std::vector<VcbStatusPaintHook> g_toolbar_title_hooks;
struct ToolbarTitleCloseButton {
  const void* toolbar;
  const void* button;
  int title_rect[4];
};
std::vector<ToolbarTitleCloseButton> g_toolbar_title_close_buttons;
bool g_toolbar_title_dark = false;
volatile LONG g_toolbar_title_draw_count = 0;
volatile LONG g_toolbar_close_draw_count = 0;

bool resolve_toolbar_title_paint_api(const QtLookup& lookup) {
  if (!GetModuleHandleW(L"Qt6Widgets.dll") || !lookup.widget_is_window_proc ||
      !lookup.qpainter_save_proc || !lookup.qpainter_restore_proc ||
      !lookup.qpainter_set_pen_color_proc || !lookup.qcolor_rgb_ctor_proc) {
    return false;
  }
  if (!g_toolbar_title_api.fill_rect) {
    std::string module;
    std::string symbol;
    g_toolbar_title_api.fill_rect = find_export(lookup.gui_modules, {
      "?fillRect@QPainter@@QEAAXAEBVQRect@@AEBVQColor@@@Z"
    }, module, symbol);
    g_toolbar_title_api.clip_rect = find_export(lookup.gui_modules, {
      "?setClipRect@QPainter@@QEAAXAEBVQRect@@W4ClipOperation@Qt@@@Z"
    }, module, symbol);
    g_toolbar_title_api.draw_text = find_export(lookup.gui_modules, {
      "?drawText@QPainter@@QEAAXHHHHHAEBVQString@@PEAVQRect@@@Z"
    }, module, symbol);
    g_toolbar_title_api.region_ctor = find_export(lookup.gui_modules, {
      "??0QRegion@@QEAA@AEBVQRect@@W4RegionType@0@@Z"
    }, module, symbol);
    g_toolbar_title_api.region_destructor = find_export(lookup.gui_modules, {
      "??1QRegion@@QEAA@XZ"
    }, module, symbol);
    g_toolbar_title_api.region_subtract = find_export(lookup.gui_modules, {
      "??ZQRegion@@QEAAAEAV0@AEBV0@@Z"
    }, module, symbol);
    g_toolbar_title_api.clip_region = find_export(lookup.gui_modules, {
      "?setClipRegion@QPainter@@QEAAXAEBVQRegion@@W4ClipOperation@Qt@@@Z"
    }, module, symbol);
    HMODULE widgets = GetModuleHandleW(L"Qt6Widgets.dll");
    HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
    g_toolbar_title_api.dock_option_ctor = GetProcAddress(widgets, "??0QStyleOptionDockWidget@@QEAA@XZ");
    g_toolbar_title_api.dock_option_destructor = GetProcAddress(widgets, "??1QStyleOptionDockWidget@@QEAA@XZ");
    g_toolbar_title_api.option_init = GetProcAddress(widgets, "?initFrom@QStyleOption@@QEAAXPEBVQWidget@@@Z");
    g_toolbar_title_api.widget_geometry = GetProcAddress(widgets, "?geometry@QWidget@@QEBAAEBVQRect@@XZ");
    g_toolbar_title_api.string_assign = GetProcAddress(GetModuleHandleW(L"Qt6Core.dll"), "??4QString@@QEAAAEAV0@AEBV0@@Z");
    g_toolbar_title_api.painter_font = GetProcAddress(gui, "?font@QPainter@@QEBAAEBVQFont@@XZ");
    g_toolbar_title_api.painter_set_font = GetProcAddress(gui, "?setFont@QPainter@@QEAAXAEBVQFont@@@Z");
    g_toolbar_title_api.font_copy = GetProcAddress(gui, "??0QFont@@QEAA@AEBV0@@Z");
    g_toolbar_title_api.font_destructor = GetProcAddress(gui, "??1QFont@@QEAA@XZ");
    g_toolbar_title_api.font_weight = GetProcAddress(gui, "?setWeight@QFont@@QEAAXW4Weight@1@@Z");
    g_toolbar_title_api.painter_font_metrics = GetProcAddress(gui, "?fontMetrics@QPainter@@QEBA?AVQFontMetrics@@XZ");
    g_toolbar_title_api.font_metrics_destructor = GetProcAddress(gui, "??1QFontMetrics@@QEAA@XZ");
    g_toolbar_title_api.elided_text = GetProcAddress(gui, "?elidedText@QFontMetrics@@QEBA?AVQString@@AEBV2@W4TextElideMode@Qt@@HH@Z");
    g_toolbar_title_api.painter_device = GetProcAddress(gui, "?device@QPainter@@QEBAPEAVQPaintDevice@@XZ");
    g_toolbar_title_api.device_ratio = GetProcAddress(gui, "?devicePixelRatioF@QPaintDevice@@QEBANXZ");
    g_toolbar_title_api.image_ctor = GetProcAddress(gui, "??0QImage@@QEAA@HHW4Format@0@@Z");
    g_toolbar_title_api.image_destructor = GetProcAddress(gui, "??1QImage@@UEAA@XZ");
    g_toolbar_title_api.image_ratio = GetProcAddress(gui, "?setDevicePixelRatio@QImage@@QEAAXN@Z");
    g_toolbar_title_api.image_fill = GetProcAddress(gui, "?fill@QImage@@QEAAXI@Z");
    g_toolbar_title_api.image_is_null = GetProcAddress(gui, "?isNull@QImage@@QEBA_NXZ");
    g_toolbar_title_api.painter_ctor = GetProcAddress(gui, "??0QPainter@@QEAA@PEAVQPaintDevice@@@Z");
    g_toolbar_title_api.painter_destructor = GetProcAddress(gui, "??1QPainter@@QEAA@XZ");
    g_toolbar_title_api.painter_translate = GetProcAddress(gui, "?translate@QPainter@@QEAAXNN@Z");
    g_toolbar_title_api.painter_composition = GetProcAddress(gui, "?setCompositionMode@QPainter@@QEAAXW4CompositionMode@1@@Z");
    g_toolbar_title_api.painter_draw_image = GetProcAddress(gui, "?drawImage@QPainter@@QEAAXAEBVQPointF@@AEBVQImage@@@Z");
    g_toolbar_title_api.panel_option_copy = GetProcAddress(widgets, "??0QStyleOptionComplex@@QEAA@AEBV0@@Z");
    g_toolbar_title_api.panel_option_destructor = GetProcAddress(widgets, "??1QStyleOptionComplex@@QEAA@XZ");
    g_toolbar_title_api.shade_panel = GetProcAddress(widgets, "?qDrawShadePanel@@YAXPEAVQPainter@@AEBVQRect@@AEBVQPalette@@_NHPEBVQBrush@@@Z");
    g_toolbar_title_api.image_pixel = GetProcAddress(gui, "?pixel@QImage@@QEBAIHH@Z");
    g_toolbar_title_api.image_set_pixel = GetProcAddress(gui, "?setPixel@QImage@@QEAAXHHI@Z");
  }
  return g_toolbar_title_api.fill_rect && g_toolbar_title_api.clip_rect && g_toolbar_title_api.draw_text &&
    g_toolbar_title_api.region_ctor && g_toolbar_title_api.region_destructor &&
    g_toolbar_title_api.region_subtract && g_toolbar_title_api.clip_region &&
    g_toolbar_title_api.dock_option_ctor && g_toolbar_title_api.dock_option_destructor &&
    g_toolbar_title_api.option_init && g_toolbar_title_api.string_assign &&
    g_toolbar_title_api.painter_font && g_toolbar_title_api.painter_set_font &&
    g_toolbar_title_api.font_copy && g_toolbar_title_api.font_destructor && g_toolbar_title_api.font_weight &&
    g_toolbar_title_api.painter_font_metrics && g_toolbar_title_api.font_metrics_destructor &&
    g_toolbar_title_api.elided_text && g_toolbar_title_api.widget_geometry && lookup.qpalette_set_color_group_proc &&
    g_toolbar_title_api.painter_device && g_toolbar_title_api.device_ratio && g_toolbar_title_api.image_ctor &&
    g_toolbar_title_api.image_destructor && g_toolbar_title_api.image_ratio && g_toolbar_title_api.image_fill &&
    g_toolbar_title_api.image_is_null && g_toolbar_title_api.painter_ctor && g_toolbar_title_api.painter_destructor &&
    g_toolbar_title_api.painter_translate && g_toolbar_title_api.painter_composition && g_toolbar_title_api.painter_draw_image &&
    g_toolbar_title_api.panel_option_copy && g_toolbar_title_api.panel_option_destructor && g_toolbar_title_api.shade_panel &&
    g_toolbar_title_api.image_pixel && g_toolbar_title_api.image_set_pixel;
}

bool draw_non_customizable_toolbar_close(const QtLookup& lookup, const void* style,
                                         const void* option, void* painter, const void* widget) {
  struct OptionHeader {
    int version;
    int type;
    unsigned int state;
    int direction;
    int rect[4];
  };
  if (!style || !painter || !memory_readable(option, 72)) {
    return false;
  }
  const auto* button = static_cast<const OptionHeader*>(option);
  const int width = button->rect[2] - button->rect[0] + 1;
  const int height = button->rect[3] - button->rect[1] + 1;
  if (button->version != 1 || button->type != 0xf0003 || width <= 0 || height <= 0 || width > 256 || height > 256) {
    return false;
  }
  void* const* vtable = *static_cast<void* const* const*>(style);
  if (!memory_readable(vtable, 27 * sizeof(void*)) || !vtable[21] || !vtable[22] || !vtable[26]) {
    return false;
  }
  const void* device = reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.painter_device)(painter);
  if (!device) {
    return false;
  }
  const double ratio = reinterpret_cast<double(__cdecl*)(const void*)>(g_toolbar_title_api.device_ratio)(device);
  if (!(ratio > 0 && ratio <= 8)) {
    return false;
  }
  alignas(16) unsigned char image[256] = {};
  alignas(16) unsigned char mask_painter[256] = {};
  alignas(16) unsigned char color[16] = {};
  reinterpret_cast<void(__cdecl*)(void*, int, int, int)>(g_toolbar_title_api.image_ctor)(
    image, static_cast<int>(width * ratio + 0.5), static_cast<int>(height * ratio + 0.5), 6);
  if (reinterpret_cast<bool(__cdecl*)(const void*)>(g_toolbar_title_api.image_is_null)(image)) {
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
    return false;
  }
  reinterpret_cast<void(__cdecl*)(void*, double)>(g_toolbar_title_api.image_ratio)(image, ratio);
  reinterpret_cast<void(__cdecl*)(void*, unsigned int)>(g_toolbar_title_api.image_fill)(image, 0);
  reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(mask_painter, image);
  reinterpret_cast<void(__cdecl*)(void*, double, double)>(g_toolbar_title_api.painter_translate)(
    mask_painter, -button->rect[0], -button->rect[1]);
  reinterpret_cast<ToolbarTitleDrawFn>(vtable[22])(style, 22, option, mask_painter, widget);
  reinterpret_cast<void(__cdecl*)(void*, int)>(g_toolbar_title_api.painter_composition)(mask_painter, 5);
  const bool enabled = (button->state & 1) != 0;
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color,
    enabled ? 216 : 112, enabled ? 219 : 112, enabled ? 226 : 112, 255);
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(mask_painter, button->rect, color);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(mask_painter);

  constexpr int background = 48;
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_save_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, button->rect, 2);
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color, background, background, background, 255);
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, button->rect, color);
  if (enabled && (button->state & 4)) {
    alignas(16) unsigned char panel_storage[256] = {};
    reinterpret_cast<void(__cdecl*)(void*, const void*)>(g_toolbar_title_api.panel_option_copy)(panel_storage, option);
    auto* panel = reinterpret_cast<Qt6TitleBarOptionView*>(panel_storage);
    panel->state &= ~4u;
    if ((button->state & 0x1000) && !(button->state & 0x2000)) {
      panel->state &= ~2u;
    }
    if (static_cast<const Qt6TitleBarOptionView*>(option)->active_sub_controls & 1) {
      panel->state |= 4;
    }
    reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, unsigned int, const void*)>(vtable[26])(
      style, panel->rect, 4, option, 1, widget);
    for (const PaletteRoleColor& role : {
      PaletteRoleColor{1, 48, 48, 48}, PaletteRoleColor{2, 74, 74, 74},
      PaletteRoleColor{3, 56, 56, 56}, PaletteRoleColor{4, 24, 24, 24},
      PaletteRoleColor{5, 32, 32, 32}, PaletteRoleColor{10, 48, 48, 48},
      PaletteRoleColor{11, 16, 16, 16}, PaletteRoleColor{12, 74, 74, 74}
    }) {
      set_qpalette_color_group_role_color(lookup, panel->palette, 5, role);
    }
    if (panel->state & 4) {
      reinterpret_cast<void(__cdecl*)(void*, const void*, const void*, bool, int, const void*)>(g_toolbar_title_api.shade_panel)(
        painter, panel->rect, panel->palette, true, 1, nullptr);
    }
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.panel_option_destructor)(panel_storage);
  }
  const double position[2] = {static_cast<double>(button->rect[0]), static_cast<double>(button->rect[1])};
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.painter_draw_image)(painter, position, image);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_restore_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  InterlockedIncrement(&g_toolbar_close_draw_count);
  return true;
}

bool draw_sketchup_toolbar_dock_title(const QtLookup& lookup, const void* style,
                                     const Qt6TitleBarOptionView* title, void* painter,
                                     const void* widget, ToolbarTitleDrawFn original_proc) {
  if (title->rect[2] < title->rect[0] || title->rect[3] < title->rect[1]) {
    return false;
  }
  alignas(16) unsigned char dock_storage[256] = {};
  alignas(16) unsigned char font_storage[64] = {};
  alignas(16) unsigned char color_storage[16] = {};
  auto* dock = reinterpret_cast<Qt6DockTitleOptionView*>(dock_storage);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.dock_option_ctor)(dock);
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(g_toolbar_title_api.option_init)(dock, widget);
  std::memcpy(dock->rect, title->rect, sizeof(dock->rect));
  dock->rect[2] -= 16;
  dock->state = title->state;
  dock->direction = title->direction;
  reinterpret_cast<void*(__cdecl*)(void*, const void*)>(g_toolbar_title_api.string_assign)(dock->text, title->text);
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color_storage, 216, 219, 226, 255);
  reinterpret_cast<void(__cdecl*)(void*, int, int, const void*)>(lookup.qpalette_set_color_group_proc)(
    dock->palette, 5, 0, color_storage);

  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_save_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, title->rect, 2);
  original_proc(style, 5, title, painter, widget);
  alignas(16) unsigned char paint_region[64] = {};
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.region_ctor)(paint_region, title->rect, 0);
  void* close_button = qwidget_child_at(lookup, const_cast<void*>(widget), title->rect[2] - 2,
    title->rect[1] + (title->rect[3] - title->rect[1]) / 2);
  if (close_button && qt_object_inherits(lookup, close_button, "QToolButton") &&
      qwidget_parent_widget(lookup, close_button) == widget) {
    if (qt_object_inherits(lookup, widget, "NonCustomizableToolbar")) {
      auto tracked = std::find_if(g_toolbar_title_close_buttons.begin(), g_toolbar_title_close_buttons.end(),
        [widget](const ToolbarTitleCloseButton& entry) { return entry.toolbar == widget; });
      if (tracked == g_toolbar_title_close_buttons.end()) {
        g_toolbar_title_close_buttons.push_back({widget, close_button, {}});
        tracked = g_toolbar_title_close_buttons.end() - 1;
      }
      tracked->button = close_button;
      std::memcpy(tracked->title_rect, title->rect, sizeof(tracked->title_rect));
      install_non_customizable_toolbar_title_hook(lookup, close_button);
    }
    const void* button_rect = reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.widget_geometry)(close_button);
    alignas(16) unsigned char button_region[64] = {};
    reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.region_ctor)(button_region, button_rect, 0);
    reinterpret_cast<void*(__cdecl*)(void*, const void*)>(g_toolbar_title_api.region_subtract)(paint_region, button_region);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.region_destructor)(button_region);
  }
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_region)(painter, paint_region, 2);
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color_storage, 48, 48, 48, 255);
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(
    painter, title->rect, color_storage);
  const void* font = reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.painter_font)(painter);
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(g_toolbar_title_api.font_copy)(font_storage, font);
  reinterpret_cast<void(__cdecl*)(void*, int)>(g_toolbar_title_api.font_weight)(font_storage, 700);
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(g_toolbar_title_api.painter_set_font)(painter, font_storage);
  if (dock->rect[2] >= dock->rect[0]) {
    void* const* vtable = *static_cast<void* const* const*>(style);
    int text_rect[4] = {};
    reinterpret_cast<void*(__cdecl*)(const void*, void*, int, const void*, const void*)>(vtable[23])(
      style, text_rect, 30, dock, widget);
    if (text_rect[2] >= text_rect[0] && text_rect[3] >= text_rect[1]) {
      alignas(16) unsigned char metrics[64] = {};
      alignas(16) unsigned char elided[32] = {};
      reinterpret_cast<void*(__cdecl*)(const void*, void*)>(g_toolbar_title_api.painter_font_metrics)(painter, metrics);
      reinterpret_cast<void*(__cdecl*)(const void*, void*, const void*, int, int, int)>(g_toolbar_title_api.elided_text)(
        metrics, elided, dock->text, 1, text_rect[2] - text_rect[0] + 1, 0);
      reinterpret_cast<void(__cdecl*)(const void*, void*, const void*, int, const void*, bool, const void*, int)>(vtable[18])(
        style, painter, text_rect, 0x81, dock->palette, (dock->state & 1) != 0, elided, 0);
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(elided);
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.font_metrics_destructor)(metrics);
    }
  }
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_restore_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.region_destructor)(paint_region);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.font_destructor)(font_storage);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.dock_option_destructor)(dock);
  InterlockedIncrement(&g_toolbar_title_draw_count);
  return true;
}

bool draw_non_customizable_toolbar_title(const QtLookup& lookup, const void* style,
                                         const void* option, void* painter, const void* widget,
                                         ToolbarTitleDrawFn original_proc) {
  constexpr unsigned int title_label = 0x100;
  if (!style || !option || !painter || !original_proc ||
      !memory_readable(option, sizeof(Qt6TitleBarOptionView))) {
    return false;
  }
  const auto* title = static_cast<const Qt6TitleBarOptionView*>(option);
  if (title->version != 1 || title->type != 0xf0005 || !(title->sub_controls & title_label)) {
    return false;
  }
  void* const* vtable = *static_cast<void* const* const*>(style);
  if (!memory_readable(vtable, 27 * sizeof(void*)) || !vtable[26]) {
    return false;
  }
  if (title->title_bar_flags == 0) {
    return draw_sketchup_toolbar_dock_title(lookup, style, title, painter, widget, original_proc);
  }
  using SubControlRectFn = void*(__cdecl*)(const void*, void*, int, const void*, int, const void*);
  int label_rect[4] = {};
  reinterpret_cast<SubControlRectFn>(vtable[26])(style, label_rect, 5, option, title_label, widget);
  if (label_rect[2] < label_rect[0] || label_rect[3] < label_rect[1]) {
    return false;
  }

  alignas(16) unsigned char region_storage[64] = {};
  alignas(16) unsigned char color_storage[16] = {};
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.region_ctor)(
    region_storage, title->rect, 0);
  for (unsigned int sub_control = 1; sub_control < title_label; sub_control <<= 1) {
    if (!(title->sub_controls & sub_control)) {
      continue;
    }
    int button_rect[4] = {};
    reinterpret_cast<SubControlRectFn>(vtable[26])(style, button_rect, 5, option, sub_control, widget);
    if (button_rect[2] >= button_rect[0] && button_rect[3] >= button_rect[1]) {
      alignas(16) unsigned char button_region[64] = {};
      reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.region_ctor)(
        button_region, button_rect, 0);
      reinterpret_cast<void*(__cdecl*)(void*, const void*)>(g_toolbar_title_api.region_subtract)(
        region_storage, button_region);
      reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.region_destructor)(button_region);
    }
  }
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_save_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, title->rect, 2);
  original_proc(style, 5, option, painter, widget);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_region)(painter, region_storage, 2);
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color_storage, 48, 48, 48, 255);
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(
    painter, title->rect, color_storage);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, label_rect, 2);
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color_storage, 216, 219, 226, 255);
  reinterpret_cast<QtPainterSetPenColorFn>(lookup.qpainter_set_pen_color_proc)(painter, color_storage);
  reinterpret_cast<void(__cdecl*)(void*, int, int, int, int, int, const void*, void*)>(g_toolbar_title_api.draw_text)(
    painter, label_rect[0] + 2, label_rect[1], label_rect[2] - label_rect[0] - 1,
    label_rect[3] - label_rect[1] + 1, 0x181, title->text, nullptr);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_restore_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.region_destructor)(region_storage);
  InterlockedIncrement(&g_toolbar_title_draw_count);
  return true;
}

bool is_sketchup_toolbar(const QtLookup& lookup, const void* widget) {
  return qt_object_inherits(lookup, widget, "NonCustomizableToolbar") ||
    qt_object_inherits(lookup, widget, "BaseCustomizableToolbar") ||
    qt_object_inherits(lookup, widget, "CustomizableToolbar") ||
    qt_object_inherits(lookup, widget, "qtapputils::ToolBar");
}

bool draw_legacy_toolbar_title(const QtLookup& lookup, const void* style, const void* option,
                              void* painter, const void* widget, ToolbarTitleDrawFn original_proc) {
  if (!style || !painter || !original_proc || !memory_readable(option, sizeof(Qt6DockTitleOptionView)) ||
      !resolve_toolbar_title_paint_api(lookup)) {
    return false;
  }
  const auto* source = static_cast<const Qt6DockTitleOptionView*>(option);
  if (source->type != 9 || source->version != 1 || source->vertical_title ||
      source->rect[2] < source->rect[0] || source->rect[3] < source->rect[1]) {
    return false;
  }
  if (!g_dock_title_option_copy) {
    g_dock_title_option_copy = GetProcAddress(GetModuleHandleW(L"Qt6Widgets.dll"),
      "??0QStyleOptionDockWidget@@QEAA@AEBV0@@Z");
  }
  if (!g_dock_title_option_copy) {
    return false;
  }
  alignas(16) unsigned char storage[256] = {};
  auto* dock = reinterpret_cast<Qt6DockTitleOptionView*>(storage);
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(g_dock_title_option_copy)(dock, source);
  apply_blender_palette_roles(lookup, dock->palette);
  alignas(16) unsigned char color[64] = {};
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color, 43, 43, 43, 255);
  int background_rect[4];
  std::memcpy(background_rect, dock->rect, sizeof(background_rect));
  void* close_button = widget ? qwidget_child_at(lookup, const_cast<void*>(widget), dock->rect[2] + 2,
    dock->rect[1] + (dock->rect[3] - dock->rect[1]) / 2) : nullptr;
  if (close_button && qt_object_inherits(lookup, close_button, "QToolButton") &&
      qwidget_parent_widget(lookup, close_button) == widget && is_sketchup_toolbar(lookup, widget)) {
    const auto* geometry = reinterpret_cast<const int*(__cdecl*)(const void*)>(g_toolbar_title_api.widget_geometry)(close_button);
    const int width = geometry[2] - geometry[0] + 1;
    if (width > 0 && width <= 128) {
      background_rect[2] += width;
      auto tracked = std::find_if(g_toolbar_title_close_buttons.begin(), g_toolbar_title_close_buttons.end(),
        [widget](const ToolbarTitleCloseButton& entry) { return entry.toolbar == widget; });
      if (tracked == g_toolbar_title_close_buttons.end()) {
        g_toolbar_title_close_buttons.push_back({widget, close_button, {}});
        tracked = g_toolbar_title_close_buttons.end() - 1;
      }
      tracked->button = close_button;
      std::memcpy(tracked->title_rect, background_rect, sizeof(background_rect));
      install_non_customizable_toolbar_title_hook(lookup, close_button);
    }
  }
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_save_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, background_rect, 2);
  reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, background_rect, color);
  reinterpret_cast<void(__cdecl*)(void*, const void*, int)>(g_toolbar_title_api.clip_rect)(painter, dock->rect, 2);
  original_proc(style, 30, dock, painter, widget);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_restore_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.dock_option_destructor)(dock);
  InterlockedIncrement(&g_toolbar_title_draw_count);
  return true;
}

void __cdecl non_customizable_toolbar_title_hook(const void* style, int control, const void* option,
                                                void* painter, const void* widget) {
  void** slot = &(*reinterpret_cast<void***>(const_cast<void*>(style)))[24];
  for (const auto& hook : g_toolbar_title_hooks) {
    if (hook.slot != slot) {
      continue;
    }
    const auto original_proc = reinterpret_cast<ToolbarTitleDrawFn>(hook.original_proc);
    if (g_toolbar_title_dark && control == 4 && widget) {
      for (const auto& entry : g_toolbar_title_close_buttons) {
        if (entry.button != widget) {
          continue;
        }
        const QtLookup& lookup = resolve_qt_lookup();
        void* toolbar = qwidget_parent_widget(lookup, const_cast<void*>(widget));
        if (toolbar == entry.toolbar && is_sketchup_toolbar(lookup, toolbar) &&
            reinterpret_cast<bool(__cdecl*)(const void*)>(lookup.widget_is_window_proc)(toolbar) &&
            qwidget_child_at(lookup, toolbar, entry.title_rect[2] - 2,
              entry.title_rect[1] + (entry.title_rect[3] - entry.title_rect[1]) / 2) == widget &&
            draw_non_customizable_toolbar_close(lookup, style, option, painter, widget)) {
          return;
        }
        break;
      }
    }
    if (g_toolbar_title_dark && control == 5 && widget) {
      const QtLookup& lookup = resolve_qt_lookup();
      if (qt_object_inherits(lookup, widget, "NonCustomizableToolbar") &&
          reinterpret_cast<bool(__cdecl*)(const void*)>(lookup.widget_is_window_proc)(widget) &&
          draw_non_customizable_toolbar_title(lookup, style, option, painter, widget, original_proc)) {
        return;
      }
    }
    original_proc(style, control, option, painter, widget);
    return;
  }
}

bool install_non_customizable_toolbar_title_hook(const QtLookup& lookup, void* widget) {
  if (!resolve_toolbar_title_paint_api(lookup)) {
    return false;
  }
  if (is_sketchup_toolbar(lookup, widget) && !qt_object_inherits(lookup, widget, "NonCustomizableToolbar")) {
    if (!install_style_control_hook(lookup, widget)) {
      return false;
    }
    g_toolbar_title_dark = true;
    return true;
  }
  void* style = qwidget_style(lookup, widget);
  if (!pointer_looks_like_cpp_object(style)) {
    return false;
  }
  void** vtable = *static_cast<void***>(style);
  if (!memory_readable(vtable, 27 * sizeof(void*))) {
    return false;
  }
  void** slot = &vtable[24];
  for (const auto& hook : g_toolbar_title_hooks) {
    if (hook.slot == slot) {
      g_toolbar_title_dark = true;
      return *slot == reinterpret_cast<void*>(&non_customizable_toolbar_title_hook);
    }
  }
  MEMORY_BASIC_INFORMATION memory = {};
  if (!VirtualQuery(slot, &memory, sizeof(memory)) || memory.Type != MEM_IMAGE || !*slot) {
    return false;
  }
  g_toolbar_title_hooks.push_back({slot, *slot});
  DWORD protection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) {
    g_toolbar_title_hooks.pop_back();
    return false;
  }
  *slot = reinterpret_cast<void*>(&non_customizable_toolbar_title_hook);
  DWORD unused = 0;
  VirtualProtect(slot, sizeof(*slot), protection, &unused);
  g_toolbar_title_dark = true;
  return true;
}

void remove_non_customizable_toolbar_title_hooks() {
  g_toolbar_title_dark = false;
  remove_unused_style_control_hooks();
  g_toolbar_title_close_buttons.clear();
  for (auto hook = g_toolbar_title_hooks.begin(); hook != g_toolbar_title_hooks.end();) {
    DWORD protection = 0;
    if (!VirtualProtect(hook->slot, sizeof(*hook->slot), PAGE_READWRITE, &protection)) {
      ++hook;
      continue;
    }
    if (*hook->slot == reinterpret_cast<void*>(&non_customizable_toolbar_title_hook)) {
      *hook->slot = hook->original_proc;
    }
    DWORD unused = 0;
    VirtualProtect(hook->slot, sizeof(*hook->slot), protection, &unused);
    hook = g_toolbar_title_hooks.erase(hook);
  }
}

bool apply_non_customizable_toolbar_flat_border_style_to_widget(const QtLookup& lookup, void* widget, bool dark) {
  if (!widget || !lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc ||
      !lookup.qstring_destructor_proc) {
    return false;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtWidgetSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtWidgetSetAttributeFn = void(__cdecl*)(void*, int, bool);
  using QtWidgetSetAutoFillBackgroundFn = void(__cdecl*)(void*, bool);
  using QtStringDestructorFn = void(__cdecl*)(void*);

  const char* stylesheet = dark
    ? "NonCustomizableToolbar, BaseCustomizableToolbar, CustomizableToolbar, qtapputils--ToolBar { background-color: #2b2b2b; border: 1px solid #3a3a3a; }"
    : "";
  alignas(16) unsigned char qstring_storage[256] = {};
  bool qstring_constructed = false;
  __try {
    const size_t stylesheet_length = std::strlen(stylesheet);
    if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
      reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(
        qstring_storage, stylesheet, static_cast<long long>(stylesheet_length));
    } else {
      reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(
        qstring_storage, stylesheet, static_cast<int>(stylesheet_length));
    }
    qstring_constructed = true;
    reinterpret_cast<QtWidgetSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(widget, qstring_storage);
    if (lookup.widget_set_attribute_proc) {
      reinterpret_cast<QtWidgetSetAttributeFn>(lookup.widget_set_attribute_proc)(widget, 93, dark);
    }
    if (lookup.widget_set_auto_fill_background_proc) {
      reinterpret_cast<QtWidgetSetAutoFillBackgroundFn>(lookup.widget_set_auto_fill_background_proc)(widget, false);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (qstring_constructed) {
      reinterpret_cast<QtStringDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
    }
    return false;
  }

  reinterpret_cast<QtStringDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  return true;
}

struct NonCustomizableToolbarStyleStats {
  size_t surface_style_count = 0;
  size_t floating_count = 0;
  size_t title_paint_count = 0;
};

NonCustomizableToolbarStyleStats apply_non_customizable_toolbar_flat_border_palette(const QtLookup& lookup, bool dark) {
  NonCustomizableToolbarStyleStats stats;
  if (!lookup.app_all_widgets_proc || !lookup.widget_update_proc || !lookup.widget_repaint_proc ||
      !lookup.object_inherits_proc) {
    return stats;
  }

  using QtWidgetIsWindowFn = bool(__cdecl*)(const void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) ||
      !is_sketchup_toolbar(lookup, widget)) {
      continue;
    }
    bool floating = false;
    if (lookup.widget_is_window_proc) {
      floating = reinterpret_cast<QtWidgetIsWindowFn>(lookup.widget_is_window_proc)(widget);
    }
    if (floating) {
      stats.floating_count += 1;
    }
    const bool border_style_applied = apply_non_customizable_toolbar_flat_border_style_to_widget(lookup, widget, dark);
    const bool title_paint_applied = dark && install_non_customizable_toolbar_title_hook(lookup, widget);
    if (!title_paint_applied && !border_style_applied) {
      continue;
    }
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    if (border_style_applied) {
      stats.surface_style_count += 1;
    }
    if (title_paint_applied) {
      stats.title_paint_count += 1;
    }
  }
  return stats;
}

std::vector<void**> find_main_import_iat_slots_for_target(void* target_proc, HMODULE importing_module = nullptr) {
  std::vector<void**> slots;
  if (!target_proc) {
    return slots;
  }

  auto add_slot = [&slots](void** slot) {
    if (slot && std::find(slots.begin(), slots.end(), slot) == slots.end()) {
      slots.push_back(slot);
    }
  };

  HMODULE executable = importing_module ? importing_module : GetModuleHandleW(nullptr);
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

  if (importing_module) {
    return slots;
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

struct ModernWorkspaceType {
  const void* vtable = nullptr;
  ModernWorkspaceSurface surface = ModernWorkspaceSurface::None;
  bool scene_tabs = false;
};

std::vector<ModernWorkspaceType> discover_modern_workspace_types(HMODULE image) {
  const void* entity_info = primary_rtti_vtable(find_vtables_for_type(".?AVCEntityInfoDlgV2@@", image));
  if (!entity_info) {
    return {};
  }
  std::vector<ModernWorkspaceType> types = {{entity_info, ModernWorkspaceSurface::EntityInfo}};
  const std::pair<const char*, ModernWorkspaceSurface> names[] = {
    {".?AVCPageTabs@@", ModernWorkspaceSurface::Tabs},
    {".?AVCTabBar@@", ModernWorkspaceSurface::Tabs},
    {".?AVPanelTabBar@@", ModernWorkspaceSurface::Tabs},
    {".?AVPanelTabElement@@", ModernWorkspaceSurface::Tabs},
    {".?AVPlusTabBar@@", ModernWorkspaceSurface::Tabs},
    {".?AVSketchUpTabBarWidget@@", ModernWorkspaceSurface::Tabs},
    {".?AVSketchUpTitleBarWidget@@", ModernWorkspaceSurface::TrayTitle},
    {".?AVViewDockWidget@@", ModernWorkspaceSurface::TrayFrame},
    {".?AVSketchUpStackWidget@@", ModernWorkspaceSurface::TrayFrame},
    {".?AVSketchUpFloatingWindowWidget@@", ModernWorkspaceSurface::TrayFrame},
    {".?AVEntityInfoControlsTabV2@@", ModernWorkspaceSurface::EntityInfo}
  };
  for (const auto& entry : names) {
    if (const void* vtable = primary_rtti_vtable(find_vtables_for_type(entry.first, image))) {
      types.push_back({vtable, entry.second, std::strcmp(entry.first, ".?AVCPageTabs@@") == 0});
    }
  }
  return types;
}

#if defined(DARKMODE_RUNTIME_TESTING)
const std::vector<ModernWorkspaceType>* g_test_modern_workspace_types = nullptr;
#endif

const std::vector<ModernWorkspaceType>& modern_workspace_types() {
#if defined(DARKMODE_RUNTIME_TESTING)
  if (g_test_modern_workspace_types) {
    return *g_test_modern_workspace_types;
  }
#endif
  static const auto types = discover_modern_workspace_types(GetModuleHandleW(nullptr));
  return types;
}

void prepare_modern_workspace_types() {
  static_cast<void>(modern_workspace_types());
}

ModernWorkspaceSurface modern_workspace_surface(const QtLookup& lookup, const void* widget,
                                               const std::vector<ModernWorkspaceType>& types) {
  if (types.empty()) {
    return ModernWorkspaceSurface::None;
  }
  for (int depth = 0; widget && depth < 16; ++depth) {
    if (!pointer_looks_like_cpp_object(widget)) {
      break;
    }
    if (qt_object_inherits(lookup, widget, "CPanelHeader")) {
      return ModernWorkspaceSurface::None;
    }
    const void* vtable = *static_cast<const void* const*>(widget);
    for (const auto& type : types) {
      if (type.vtable == vtable) {
        return type.surface;
      }
    }
    widget = qwidget_parent_widget(lookup, const_cast<void*>(widget));
  }
  return ModernWorkspaceSurface::None;
}

bool g_modern_workspace_dark = false;
LONG g_modern_workspace_title_draws = 0;
LONG g_modern_workspace_tab_draws = 0;
bool set_modern_workspace_frame_hook(bool enabled, HMODULE module = nullptr);
thread_local bool g_modern_workspace_style_applying = false;
std::vector<VcbStatusPaintHook> g_modern_workspace_style_hooks;

struct ModernWorkspacePaintApi {
  void(__cdecl* brush_color)(void*, const void*) = nullptr;
  void(__cdecl* brush_ctor)(void*, const void*, int) = nullptr;
  void(__cdecl* fill_rect)(void*, const void*, const void*) = nullptr;
  void(__cdecl* pen_ctor)(void*, const void*) = nullptr;
  unsigned int(__cdecl* rgba)(const void*) = nullptr;
  void(__cdecl* tab_copy)(void*, const void*) = nullptr;
  void(__cdecl* tab_destroy)(void*) = nullptr;
  void(__cdecl* dock_copy)(void*, const void*) = nullptr;
  void(__cdecl* dock_destroy)(void*) = nullptr;
  long long(__cdecl* palette_key)(const void*) = nullptr;
  unsigned long long(__cdecl* palette_mask)(const void*) = nullptr;
  int(__cdecl* palette_group)(const void*) = nullptr;
  void*(__cdecl* palette_assign)(void*, const void*) = nullptr;
  void*(__cdecl* cursor_pos)(void*) = nullptr;
  void*(__cdecl* map_from_global)(const void*, void*, const void*) = nullptr;
  void(__cdecl* option_copy)(void*, const void*) = nullptr;
  void(__cdecl* option_destroy)(void*) = nullptr;
  void(__cdecl* frame_copy)(void*, const void*) = nullptr;
  void(__cdecl* frame_destroy)(void*) = nullptr;
  void(__cdecl* tab_frame_copy)(void*, const void*) = nullptr;
  void(__cdecl* tab_frame_destroy)(void*) = nullptr;
  void(__cdecl* tab_base_copy)(void*, const void*) = nullptr;
  void(__cdecl* tab_base_destroy)(void*) = nullptr;
  ToolbarTitleDrawFn proxy_primitive = nullptr;
};

ModernWorkspacePaintApi g_modern_workspace_paint_api;
std::vector<VcbStatusPaintHook> g_modern_workspace_color_hooks;
VcbStatusPaintHook g_modern_workspace_primitive_hook;
struct ModernWorkspacePaintContext {
  void* painter = nullptr;
  int control = -1;
  unsigned int state = 0;
  int primitive = -1;
};
thread_local ModernWorkspacePaintContext g_modern_workspace_paint;

bool resolve_modern_workspace_paint_api() {
  auto& api = g_modern_workspace_paint_api;
  HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
  HMODULE widgets = GetModuleHandleW(L"Qt6Widgets.dll");
  api.brush_color = reinterpret_cast<decltype(api.brush_color)>(
    GetProcAddress(gui, "?setBrush@QPainter@@QEAAXVQColor@@@Z"));
  api.brush_ctor = reinterpret_cast<decltype(api.brush_ctor)>(
    GetProcAddress(gui, "??0QBrush@@QEAA@AEBVQColor@@W4BrushStyle@Qt@@@Z"));
  api.fill_rect = reinterpret_cast<decltype(api.fill_rect)>(
    GetProcAddress(gui, "?fillRect@QPainter@@QEAAXAEBVQRect@@AEBVQColor@@@Z"));
  api.pen_ctor = reinterpret_cast<decltype(api.pen_ctor)>(GetProcAddress(gui, "??0QPen@@QEAA@AEBVQColor@@@Z"));
  api.rgba = reinterpret_cast<decltype(api.rgba)>(GetProcAddress(gui, "?rgba@QColor@@QEBAIXZ"));
  api.tab_copy = reinterpret_cast<decltype(api.tab_copy)>(
    GetProcAddress(widgets, "??0QStyleOptionTab@@QEAA@AEBV0@@Z"));
  api.tab_destroy = reinterpret_cast<decltype(api.tab_destroy)>(
    GetProcAddress(widgets, "??1QStyleOptionTab@@QEAA@XZ"));
  api.dock_copy = reinterpret_cast<decltype(api.dock_copy)>(
    GetProcAddress(widgets, "??0QStyleOptionDockWidget@@QEAA@AEBV0@@Z"));
  api.dock_destroy = reinterpret_cast<decltype(api.dock_destroy)>(
    GetProcAddress(widgets, "??1QStyleOptionDockWidget@@QEAA@XZ"));
  api.palette_key = reinterpret_cast<decltype(api.palette_key)>(
    GetProcAddress(gui, "?cacheKey@QPalette@@QEBA_JXZ"));
  api.palette_mask = reinterpret_cast<decltype(api.palette_mask)>(
    GetProcAddress(gui, "?resolveMask@QPalette@@QEBA_KXZ"));
  api.palette_group = reinterpret_cast<decltype(api.palette_group)>(
    GetProcAddress(gui, "?currentColorGroup@QPalette@@QEBA?AW4ColorGroup@1@XZ"));
  api.palette_assign = reinterpret_cast<decltype(api.palette_assign)>(
    GetProcAddress(gui, "??4QPalette@@QEAAAEAV0@AEBV0@@Z"));
  api.cursor_pos = reinterpret_cast<decltype(api.cursor_pos)>(GetProcAddress(gui, "?pos@QCursor@@SA?AVQPoint@@XZ"));
  api.map_from_global = reinterpret_cast<decltype(api.map_from_global)>(
    GetProcAddress(widgets, "?mapFromGlobal@QWidget@@QEBA?AVQPoint@@AEBV2@@Z"));
  api.option_copy = reinterpret_cast<decltype(api.option_copy)>(GetProcAddress(widgets, "??0QStyleOption@@QEAA@AEBV0@@Z"));
  api.option_destroy = reinterpret_cast<decltype(api.option_destroy)>(GetProcAddress(widgets, "??1QStyleOption@@QEAA@XZ"));
  api.frame_copy = reinterpret_cast<decltype(api.frame_copy)>(GetProcAddress(widgets, "??0QStyleOptionFrame@@QEAA@AEBV0@@Z"));
  api.frame_destroy = reinterpret_cast<decltype(api.frame_destroy)>(GetProcAddress(widgets, "??1QStyleOptionFrame@@QEAA@XZ"));
  api.tab_frame_copy = reinterpret_cast<decltype(api.tab_frame_copy)>(GetProcAddress(widgets, "??0QStyleOptionTabWidgetFrame@@QEAA@AEBV0@@Z"));
  api.tab_frame_destroy = reinterpret_cast<decltype(api.tab_frame_destroy)>(GetProcAddress(widgets, "??1QStyleOptionTabWidgetFrame@@QEAA@XZ"));
  api.tab_base_copy = reinterpret_cast<decltype(api.tab_base_copy)>(GetProcAddress(widgets, "??0QStyleOptionTabBarBase@@QEAA@AEBV0@@Z"));
  api.tab_base_destroy = reinterpret_cast<decltype(api.tab_base_destroy)>(GetProcAddress(widgets, "??1QStyleOptionTabBarBase@@QEAA@XZ"));
  api.proxy_primitive = reinterpret_cast<ToolbarTitleDrawFn>(GetProcAddress(widgets,
    "?drawPrimitive@QProxyStyle@@UEBAXW4PrimitiveElement@QStyle@@PEBVQStyleOption@@PEAVQPainter@@PEBVQWidget@@@Z"));
  return api.brush_color && api.brush_ctor && api.fill_rect && api.pen_ctor && api.rgba && api.tab_copy && api.tab_destroy &&
    api.dock_copy && api.dock_destroy;
}

struct ModernWorkspacePaletteEntry {
  alignas(16) unsigned char palette[256] = {};
  long long key = 0;
  unsigned long long mask = 0;
  int group = 0;
  bool constructed = false;
};

constexpr size_t modern_workspace_palette_capacity = 16;
ModernWorkspacePaletteEntry g_modern_workspace_palettes[modern_workspace_palette_capacity];
size_t g_modern_workspace_palette_next = 0;

void clear_modern_workspace_palettes(const QtLookup& lookup) {
  for (auto& entry : g_modern_workspace_palettes) {
    if (entry.constructed) {
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpalette_destructor_proc)(entry.palette);
      entry.constructed = false;
    }
  }
  g_modern_workspace_palette_next = 0;
}

void apply_modern_workspace_paint_palette(const QtLookup& lookup, void* palette) {
  const auto& api = g_modern_workspace_paint_api;
  const bool cache_available = api.palette_key && api.palette_mask && api.palette_group && api.palette_assign &&
    lookup.qpalette_copy_ctor_proc && lookup.qpalette_destructor_proc;
  const long long key = cache_available ? api.palette_key(palette) : 0;
  const unsigned long long mask = cache_available ? api.palette_mask(palette) : 0;
  const int group = cache_available ? api.palette_group(palette) : 0;
  if (cache_available) {
    for (const auto& entry : g_modern_workspace_palettes) {
      if (entry.constructed && entry.key == key && entry.mask == mask && entry.group == group) {
        api.palette_assign(palette, entry.palette);
        return;
      }
    }
  }
  apply_blender_palette_roles(lookup, palette);
  for (int role : {0, 6, 8}) {
    set_qpalette_role_color(lookup, palette, {role, 216, 219, 226});
    set_qpalette_color_group_role_color(lookup, palette, 1, {role, 142, 147, 155});
  }
  if (cache_available) {
    auto& entry = g_modern_workspace_palettes[g_modern_workspace_palette_next];
    if (entry.constructed) {
      reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpalette_destructor_proc)(entry.palette);
    }
    reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.qpalette_copy_ctor_proc)(entry.palette, palette);
    entry.key = key;
    entry.mask = mask;
    entry.group = group;
    entry.constructed = true;
    g_modern_workspace_palette_next = (g_modern_workspace_palette_next + 1) % modern_workspace_palette_capacity;
  }
}

const void* modern_workspace_paint_color(const void* color, void* replacement, bool foreground) {
  if (!g_modern_workspace_paint.painter || !color || !g_modern_workspace_paint_api.rgba) {
    return color;
  }
  const unsigned int rgba = g_modern_workspace_paint_api.rgba(color);
  const unsigned int rgb = rgba & 0xffffff;
  const unsigned int shade = (rgb >> 16) & 255;
  const bool neutral = ((rgb >> 8) & 255) == shade && (rgb & 255) == shade;
  const bool native_blue = rgb == 0x0e416c || rgb == 0x009ad9 || rgb == 0x0063a3;
  const bool native_tab_background = g_modern_workspace_paint.control == 8 && rgb == 0xe0e1e9;
  if ((foreground && !neutral && !native_blue) ||
      (!foreground && !native_tab_background && (!neutral || shade < 128))) {
    return color;
  }
  int red = 48;
  int green = 48;
  int blue = 48;
  if (foreground) {
    const bool enabled = (g_modern_workspace_paint.state & 1) != 0;
    red = enabled ? 216 : 142;
    green = enabled ? 219 : 147;
    blue = enabled ? 226 : 155;
  } else if (g_modern_workspace_paint.control != 30) {
    red = green = blue = (g_modern_workspace_paint.state & 0x8000) ? 69 :
      (g_modern_workspace_paint.state & 0x2000) ? 58 : 48;
  }
  reinterpret_cast<QtColorRgbCtorFn>(resolve_qt_lookup().qcolor_rgb_ctor_proc)(
    replacement, red, green, blue, rgba >> 24);
  return replacement;
}

void __cdecl modern_workspace_brush_color_hook(void* painter, const void* color) {
  alignas(16) unsigned char replacement[64] = {};
  g_modern_workspace_paint_api.brush_color(painter,
    painter == g_modern_workspace_paint.painter && g_modern_workspace_paint.control == 8
      ? modern_workspace_paint_color(color, replacement, false) : color);
}

void __cdecl modern_workspace_brush_ctor_hook(void* brush, const void* color, int style) {
  alignas(16) unsigned char replacement[64] = {};
  g_modern_workspace_paint_api.brush_ctor(brush,
    g_modern_workspace_paint.control == 9 ? modern_workspace_paint_color(color, replacement, true) : color, style);
}

void __cdecl modern_workspace_fill_rect_hook(void* painter, const void* rect, const void* color) {
  alignas(16) unsigned char replacement[64] = {};
  const bool selected_underline = g_modern_workspace_paint.control == 8 && color &&
    (g_modern_workspace_paint_api.rgba(color) & 0xffffff) == 0x0063a3;
  g_modern_workspace_paint_api.fill_rect(painter, rect,
    painter == g_modern_workspace_paint.painter && (g_modern_workspace_paint.control == 30 || selected_underline)
      ? modern_workspace_paint_color(color, replacement, selected_underline) : color);
}

void __cdecl modern_workspace_primitive_pen_hook(void* pen, const void* color) {
  alignas(16) unsigned char replacement[64] = {};
  if (g_modern_workspace_dark && g_modern_workspace_paint.painter &&
      g_modern_workspace_paint.primitive >= 0 && color) {
    const unsigned int rgba = g_modern_workspace_paint_api.rgba(color);
    const unsigned int shade = (rgba >> 16) & 255;
    if (((rgba >> 8) & 255) == shade && (rgba & 255) == shade && shade >= 96) {
      reinterpret_cast<QtColorRgbCtorFn>(resolve_qt_lookup().qcolor_rgb_ctor_proc)(replacement, 58, 58, 58, rgba >> 24);
      color = replacement;
    }
  }
  g_modern_workspace_paint_api.pen_ctor(pen, color);
}

bool set_modern_workspace_color_hooks(bool enabled, HMODULE module = nullptr) {
  if (!enabled) {
    for (auto hook = g_modern_workspace_color_hooks.begin(); hook != g_modern_workspace_color_hooks.end();) {
      const void* current = *hook->slot;
      if (current == reinterpret_cast<void*>(&modern_workspace_brush_color_hook) ||
          current == reinterpret_cast<void*>(&modern_workspace_brush_ctor_hook) ||
          current == reinterpret_cast<void*>(&modern_workspace_fill_rect_hook) ||
          current == reinterpret_cast<void*>(&modern_workspace_primitive_pen_hook)) {
        DWORD protection = 0;
        if (!VirtualProtect(hook->slot, sizeof(void*), PAGE_READWRITE, &protection)) {
          ++hook;
          continue;
        }
        *hook->slot = hook->original_proc;
        DWORD unused = 0;
        VirtualProtect(hook->slot, sizeof(void*), protection, &unused);
      }
      hook = g_modern_workspace_color_hooks.erase(hook);
    }
    return g_modern_workspace_color_hooks.empty();
  }
  if (!g_modern_workspace_color_hooks.empty()) {
    return true;
  }
  if (!resolve_modern_workspace_paint_api()) {
    return false;
  }
  const std::pair<void*, void*> replacements[] = {
    {reinterpret_cast<void*>(g_modern_workspace_paint_api.brush_color), reinterpret_cast<void*>(&modern_workspace_brush_color_hook)},
    {reinterpret_cast<void*>(g_modern_workspace_paint_api.brush_ctor), reinterpret_cast<void*>(&modern_workspace_brush_ctor_hook)},
    {reinterpret_cast<void*>(g_modern_workspace_paint_api.fill_rect), reinterpret_cast<void*>(&modern_workspace_fill_rect_hook)},
    {reinterpret_cast<void*>(g_modern_workspace_paint_api.pen_ctor), reinterpret_cast<void*>(&modern_workspace_primitive_pen_hook)}
  };
  for (const auto& replacement : replacements) {
    const auto slots = find_main_import_iat_slots_for_target(replacement.first, module ? module : GetModuleHandleW(nullptr));
    if (slots.empty()) {
      static_cast<void>(set_modern_workspace_color_hooks(false));
      return false;
    }
    for (void** slot : slots) {
      DWORD protection = 0;
      if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
        static_cast<void>(set_modern_workspace_color_hooks(false));
        return false;
      }
      g_modern_workspace_color_hooks.push_back({slot, *slot});
      *slot = replacement.second;
      DWORD unused = 0;
      VirtualProtect(slot, sizeof(void*), protection, &unused);
    }
  }
  return true;
}

bool draw_modern_workspace_control(const QtLookup& lookup, const void* style, int control, const void* option,
                                  void* painter, const void* widget, ToolbarTitleDrawFn original) {
  if (!g_modern_workspace_dark || !widget || !painter || !original ||
      (control != 8 && control != 9 && control != 30) || !g_modern_workspace_paint_api.tab_copy ||
      !memory_readable(option, 64) ||
      (g_modern_workspace_paint.painter == painter && g_modern_workspace_paint.control == control)) {
    return false;
  }
  const auto surface = modern_workspace_surface(lookup, widget, modern_workspace_types());
  const auto* source = static_cast<const Qt6DockTitleOptionView*>(option);
  const bool title = control == 30;
  if (surface == ModernWorkspaceSurface::None ||
      (title ? surface != ModernWorkspaceSurface::TrayTitle || source->type != 9 || source->version != 1 :
        source->type != 3 || source->version != 1 || !qt_object_inherits(lookup, widget, "QTabBar"))) {
    return false;
  }
  alignas(16) unsigned char storage[512] = {};
  const auto& api = g_modern_workspace_paint_api;
  (title ? api.dock_copy : api.tab_copy)(storage, option);
  auto* themed = reinterpret_cast<Qt6DockTitleOptionView*>(storage);
  apply_modern_workspace_paint_palette(lookup, themed->palette);
  const auto previous = g_modern_workspace_paint;
  g_modern_workspace_paint = {painter, control, source->state};
  InterlockedIncrement(title ? &g_modern_workspace_title_draws : &g_modern_workspace_tab_draws);
  if (control == 8 && surface == ModernWorkspaceSurface::Tabs) {
    const void* owner = qwidget_parent_widget(lookup, const_cast<void*>(widget));
    for (int depth = 0; owner && depth < 16 && pointer_looks_like_cpp_object(owner); ++depth) {
      const void* owner_table = *static_cast<const void* const*>(owner);
      const auto& types = modern_workspace_types();
      if (std::any_of(types.begin(), types.end(), [owner_table](const ModernWorkspaceType& type) {
          return type.scene_tabs && type.vtable == owner_table;
        })) {
        alignas(16) unsigned char background[64] = {};
        reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(background, 43, 43, 43, 255);
        api.fill_rect(painter, source->rect, background);
        break;
      }
      owner = qwidget_parent_widget(lookup, const_cast<void*>(owner));
    }
  }
  original(style, control, storage, painter, widget);
  g_modern_workspace_paint = previous;
  (title ? api.dock_destroy : api.tab_destroy)(storage);
  return true;
}

void __cdecl modern_workspace_primitive_draw_hook(const void* style, int primitive, const void* option,
                                                void* painter, const void* widget) {
  const auto original = reinterpret_cast<ToolbarTitleDrawFn>(g_modern_workspace_primitive_hook.original_proc);
  if (!original) {
    return;
  }
  const auto& api = g_modern_workspace_paint_api;
  const auto* source = static_cast<const Qt6DockTitleOptionView*>(option);
    if (g_modern_workspace_dark && (primitive == 0 || primitive == 2 || primitive == 8) &&
      painter && widget && memory_readable(option, 64) &&
      !(g_modern_workspace_paint.painter == painter && g_modern_workspace_paint.primitive == primitive)) {
    const auto surface = modern_workspace_surface(resolve_qt_lookup(), widget, modern_workspace_types());
    const bool relevant = primitive != 12 &&
      (surface == ModernWorkspaceSurface::TrayFrame || surface == ModernWorkspaceSurface::TrayTitle);
    void(__cdecl* copy)(void*, const void*) = nullptr;
    void(__cdecl* destroy)(void*) = nullptr;
    if (source->type == 0 && source->version == 1) {
      copy = api.option_copy;
      destroy = api.option_destroy;
    } else if (source->type == 5 && source->version >= 1 && source->version <= 3) {
      copy = api.frame_copy;
      destroy = api.frame_destroy;
    } else if (source->type == 9 && source->version == 1) {
      copy = api.dock_copy;
      destroy = api.dock_destroy;
    } else if (source->type == 11 && source->version == 1) {
      copy = api.tab_frame_copy;
      destroy = api.tab_frame_destroy;
    } else if (source->type == 12 && source->version >= 1 && source->version <= 2) {
      copy = api.tab_base_copy;
      destroy = api.tab_base_destroy;
    }
    if (relevant && copy && destroy) {
      alignas(16) unsigned char storage[512] = {};
      copy(storage, option);
      apply_modern_workspace_paint_palette(resolve_qt_lookup(), reinterpret_cast<Qt6DockTitleOptionView*>(storage)->palette);
      const auto previous = g_modern_workspace_paint;
      g_modern_workspace_paint = {painter, -1, source->state, primitive};
      original(style, primitive, storage, painter, widget);
      g_modern_workspace_paint = previous;
      destroy(storage);
      return;
    }
  }
  if (g_modern_workspace_dark && primitive == 47 && painter && widget && memory_readable(option, 64) &&
      source->version == 1 && source->type == 0 && !(source->state & 4) &&
      api.cursor_pos && api.map_from_global && api.option_copy && api.option_destroy && api.proxy_primitive &&
      modern_workspace_surface(resolve_qt_lookup(), widget, modern_workspace_types()) == ModernWorkspaceSurface::Tabs &&
      source->rect[2] >= source->rect[0] && source->rect[3] >= source->rect[1]) {
    int global[2] = {};
    int local[2] = {};
    api.cursor_pos(global);
    api.map_from_global(widget, local, global);
    const bool inside = local[0] >= source->rect[0] && local[0] <= source->rect[2] &&
      local[1] >= source->rect[1] && local[1] <= source->rect[3];
    if (!inside) {
      alignas(16) unsigned char storage[128] = {};
      api.option_copy(storage, option);
      reinterpret_cast<Qt6DockTitleOptionView*>(storage)->state &= ~(0x2000u | 2u);
      api.proxy_primitive(style, primitive, storage, painter, widget);
      api.option_destroy(storage);
      return;
    }
  }
  original(style, primitive, option, painter, widget);
}

bool set_modern_workspace_primitive_hook(bool enabled, void** slot = nullptr) {
  auto& hook = g_modern_workspace_primitive_hook;
  if (!enabled) {
    if (hook.slot && *hook.slot == reinterpret_cast<void*>(&modern_workspace_primitive_draw_hook)) {
      DWORD protection = 0;
      if (!VirtualProtect(hook.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
        return false;
      }
      *hook.slot = hook.original_proc;
      DWORD unused = 0;
      VirtualProtect(hook.slot, sizeof(void*), protection, &unused);
    }
    hook = {};
    return true;
  }
  if (hook.slot) {
    return hook.slot == slot && *slot == reinterpret_cast<void*>(&modern_workspace_primitive_draw_hook);
  }
  DWORD protection = 0;
  if (!memory_readable(slot, sizeof(void*)) || !*slot ||
      !VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
    return false;
  }
  hook = {slot, *slot};
  *slot = reinterpret_cast<void*>(&modern_workspace_primitive_draw_hook);
  DWORD unused = 0;
  VirtualProtect(slot, sizeof(void*), protection, &unused);
  return true;
}

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_workspace_style_preparations = 0;
#endif

bool apply_modern_workspace_widget_style(const QtLookup& lookup, void* widget, bool dark,
                                        ModernWorkspaceSurface explicit_surface = ModernWorkspaceSurface::None) {
  const auto surface = explicit_surface == ModernWorkspaceSurface::None ?
    modern_workspace_surface(lookup, widget, modern_workspace_types()) : explicit_surface;
  if (!widget || (dark && surface == ModernWorkspaceSurface::None) || g_modern_workspace_style_applying ||
      !lookup.widget_palette_proc || !lookup.qpalette_copy_ctor_proc || !lookup.qpalette_destructor_proc ||
      !lookup.widget_set_palette_proc || !lookup.from_utf8_proc || !lookup.widget_set_stylesheet_proc) {
    return false;
  }
  const auto& property_api = widget_theme_api();
  const auto property = property_api.property;
  const auto set_property = property_api.set_property;
  const auto variant_destroy = property_api.variant_destroy;
  const auto variant_default = property_api.variant_default;
  const auto variant_bytes = property_api.variant_bytes;
  const auto bytes_ctor = property_api.bytes_ctor;
  const auto to_bytes = property_api.to_bytes;
  const auto type_name = property_api.type_name;
  const auto variant_data = property_api.variant_data;
  const auto palette_variant = property_api.palette_variant;
  if (!property || !set_property || !variant_destroy || !variant_default || !variant_bytes || !bytes_ctor ||
      !to_bytes || !type_name || !variant_data || !palette_variant) {
    return false;
  }
  std::string current;
  if (!read_omnibar_stylesheet(lookup, widget, current)) {
    return false;
  }
  constexpr const char* palette_key = "_darkmode_workspace_native_palette";
  constexpr const char* native_key = "_darkmode_workspace_native_qss";
  constexpr const char* themed_key = "_darkmode_workspace_themed_qss";
  const auto get_text = [&](const char* name) {
    alignas(16) unsigned char value[64] = {};
    alignas(16) unsigned char bytes[64] = {};
    property(widget, value, name);
    to_bytes(value, bytes);
    const long long size = reinterpret_cast<long long(__cdecl*)(const void*)>(lookup.qbytearray_size_proc)(bytes);
    const char* data = reinterpret_cast<const char*(__cdecl*)(const void*)>(lookup.qbytearray_const_data_proc)(bytes);
    std::string text;
    if (size > 0 && size <= 1024 * 1024 && data && memory_readable(data, static_cast<size_t>(size))) {
      text.assign(data, static_cast<size_t>(size));
    }
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.qbytearray_destructor_proc)(bytes);
    variant_destroy(value);
    return text;
  };
  const auto set_text = [&](const char* name, const std::string& text) {
    alignas(16) unsigned char bytes[64] = {};
    alignas(16) unsigned char value[64] = {};
    bytes_ctor(bytes, text.data(), static_cast<long long>(text.size()));
    variant_bytes(value, bytes);
    set_property(widget, name, value);
    variant_destroy(value);
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.qbytearray_destructor_proc)(bytes);
  };
  const auto set_stylesheet = [&](const std::string& text) {
    alignas(16) unsigned char string[64] = {};
    reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(
      string, text.data(), static_cast<long long>(text.size()));
    reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(widget, string);
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(string);
  };
  alignas(16) unsigned char original[64] = {};
  property(widget, original, palette_key);
  const char* original_type = type_name(original);
  const bool saved = original_type && std::strcmp(original_type, "QPalette") == 0;
  constexpr const char* stamp_key = "_darkmode_workspace_stamp";
  if (dark && saved && current == get_text(themed_key) &&
      widget_theme_stamp(lookup, widget, stamp_key, false, qwidget_style(lookup, widget),
        qwidget_parent_widget(lookup, widget), static_cast<size_t>(surface))) {
    variant_destroy(original);
    return true;
  }
#if defined(DARKMODE_RUNTIME_TESTING)
  ++g_test_workspace_style_preparations;
#endif
  g_modern_workspace_style_applying = true;
  const auto refresh_style = [&](size_t slot) {
    void* style = qwidget_style(lookup, widget);
    if (style && lookup.object_inherits_proc &&
        reinterpret_cast<bool(__cdecl*)(const void*, const char*)>(lookup.object_inherits_proc)(style, "QStyleSheetStyle")) {
      auto* methods = *static_cast<void***>(style);
      if (memory_readable(methods, (slot + 1) * sizeof(void*))) {
        reinterpret_cast<void(__cdecl*)(void*, void*)>(methods[slot])(style, widget);
      }
    }
  };
  constexpr size_t widget_unpolish_slot = 15;
  constexpr size_t widget_polish_slot = 13;
  if (dark || saved) {
    refresh_style(widget_unpolish_slot);
  }
  if (dark) {
    if (!saved) {
      variant_destroy(original);
      const void* palette = reinterpret_cast<const void*(__cdecl*)(const void*)>(lookup.widget_palette_proc)(widget);
      palette_variant(palette, original);
      set_property(widget, palette_key, original);
      set_text(native_key, current);
    } else if (current != get_text(themed_key)) {
      set_text(native_key, current);
    }
    const std::string themed = recolor_modern_workspace_stylesheet(get_text(native_key), surface);
    set_text(themed_key, themed);
    alignas(16) unsigned char palette[256] = {};
    reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.qpalette_copy_ctor_proc)(palette, variant_data(original));
    apply_blender_palette_roles(lookup, palette);
    for (int role : {0, 6, 8}) {
      set_qpalette_role_color(lookup, palette, {role, 216, 219, 226});
      set_qpalette_color_group_role_color(lookup, palette, 1, {role, 142, 147, 155});
    }
    reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_palette_proc)(widget, palette);
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpalette_destructor_proc)(palette);
    if (current != themed) {
      set_stylesheet(themed);
    }
    refresh_style(widget_polish_slot);
    if (surface == ModernWorkspaceSurface::TrayTitle || qt_object_inherits(lookup, widget, "QTabBar")) {
      static_cast<void>(install_style_control_hook(lookup, widget));
    }
    widget_theme_stamp(lookup, widget, stamp_key, true, qwidget_style(lookup, widget),
      qwidget_parent_widget(lookup, widget), static_cast<size_t>(surface));
  } else {
    if (saved) {
      reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_palette_proc)(widget, variant_data(original));
      if (current == get_text(themed_key)) {
        set_stylesheet(get_text(native_key));
      }
      refresh_style(widget_polish_slot);
      alignas(16) unsigned char empty[64] = {};
      variant_default(empty);
      for (const char* name : {palette_key, native_key, themed_key, stamp_key}) {
        set_property(widget, name, empty);
      }
      variant_destroy(empty);
    }
  }
  variant_destroy(original);
  g_modern_workspace_style_applying = false;
  return true;
}

size_t apply_notification_style(const QtLookup& lookup, void* widget, bool dark, const void* expected) {
  if (!expected || !pointer_looks_like_cpp_object(widget) ||
      *reinterpret_cast<const void* const*>(widget) != expected) {
    return 0;
  }
  std::vector<void*> widgets;
  std::unordered_set<std::uintptr_t> visited;
  collect_qwidget_descendants_from_qobject(lookup, widget, 0, 32, widgets, 65536, visited, "Notification", true);
  add_unique_pointer(widgets, widget);
  size_t applied = 0;
  for (void* child : widgets) {
    applied += apply_modern_workspace_widget_style(lookup, child, dark, ModernWorkspaceSurface::Notification) ? 1 : 0;
  }
  return applied;
}

size_t apply_existing_notification_styles(const QtLookup& lookup, bool dark) {
  if (!lookup.app_all_widgets_proc) {
    return 0;
  }
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, static_cast<size_t>(-1)));
  size_t applied = 0;
  for (void* widget : widgets) {
    applied += apply_notification_style(lookup, widget, dark);
  }
  return applied;
}

void __cdecl modern_workspace_stylesheet_hook(void* widget, const void* stylesheet) {
  const QtLookup& lookup = resolve_qt_lookup();
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(lookup.widget_set_stylesheet_proc)(widget, stylesheet);
  if (g_modern_workspace_dark && !g_modern_workspace_style_applying) {
    static_cast<void>(apply_modern_workspace_widget_style(lookup, widget, true));
  }
}

void __cdecl modern_workspace_show_hook(void* widget) {
  const QtLookup& lookup = resolve_qt_lookup();
  if (g_modern_workspace_dark && !g_modern_workspace_style_applying &&
      modern_workspace_surface(lookup, widget, modern_workspace_types()) != ModernWorkspaceSurface::None) {
    std::vector<void*> widgets;
    std::unordered_set<std::uintptr_t> visited;
    collect_qwidget_descendants_from_qobject(lookup, widget, 0, 32, widgets, 65536, visited, "Workspace show", true);
    for (void* child : widgets) {
      static_cast<void>(apply_modern_workspace_widget_style(lookup, child, true));
    }
  }
  static const auto original = reinterpret_cast<void(__cdecl*)(void*)>(
    GetProcAddress(GetModuleHandleW(L"Qt6Widgets.dll"), "?show@QWidget@@QEAAXXZ"));
  if (original) {
    original(widget);
  }
}

#if defined(DARKMODE_RUNTIME_TESTING)
HMODULE g_test_modern_workspace_setup_module = nullptr;
#endif

bool set_modern_workspace_style_hook(const QtLookup& lookup, bool enabled, HMODULE module = nullptr) {
  module = module ? module : GetModuleHandleW(nullptr);
#if defined(DARKMODE_RUNTIME_TESTING)
  g_test_modern_workspace_setup_module = module;
#endif
  g_modern_workspace_dark = enabled;
  if (!enabled) {
    clear_modern_workspace_palettes(lookup);
    const bool primitives_removed = set_modern_workspace_primitive_hook(false);
    const bool frame_removed = set_modern_workspace_frame_hook(false);
    const bool colors_removed = set_modern_workspace_color_hooks(false);
    for (auto hook = g_modern_workspace_style_hooks.begin(); hook != g_modern_workspace_style_hooks.end();) {
        if (*hook->slot == reinterpret_cast<void*>(&modern_workspace_stylesheet_hook) ||
          *hook->slot == reinterpret_cast<void*>(&modern_workspace_show_hook)) {
        DWORD protection = 0;
        if (!VirtualProtect(hook->slot, sizeof(void*), PAGE_READWRITE, &protection)) {
          ++hook;
          continue;
        }
        *hook->slot = hook->original_proc;
        DWORD unused = 0;
        VirtualProtect(hook->slot, sizeof(void*), protection, &unused);
      }
      hook = g_modern_workspace_style_hooks.erase(hook);
    }
    return primitives_removed && frame_removed && colors_removed && g_modern_workspace_style_hooks.empty();
  }
  if (modern_workspace_types().empty()) {
    return true;
  }
  if (!set_modern_workspace_color_hooks(true, module)) {
    return false;
  }
  static_cast<void>(set_modern_workspace_frame_hook(true));
  const auto* proxy_table = static_cast<const void* const*>(primary_rtti_vtable(
    find_vtables_for_type(".?AVSketchUpProxyStyle@@", module)));
  if (!memory_readable(proxy_table, 23 * sizeof(void*)) ||
      !install_style_control_slot(const_cast<void**>(proxy_table + 22))) {
    static_cast<void>(set_modern_workspace_primitive_hook(false));
    static_cast<void>(set_modern_workspace_frame_hook(false));
    static_cast<void>(set_modern_workspace_color_hooks(false));
    return false;
  }
  static_cast<void>(set_modern_workspace_primitive_hook(true, const_cast<void**>(proxy_table + 21)));
  if (!g_modern_workspace_style_hooks.empty()) {
    return std::all_of(g_modern_workspace_style_hooks.begin(), g_modern_workspace_style_hooks.end(),
      [&lookup](const VcbStatusPaintHook& hook) {
        const void* expected = hook.original_proc == reinterpret_cast<void*>(lookup.widget_set_stylesheet_proc)
          ? reinterpret_cast<void*>(&modern_workspace_stylesheet_hook) : reinterpret_cast<void*>(&modern_workspace_show_hook);
        return *hook.slot == expected;
      });
  }
  const std::pair<void*, void*> hooks[] = {
    {reinterpret_cast<void*>(lookup.widget_set_stylesheet_proc), reinterpret_cast<void*>(&modern_workspace_stylesheet_hook)},
    {reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"Qt6Widgets.dll"), "?show@QWidget@@QEAAXXZ")),
      reinterpret_cast<void*>(&modern_workspace_show_hook)}
  };
  for (const auto& replacement : hooks) {
    const auto slots = find_main_import_iat_slots_for_target(replacement.first, module);
    for (void** slot : slots) {
      DWORD protection = 0;
      if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
        static_cast<void>(set_modern_workspace_style_hook(lookup, false));
        return false;
      }
      g_modern_workspace_style_hooks.push_back({slot, *slot});
      *slot = replacement.second;
      DWORD unused = 0;
      VirtualProtect(slot, sizeof(void*), protection, &unused);
    }
  }
  return !g_modern_workspace_style_hooks.empty();
}

size_t apply_modern_workspace_styles(const QtLookup& lookup, bool dark) {
  if (modern_workspace_types().empty() || !lookup.app_all_widgets_proc) {
    return 0;
  }
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, static_cast<size_t>(-1)));
  size_t applied = 0;
  for (void* widget : widgets) {
    applied += apply_modern_workspace_widget_style(lookup, widget, dark) ? 1 : 0;
  }
  return applied;
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

bool icon_theme_enabled() {
  return InterlockedCompareExchange(&g_icon_theme_enabled, 0, 0) != 0;
}

bool is_svg_hex_digit(unsigned char value) {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
}

bool is_svg_hex_color(const unsigned char* color, const unsigned char* svg_end) {
  if (!color || color + 7 > svg_end || color[0] != '#') {
    return false;
  }
  for (size_t index = 1; index < 7; ++index) {
    if (!is_svg_hex_digit(color[index])) {
      return false;
    }
  }
  return color + 7 == svg_end || !is_svg_hex_digit(color[7]);
}

bool svg_color_is_non_rendering_metadata(const unsigned char* svg_begin, const unsigned char* color) {
  if (!svg_begin || !color || color <= svg_begin) {
    return false;
  }

  const auto* tag_begin = color;
  while (tag_begin > svg_begin && tag_begin[-1] != '<' && tag_begin[-1] != '>') {
    --tag_begin;
  }
  if (tag_begin == svg_begin || tag_begin[-1] != '<') {
    return false;
  }
  --tag_begin;

  const auto tag_starts_with = [tag_begin, color](const char* value) {
    const size_t length = std::strlen(value);
    return tag_begin + length <= color && std::memcmp(tag_begin, value, length) == 0;
  };
  return tag_starts_with("<!--") || tag_starts_with("<metadata") || tag_starts_with("<sodipodi:namedview");
}

bool has_only_black_svg_color(const unsigned char* svg_begin, size_t svg_size) {
  if (!svg_begin || svg_size < 7) {
    return false;
  }

  bool found_black = false;
  const auto* const svg_end = svg_begin + svg_size;
  for (const auto* color = svg_begin; color + 7 <= svg_end; ++color) {
    if (!is_svg_hex_color(color, svg_end)) {
      continue;
    }
    if (svg_color_is_non_rendering_metadata(svg_begin, color)) {
      color += 6;
      continue;
    }
    if (std::memcmp(color, "#000000", 7) != 0) {
      return false;
    }
    found_black = true;
    color += 6;
  }
  return found_black;
}

const char* svg_icon_theme_replacement(const unsigned char* color, bool replace_black, bool monochrome_control = false) {
  if (monochrome_control && (_memicmp(color, "#0e416c", 7) == 0 || _memicmp(color, "#009ad9", 7) == 0)) {
    return "#ffffff";
  }
  if (std::memcmp(color, "#0e416c", 7) == 0) {
    return "#1b70a8";
  }
  if (replace_black && std::memcmp(color, "#000000", 7) == 0) {
    return "#ffffff";
  }
  if (std::memcmp(color, "#ff0404", 7) == 0) {
    return "#df3f2f";
  }
  if (std::memcmp(color, "#231F20", 7) == 0) {
    return "#9da9b8";
  }
  if (std::memcmp(color, "#252A2E", 7) == 0) {
    return "#ffffff";
  }
  if (std::memcmp(color, "#363545", 7) == 0) {
    return "#ffffff";
  }
  return nullptr;
}

size_t patch_icon_theme_svg_colors(unsigned char* svg_begin, size_t svg_size, bool monochrome_control = false) {
  if (!svg_begin || svg_size < 7) {
    return 0;
  }

  DWORD original_protection = 0;
  if (!VirtualProtect(svg_begin, svg_size, PAGE_READWRITE, &original_protection)) {
    return 0;
  }

  size_t patched_count = 0;
  auto* const svg_end = svg_begin + svg_size;
  const bool replace_black = has_only_black_svg_color(svg_begin, svg_size);
  for (auto* color = svg_begin; color + 7 <= svg_end; ++color) {
    if (!is_svg_hex_color(color, svg_end)) {
      continue;
    }
    if (svg_color_is_non_rendering_metadata(svg_begin, color)) {
      color += 6;
      continue;
    }
    const char* replacement = svg_icon_theme_replacement(color, replace_black, monochrome_control);
    if (!replacement || std::memcmp(color, replacement, 7) == 0) {
      continue;
    }
    std::memcpy(color, replacement, 7);
    patched_count += 1;
    color += 6;
  }

  DWORD unused_protection = 0;
  VirtualProtect(svg_begin, svg_size, original_protection, &unused_protection);
  return patched_count;
}

size_t patch_unique_icon_theme_svg_token(const char* signature, const char* source_token,
                                         const char* replacement_token) {
  if (!signature || !source_token || !replacement_token ||
      std::strlen(source_token) != std::strlen(replacement_token)) {
    return 0;
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
    return 0;
  }

  const auto* const image_end = image_base + nt_headers->OptionalHeader.SizeOfImage;
  const size_t signature_size = std::strlen(signature);
  const auto* const signature_begin = reinterpret_cast<const unsigned char*>(signature);
  const auto* marker = static_cast<const unsigned char*>(nullptr);
  const auto* cursor = image_base;
  while (cursor + signature_size <= image_end) {
    const auto* candidate = std::search(cursor, image_end, signature_begin, signature_begin + signature_size);
    if (candidate == image_end) {
      break;
    }
    if (marker) {
      return 0;
    }
    marker = candidate;
    cursor = candidate + 1;
  }
  if (!marker) {
    return 0;
  }

  constexpr char svg_open[] = "<svg";
  constexpr char svg_close[] = "</svg>";
  const auto* svg_begin = static_cast<const unsigned char*>(nullptr);
  cursor = image_base;
  while (cursor + std::size(svg_open) - 1 <= marker) {
    const auto* candidate = std::search(cursor, marker, svg_open, svg_open + std::size(svg_open) - 1);
    if (candidate == marker) {
      break;
    }
    svg_begin = candidate;
    cursor = candidate + 1;
  }
  if (!svg_begin) {
    return 0;
  }

  const auto* svg_close_begin = std::search(marker, image_end, svg_close, svg_close + std::size(svg_close) - 1);
  if (svg_close_begin == image_end) {
    return 0;
  }
  const auto* const svg_end = svg_close_begin + std::size(svg_close) - 1;
  const size_t svg_size = static_cast<size_t>(svg_end - svg_begin);

  DWORD original_protection = 0;
  auto* const writable_svg = const_cast<unsigned char*>(svg_begin);
  if (!VirtualProtect(writable_svg, svg_size, PAGE_READWRITE, &original_protection)) {
    return 0;
  }

  const size_t token_size = std::strlen(source_token);
  size_t patched_count = 0;
  for (auto* token = writable_svg; token + token_size <= writable_svg + svg_size; ++token) {
    if (std::memcmp(token, source_token, token_size) != 0) {
      continue;
    }
    std::memcpy(token, replacement_token, token_size);
    patched_count += 1;
    token += token_size - 1;
  }

  DWORD unused_protection = 0;
  VirtualProtect(writable_svg, svg_size, original_protection, &unused_protection);
  return patched_count;
}

size_t patch_default_environment_svg() {
  const struct {
    const char* source_token;
    const char* replacement_token;
  } patches[] = {
    {"fill=\"white\"", "fill=\"#333 \""},
    {"#99A1B8", "#586072"},
    {"#6A6E79", "#c3cad6"},
    {"#6C6C6C", "#b9c0cc"}
  };

  size_t patched_count = 0;
  for (const auto& patch : patches) {
    patched_count += patch_unique_icon_theme_svg_token(
      "paint0_linear_43064_14721", patch.source_token, patch.replacement_token);
  }
  return patched_count;
}

bool refresh_panel_header_chevron(const QtLookup& lookup, void* header);

size_t refresh_tray_chevron_pixmaps(const QtLookup& lookup) {
  if (!lookup.app_all_widgets_proc) {
    return 0;
  }

  const auto panel_header_vtables = find_panel_header_vtables();
  if (panel_header_vtables.empty()) {
    return 0;
  }

  size_t refreshed_count = 0;
  size_t header_count = 0;
  std::vector<void*> widgets;
  const size_t widget_count = dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096);
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) ||
        std::find(panel_header_vtables.begin(), panel_header_vtables.end(), *reinterpret_cast<const void* const*>(widget)) == panel_header_vtables.end()) {
      continue;
    }

    header_count += 1;

    if (refresh_panel_header_chevron(lookup, widget)) {
      refreshed_count += 1;
    }
  }
  InterlockedExchange(&g_icon_theme_tray_chevron_widget_count, static_cast<LONG>(widget_count));
  InterlockedExchange(&g_icon_theme_tray_chevron_header_count, static_cast<LONG>(header_count));
  return refreshed_count;
}

bool refresh_toolbutton_icon_pixmap(
  void* button,
  FARPROC icon_getter_proc,
  FARPROC icon_setter_proc,
  FARPROC icon_destructor_proc
) {
  if (!button || !icon_getter_proc || !icon_setter_proc || !icon_destructor_proc) {
    return false;
  }

  using QtButtonIconGetterFn = void(__cdecl*)(const void*, void*);
  using QtButtonIconSetterFn = void(__cdecl*)(void*, const void*);
  using QtIconDestructorFn = void(__cdecl*)(void*);

  alignas(16) unsigned char icon_storage[64] = {};
  bool icon_constructed = false;
  bool refreshed = false;
  __try {
    reinterpret_cast<QtButtonIconGetterFn>(icon_getter_proc)(button, icon_storage);
    icon_constructed = true;
    reinterpret_cast<QtButtonIconSetterFn>(icon_setter_proc)(button, icon_storage);
    refreshed = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    refreshed = false;
  }
  if (icon_constructed) {
    reinterpret_cast<QtIconDestructorFn>(icon_destructor_proc)(icon_storage);
  }
  return refreshed;
}

size_t refresh_toolbutton_icon_pixmaps(const QtLookup& lookup) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_update_proc || !lookup.widget_repaint_proc) {
    return 0;
  }

  std::string icon_getter_module;
  std::string icon_getter_symbol;
  const FARPROC icon_getter_proc = find_export(lookup.widgets_modules, {
    "?icon@QAbstractButton@@QEBA?AVQIcon@@XZ"
  }, icon_getter_module, icon_getter_symbol);
  std::string icon_setter_module;
  std::string icon_setter_symbol;
  const FARPROC icon_setter_proc = find_export(lookup.widgets_modules, {
    "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z"
  }, icon_setter_module, icon_setter_symbol);
  std::string icon_destructor_module;
  std::string icon_destructor_symbol;
  const FARPROC icon_destructor_proc = find_export(lookup.gui_modules, {
    "??1QIcon@@QEAA@XZ"
  }, icon_destructor_module, icon_destructor_symbol);
  if (!icon_getter_proc || !icon_setter_proc || !icon_destructor_proc) {
    return 0;
  }

  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  size_t refreshed_count = 0;
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QToolButton")) {
      continue;
    }
    if (!refresh_toolbutton_icon_pixmap(widget, icon_getter_proc, icon_setter_proc, icon_destructor_proc)) {
      continue;
    }
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    refreshed_count += 1;
  }
  return refreshed_count;
}

struct Qt6PushButtonOptionView {
  int version;
  int type;
  unsigned int state;
  int direction;
  int rect[4];
  void* font_metrics;
  unsigned char palette[16];
  void* style_object;
  unsigned int features;
  unsigned int features_padding;
  unsigned char text[24];
  void* icon;
  int icon_size[2];
};
static_assert(offsetof(Qt6PushButtonOptionView, icon) == 96);
static_assert(offsetof(Qt6PushButtonOptionView, icon_size) == 104);
static_assert(sizeof(Qt6PushButtonOptionView) == 112);

struct SceneSearchIconApi {
  FARPROC icon_pixmap = nullptr;
  FARPROC pixmap_to_image = nullptr;
  FARPROC pixmap_from_image = nullptr;
  FARPROC pixmap_destructor = nullptr;
};
SceneSearchIconApi g_scene_search_icon_api;

bool resolve_scene_search_icon_api(const QtLookup& lookup) {
  auto& api = g_scene_search_icon_api;
  if (!api.icon_pixmap) {
    auto resolve = [&lookup](const char* symbol, bool widgets = false) {
      std::string module_name;
      std::string symbol_name;
      return find_export(widgets ? lookup.widgets_modules : lookup.gui_modules, {symbol}, module_name, symbol_name);
    };
    api.icon_pixmap = resolve("?pixmap@QIcon@@QEBA?AVQPixmap@@AEBVQSize@@NW4Mode@1@W4State@1@@Z");
    api.pixmap_to_image = resolve("?toImage@QPixmap@@QEBA?AVQImage@@XZ");
    api.pixmap_from_image = resolve("?fromImage@QPixmap@@SA?AV1@AEBVQImage@@V?$QFlags@W4ImageConversionFlag@Qt@@@@@Z");
    api.pixmap_destructor = resolve("??1QPixmap@@UEAA@XZ");
  }
  return api.icon_pixmap && api.pixmap_to_image && api.pixmap_from_image && api.pixmap_destructor;
}

struct ControlPixmapApi {
  long long(__cdecl* key)(const void*) = nullptr;
  int(__cdecl* width)(const void*) = nullptr;
  int(__cdecl* height)(const void*) = nullptr;
  double(__cdecl* ratio)(const void*) = nullptr;
  void(__cdecl* copy)(void*, const void*) = nullptr;
  void*(__cdecl* image_rect)(const void*, void*) = nullptr;
};

const ControlPixmapApi& control_pixmap_api() {
  static const auto api = [] {
    const HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
    ControlPixmapApi result;
    result.key = reinterpret_cast<decltype(result.key)>(GetProcAddress(gui, "?cacheKey@QPixmap@@QEBA_JXZ"));
    result.width = reinterpret_cast<decltype(result.width)>(GetProcAddress(gui, "?width@QPixmap@@QEBAHXZ"));
    result.height = reinterpret_cast<decltype(result.height)>(GetProcAddress(gui, "?height@QPixmap@@QEBAHXZ"));
    result.ratio = reinterpret_cast<decltype(result.ratio)>(GetProcAddress(gui, "?devicePixelRatio@QPixmap@@QEBANXZ"));
    result.copy = reinterpret_cast<decltype(result.copy)>(GetProcAddress(gui, "??0QPixmap@@QEAA@AEBV0@@Z"));
    result.image_rect = reinterpret_cast<decltype(result.image_rect)>(GetProcAddress(gui, "?rect@QImage@@QEBA?AVQRect@@XZ"));
    return result;
  }();
  return api;
}

struct ControlPixmapEntry {
  std::array<std::uint64_t, 7> key = {};
  std::uint64_t age = 0;
  size_t bytes = 0;
  alignas(16) unsigned char pixmap[256] = {};
};
std::vector<std::unique_ptr<ControlPixmapEntry>> g_control_pixmaps;
size_t g_control_pixmap_bytes = 0;
std::uint64_t g_control_pixmap_age = 0;
constexpr size_t control_pixmap_budget = 8 * 1024 * 1024;
#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_control_pixmap_builds = 0;
#endif

void clear_control_pixmaps() {
  for (const auto& entry : g_control_pixmaps) {
    reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(entry->pixmap);
  }
  g_control_pixmaps.clear();
  g_control_pixmap_bytes = 0;
}

bool create_tinted_control_pixmap(const QtLookup& lookup, const void* source, void* result, bool enabled,
                                  unsigned int foreground, int logical_width = 0, int logical_height = 0) {
  if (!source || !result || !resolve_toolbar_title_paint_api(lookup) || !resolve_scene_search_icon_api(lookup)) {
    return false;
  }
  const auto& cache_api = control_pixmap_api();
  if (!cache_api.image_rect || !cache_api.copy) {
    return false;
  }
  std::array<std::uint64_t, 7> key = {};
  size_t cost = 0;
  if (cache_api.key && cache_api.width && cache_api.height && cache_api.ratio) {
    const int width = cache_api.width(source);
    const int height = cache_api.height(source);
    const double ratio = cache_api.ratio(source);
    if (width > 0 && height > 0 && width <= 2048 && height <= 2048 && ratio > 0 && ratio <= 8) {
      key = {static_cast<std::uint64_t>(cache_api.key(source)), static_cast<std::uint64_t>(width),
        static_cast<std::uint64_t>(height), enabled ? foreground : 0x707070u,
        static_cast<std::uint64_t>(logical_width), static_cast<std::uint64_t>(logical_height), 0};
      std::memcpy(&key[6], &ratio, sizeof(ratio));
      cost = static_cast<size_t>(width) * height * 4;
      for (auto& entry : g_control_pixmaps) {
        if (entry->key == key) {
          entry->age = ++g_control_pixmap_age;
          cache_api.copy(result, entry->pixmap);
          return true;
        }
      }
    }
  }
  const auto& api = g_scene_search_icon_api;
  alignas(16) unsigned char image[256] = {};
  reinterpret_cast<void*(__cdecl*)(const void*, void*)>(api.pixmap_to_image)(source, image);
  const bool valid = !reinterpret_cast<bool(__cdecl*)(const void*)>(g_toolbar_title_api.image_is_null)(image);
  if (valid) {
    alignas(16) unsigned char painter[256] = {};
    alignas(16) unsigned char color[16] = {};
    reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color,
      enabled ? (foreground >> 16) & 255 : 112, enabled ? (foreground >> 8) & 255 : 112,
      enabled ? foreground & 255 : 112, 255);
    int icon_rect[4] = {};
    cache_api.image_rect(image, icon_rect);
    if (logical_width > 0 && logical_height > 0) {
      icon_rect[2] = logical_width - 1;
      icon_rect[3] = logical_height - 1;
    }
    reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
    reinterpret_cast<void(__cdecl*)(void*, int)>(g_toolbar_title_api.painter_composition)(painter, 5);
    reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, icon_rect, color);
    if (logical_width == 32 && logical_height == 32) {
      reinterpret_cast<void(__cdecl*)(void*, int)>(g_toolbar_title_api.painter_composition)(painter, 2);
      const int caps[2][4] = {{0, 0, 1, 1}, {0, 30, 1, 31}};
      for (const auto& cap : caps) {
        reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, cap, color);
      }
    }
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
    reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned int)>(api.pixmap_from_image)(result, image, 0);
#if defined(DARKMODE_RUNTIME_TESTING)
    ++g_test_control_pixmap_builds;
#endif
    if (key[0] && cost && cost <= control_pixmap_budget) {
      while (!g_control_pixmaps.empty() &&
          (g_control_pixmaps.size() >= 256 || g_control_pixmap_bytes + cost > control_pixmap_budget)) {
        const auto oldest = std::min_element(g_control_pixmaps.begin(), g_control_pixmaps.end(),
          [](const auto& first, const auto& second) { return first->age < second->age; });
        g_control_pixmap_bytes -= (*oldest)->bytes;
        reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)((*oldest)->pixmap);
        g_control_pixmaps.erase(oldest);
      }
      auto entry = std::make_unique<ControlPixmapEntry>();
      entry->key = key;
      entry->age = ++g_control_pixmap_age;
      entry->bytes = cost;
      cache_api.copy(entry->pixmap, result);
      g_control_pixmap_bytes += cost;
      g_control_pixmaps.push_back(std::move(entry));
    }
  }
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  return valid;
}

bool create_content_browser_search_pixmap(const QtLookup& lookup, const void* source, void* result, bool enabled,
                                         unsigned int foreground = 0xd8dbe2) {
  return create_tinted_control_pixmap(lookup, source, result, enabled, foreground);
}

bool refresh_panel_header_chevron(const QtLookup& lookup, void* header) {
  if (!header || !lookup.object_children_proc || !lookup.object_object_name_proc ||
      !resolve_toolbar_title_paint_api(lookup) || !resolve_scene_search_icon_api(lookup)) {
    return false;
  }
  const HMODULE widgets_module = GetModuleHandleW(L"Qt6Widgets.dll");
  const HMODULE gui_module = GetModuleHandleW(L"Qt6Gui.dll");
  const auto pixmap_get = reinterpret_cast<void*(__cdecl*)(const void*, void*)>(
    GetProcAddress(widgets_module, "?pixmap@QLabel@@QEBA?AVQPixmap@@XZ"));
  const auto pixmap_set = reinterpret_cast<void(__cdecl*)(void*, const void*)>(
    GetProcAddress(widgets_module, "?setPixmap@QLabel@@QEAAXAEBVQPixmap@@@Z"));
  const auto image_rect = reinterpret_cast<void*(__cdecl*)(const void*, void*)>(
    GetProcAddress(gui_module, "?rect@QImage@@QEBA?AVQRect@@XZ"));
  if (!pixmap_get || !pixmap_set || !image_rect) {
    return false;
  }
  std::vector<void*> children;
  std::unordered_set<std::uintptr_t> visited;
  collect_qwidget_descendants_from_qobject(lookup, header, 0, 2, children, 32, visited, "tray arrow", true);
  void* arrow = nullptr;
  for (void* child : children) {
    if (qwidget_parent_widget(lookup, child) == header && qt_object_inherits(lookup, child, "QLabel") &&
        qt_string_return_to_utf8(lookup, lookup.object_object_name_proc, child) == "arrow_") {
      if (arrow) {
        return false;
      }
      arrow = child;
    }
  }
  if (!arrow) {
    return false;
  }
  alignas(16) unsigned char source[256] = {};
  alignas(16) unsigned char image[256] = {};
  pixmap_get(arrow, source);
  reinterpret_cast<void*(__cdecl*)(const void*, void*)>(g_scene_search_icon_api.pixmap_to_image)(source, image);
  const bool valid = !reinterpret_cast<bool(__cdecl*)(const void*)>(g_toolbar_title_api.image_is_null)(image);
  if (valid) {
    alignas(16) unsigned char painter[256] = {};
    alignas(16) unsigned char color[16] = {};
    alignas(16) unsigned char tinted[256] = {};
    int bounds[4] = {};
    image_rect(image, bounds);
    reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color, 255, 255, 255, 255);
    reinterpret_cast<void(__cdecl*)(void*, void*)>(g_toolbar_title_api.painter_ctor)(painter, image);
    reinterpret_cast<void(__cdecl*)(void*, int)>(g_toolbar_title_api.painter_composition)(painter, 5);
    reinterpret_cast<void(__cdecl*)(void*, const void*, const void*)>(g_toolbar_title_api.fill_rect)(painter, bounds, color);
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.painter_destructor)(painter);
    reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned int)>(g_scene_search_icon_api.pixmap_from_image)(tinted, image, 0);
    pixmap_set(arrow, tinted);
    reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(tinted);
  }
  reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(image);
  reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(source);
  return valid;
}

const void* content_browser_search_vtable() {
  static const void* vtable = primary_rtti_vtable(find_vtables_for_type(".?AVContentBrowserSearchBar@@"));
  return vtable;
}

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_content_device_classifications = 0;
#endif

unsigned int content_browser_device_offset(const void* const* table) {
  static thread_local std::unordered_map<const void*, unsigned int> offsets;
  const auto found = offsets.find(table);
  if (found != offsets.end()) {
    return found->second;
  }
#if defined(DARKMODE_RUNTIME_TESTING)
  ++g_test_content_device_classifications;
#endif
  if (!table || !memory_readable(table - 1, sizeof(void*))) {
    return 0;
  }
  const auto* locator = static_cast<const unsigned int*>(table[-1]);
  const unsigned int offset = memory_readable(locator, 3 * sizeof(unsigned int)) && locator[0] == 1 &&
    locator[1] > 0 && locator[1] <= 4096 && locator[2] == 0 ? locator[1] : 0;
  MEMORY_BASIC_INFORMATION region = {};
  if (offsets.size() < 128 && VirtualQuery(table - 1, &region, sizeof(region)) == sizeof(region) &&
      region.Type == MEM_IMAGE && region.Protect == PAGE_READONLY &&
      (region.AllocationBase == GetModuleHandleW(nullptr) || region.AllocationBase == GetModuleHandleW(L"Qt6Widgets.dll") ||
       region.AllocationBase == GetModuleHandleW(L"Qt6Gui.dll"))) {
    offsets.emplace(table, offset);
  }
  return offset;
}

const void* content_browser_search_device_owner(const void* device, const void* expected_vtable) {
  if (!expected_vtable || !memory_readable(device, sizeof(void*))) {
    return nullptr;
  }
  const auto* table = *static_cast<const void* const* const*>(device);
  const unsigned int offset = content_browser_device_offset(table);
  if (!offset) {
    return nullptr;
  }
  const auto address = reinterpret_cast<std::uintptr_t>(device);
  if (address < 0x10000 + offset) {
    return nullptr;
  }
  const auto* owner = reinterpret_cast<const void*>(address - offset);
  return memory_readable(owner, sizeof(void*)) && *static_cast<const void* const*>(owner) == expected_vtable
    ? owner : nullptr;
}

using ContentBrowserDrawPixmapFn = void(__cdecl*)(void*, const void*, const void*);
ContentBrowserDrawPixmapFn g_content_browser_draw_pixmap = nullptr;
FARPROC g_content_browser_widget_enabled = nullptr;
bool g_content_browser_search_dark = false;
std::vector<VcbStatusPaintHook> g_content_browser_search_pixmap_hooks;

bool resolve_content_browser_search_icon_api(const QtLookup& lookup) {
  if (!g_content_browser_draw_pixmap) {
    std::string module_name;
    std::string symbol_name;
    g_content_browser_draw_pixmap = reinterpret_cast<ContentBrowserDrawPixmapFn>(find_export(lookup.gui_modules,
      {"?drawPixmap@QPainter@@QEAAXAEBVQRect@@AEBVQPixmap@@@Z"}, module_name, symbol_name));
    g_content_browser_widget_enabled = find_export(lookup.widgets_modules,
      {"?isEnabled@QWidget@@QEBA_NXZ"}, module_name, symbol_name);
  }
  return g_content_browser_draw_pixmap && g_content_browser_widget_enabled &&
    resolve_scene_search_icon_api(lookup) && resolve_toolbar_title_paint_api(lookup);
}

void __cdecl content_browser_search_draw_pixmap_hook(void* painter, const void* rect, const void* pixmap) {
  const void* device = g_content_browser_search_dark && painter && g_toolbar_title_api.painter_device
    ? reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.painter_device)(painter) : nullptr;
  const void* owner = g_content_browser_search_dark ?
    content_browser_search_device_owner(device, content_browser_search_vtable()) : nullptr;
  if (owner) {
    const bool enabled = reinterpret_cast<bool(__cdecl*)(const void*)>(g_content_browser_widget_enabled)(owner);
    alignas(16) unsigned char tinted[256] = {};
    if (create_content_browser_search_pixmap(resolve_qt_lookup(), pixmap, tinted, enabled)) {
      g_content_browser_draw_pixmap(painter, rect, tinted);
      reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(tinted);
      return;
    }
  }
  g_content_browser_draw_pixmap(painter, rect, pixmap);
}

bool set_content_browser_search_icon_hook(const QtLookup& lookup, bool dark) {
  g_content_browser_search_dark = false;
  if (!dark) {
    clear_control_pixmaps();
    for (auto hook = g_content_browser_search_pixmap_hooks.begin(); hook != g_content_browser_search_pixmap_hooks.end();) {
      if (*hook->slot == reinterpret_cast<void*>(&content_browser_search_draw_pixmap_hook)) {
        DWORD protection = 0;
        if (!VirtualProtect(hook->slot, sizeof(void*), PAGE_READWRITE, &protection)) {
          ++hook;
          continue;
        }
        *hook->slot = hook->original_proc;
        DWORD ignored = 0;
        VirtualProtect(hook->slot, sizeof(void*), protection, &ignored);
      }
      hook = g_content_browser_search_pixmap_hooks.erase(hook);
    }
    return g_content_browser_search_pixmap_hooks.empty();
  }
  if (!resolve_content_browser_search_icon_api(lookup)) {
    return false;
  }
  if (!g_content_browser_search_pixmap_hooks.empty()) {
    g_content_browser_search_dark = std::all_of(g_content_browser_search_pixmap_hooks.begin(),
      g_content_browser_search_pixmap_hooks.end(), [](const VcbStatusPaintHook& hook) {
        return *hook.slot == reinterpret_cast<void*>(&content_browser_search_draw_pixmap_hook);
      });
    return g_content_browser_search_dark;
  }
  const auto slots = find_main_import_iat_slots_for_target(
    reinterpret_cast<void*>(g_content_browser_draw_pixmap), GetModuleHandleW(nullptr));
  if (slots.empty()) {
    return false;
  }
  for (void** slot : slots) {
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      set_content_browser_search_icon_hook(lookup, false);
      return false;
    }
    g_content_browser_search_pixmap_hooks.push_back({slot, *slot});
    *slot = reinterpret_cast<void*>(&content_browser_search_draw_pixmap_hook);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), protection, &ignored);
  }
  g_content_browser_search_dark = true;
  return true;
}

struct SceneSearchPixmapContext {
  const void* icon;
  bool enabled;
};
thread_local const SceneSearchPixmapContext* g_scene_search_pixmap_context = nullptr;
std::vector<VcbStatusPaintHook> g_scene_search_pixmap_hooks;

void* __cdecl scene_search_pixmap_hook(const void* icon, void* result, const void* size,
                                     double ratio, int mode, int state) {
  const auto& api = g_scene_search_icon_api;
  reinterpret_cast<void*(__cdecl*)(const void*, void*, const void*, double, int, int)>(api.icon_pixmap)(
    icon, result, size, ratio, mode, state);
  const auto* context = g_scene_search_pixmap_context;
  if (!context || !memory_readable(icon, sizeof(void*)) ||
      *static_cast<const void* const*>(icon) != context->icon || !memory_readable(size, 2 * sizeof(int))) {
    return result;
  }
  const auto* dimensions = static_cast<const int*>(size);
  if (dimensions[0] <= 0 || dimensions[1] <= 0 || dimensions[0] > 256 || dimensions[1] > 256 ||
      !(ratio > 0 && ratio <= 8)) {
    return result;
  }
  alignas(16) unsigned char tinted[256] = {};
  if (create_tinted_control_pixmap(resolve_qt_lookup(), result, tinted, context->enabled, 0xd8dbe2,
      dimensions[0], dimensions[1])) {
    reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(result);
    control_pixmap_api().copy(result, tinted);
    reinterpret_cast<void(__cdecl*)(void*)>(api.pixmap_destructor)(tinted);
  }
  return result;
}

void remove_scene_search_pixmap_hooks() {
  clear_control_pixmaps();
  for (auto hook = g_scene_search_pixmap_hooks.begin(); hook != g_scene_search_pixmap_hooks.end();) {
    if (*hook->slot == reinterpret_cast<void*>(&scene_search_pixmap_hook)) {
      DWORD protection = 0;
      if (!VirtualProtect(hook->slot, sizeof(void*), PAGE_READWRITE, &protection)) {
        ++hook;
        continue;
      }
      *hook->slot = hook->original_proc;
      DWORD ignored = 0;
      VirtualProtect(hook->slot, sizeof(void*), protection, &ignored);
    }
    hook = g_scene_search_pixmap_hooks.erase(hook);
  }
}

bool install_scene_search_pixmap_hooks(const QtLookup& lookup) {
  if (!resolve_scene_search_icon_api(lookup)) {
    return false;
  }
  if (!g_scene_search_pixmap_hooks.empty()) {
    return std::all_of(g_scene_search_pixmap_hooks.begin(), g_scene_search_pixmap_hooks.end(),
      [](const VcbStatusPaintHook& hook) { return *hook.slot == reinterpret_cast<void*>(&scene_search_pixmap_hook); });
  }
  HMODULE widgets = GetModuleHandleW(L"Qt6Widgets.dll");
  if (!widgets) {
    return false;
  }
  auto slots = find_main_import_iat_slots_for_target(reinterpret_cast<void*>(g_scene_search_icon_api.icon_pixmap), widgets);
  if (slots.empty()) {
    return false;
  }
  const auto application_slots = find_main_import_iat_slots_for_target(
    reinterpret_cast<void*>(g_scene_search_icon_api.icon_pixmap), GetModuleHandleW(nullptr));
  slots.insert(slots.end(), application_slots.begin(), application_slots.end());
  for (void** slot : slots) {
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      remove_scene_search_pixmap_hooks();
      return false;
    }
    g_scene_search_pixmap_hooks.push_back({slot, *slot});
    *slot = reinterpret_cast<void*>(&scene_search_pixmap_hook);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), protection, &ignored);
  }
  return true;
}

bool draw_scene_search_button(const QtLookup& lookup, const void* style, const void* option,
                              void* painter, const void* widget, ToolbarTitleDrawFn original_proc, int control = 2) {
  if (!style || !painter || !original_proc || !memory_readable(option, sizeof(Qt6PushButtonOptionView)) ||
      !resolve_toolbar_title_paint_api(lookup) || !install_scene_search_pixmap_hooks(lookup)) {
    return false;
  }
  const auto* button = static_cast<const Qt6PushButtonOptionView*>(option);
  if (button->version != 1 || button->type != 2 || !button->icon || button->icon_size[0] <= 0 || button->icon_size[1] <= 0 ||
      button->icon_size[0] > 256 || button->icon_size[1] > 256) {
    return false;
  }
  const void* device = reinterpret_cast<const void*(__cdecl*)(const void*)>(g_toolbar_title_api.painter_device)(painter);
  if (!device) {
    return false;
  }
  const double ratio = reinterpret_cast<double(__cdecl*)(const void*)>(g_toolbar_title_api.device_ratio)(device);
  if (!(ratio > 0 && ratio <= 8)) {
    return false;
  }
  const SceneSearchPixmapContext context = {button->icon, (button->state & 1) != 0};
  const auto* previous = g_scene_search_pixmap_context;
  g_scene_search_pixmap_context = &context;
  original_proc(style, control, option, painter, widget);
  g_scene_search_pixmap_context = previous;
  return true;
}

std::vector<VcbStatusPaintHook> g_scene_search_hooks;
bool g_scene_search_dark = false;
thread_local bool g_scene_search_rendering = false;
volatile LONG g_scene_search_draw_count = 0;

const void* scene_search_owner_vtable() {
  static const void* vtable = primary_rtti_vtable(page_tabs_vtables());
  return vtable;
}

bool scene_search_owner_matches(const QtLookup& lookup, const void* owner, const void* button, const void* expected_vtable) {
  if (!button || !expected_vtable || !memory_readable(owner, sizeof(void*)) ||
      *static_cast<const void* const*>(owner) != expected_vtable) {
    return false;
  }
  const void* list = nullptr;
  if (!try_qobject_children_list(lookup.object_children_proc, owner, &list)) {
    return false;
  }
  const auto children = decode_qt_pointer_list(list);
  if (!children.items || children.size < 1 || children.size > 64) {
    return false;
  }
  const void* candidate = nullptr;
  for (long long index = 0; index < children.size; ++index) {
    void* child = children.items[index];
    if (qt_object_inherits(lookup, child, "QPushButton") && qwidget_parent_widget(lookup, child) == owner) {
      if (candidate) {
        return false;
      }
      candidate = child;
    }
  }
  return candidate == button;
}

bool content_browser_navigation_owner_matches(const QtLookup& lookup, const void* owner, const void* button,
                                             const void* search_vtable) {
  if (!search_vtable || !pointer_looks_like_cpp_object(owner) || !qt_object_inherits(lookup, button, "QPushButton") ||
      qwidget_parent_widget(lookup, const_cast<void*>(button)) != owner ||
      qt_string_return_to_utf8(lookup, lookup.object_object_name_proc, button) != "nav_menu_") {
    return false;
  }
  const void* list = nullptr;
  if (!try_qobject_children_list(lookup.object_children_proc, owner, &list)) {
    return false;
  }
  const auto children = decode_qt_pointer_list(list);
  if (!children.items || children.size < 1 || children.size > 128) {
    return false;
  }
  size_t search_count = 0;
  for (long long index = 0; index < children.size; ++index) {
    const void* child = children.items[index];
    if (memory_readable(child, sizeof(void*)) && *static_cast<const void* const*>(child) == search_vtable) {
      ++search_count;
    }
  }
  return search_count == 1;
}

bool is_content_browser_navigation_button(const QtLookup& lookup, const void* widget) {
  if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QPushButton")) {
    return false;
  }
  void* parent = qwidget_parent_widget(lookup, const_cast<void*>(widget));
  return qt_object_inherits(lookup, parent, "ContentBrowserPage") &&
    content_browser_navigation_owner_matches(lookup, parent, widget, content_browser_search_vtable());
}

bool is_scene_search_button(const QtLookup& lookup, const void* widget) {
  return pointer_looks_like_cpp_object(widget) && qt_object_inherits(lookup, widget, "QPushButton") &&
    scene_search_owner_matches(lookup, qwidget_parent_widget(lookup, const_cast<void*>(widget)), widget, scene_search_owner_vtable());
}

void __cdecl scene_search_label_hook(const void* style, int control, const void* option,
                                     void* painter, const void* widget) {
  void** slot = *reinterpret_cast<void***>(const_cast<void*>(style)) + 22;
  const auto hook = std::find_if(g_scene_search_hooks.begin(), g_scene_search_hooks.end(),
    [slot](const VcbStatusPaintHook& candidate) { return candidate.slot == slot; });
  if (hook == g_scene_search_hooks.end() || !hook->original_proc) {
    return;
  }
  const auto original_proc = reinterpret_cast<ToolbarTitleDrawFn>(hook->original_proc);
  if (g_modern_workspace_dark && draw_modern_workspace_control(resolve_qt_lookup(), style, control,
      option, painter, widget, original_proc)) {
    return;
  }
  if (g_toolbar_title_dark && control == 30 && widget) {
    const QtLookup& lookup = resolve_qt_lookup();
    if (is_sketchup_toolbar(lookup, widget) && !qt_object_inherits(lookup, widget, "NonCustomizableToolbar") &&
        reinterpret_cast<bool(__cdecl*)(const void*)>(lookup.widget_is_window_proc)(widget) &&
        draw_legacy_toolbar_title(lookup, style, option, painter, widget, original_proc)) {
      return;
    }
  }
  if (g_scene_search_dark && !g_scene_search_rendering && control == 0 && widget) {
    const QtLookup& lookup = resolve_qt_lookup();
    const bool scene_search = is_scene_search_button(lookup, widget);
    if (scene_search || is_content_browser_navigation_button(lookup, widget)) {
      g_scene_search_rendering = true;
      const bool painted = draw_scene_search_button(lookup, style, option, painter, widget, original_proc, control);
      g_scene_search_rendering = false;
      if (painted) {
        if (scene_search) {
          InterlockedIncrement(&g_scene_search_draw_count);
        }
        return;
      }
    }
  }
  original_proc(style, control, option, painter, widget);
}

bool install_style_control_hook(const QtLookup& lookup, const void* widget) {
  void* style = qwidget_style(lookup, widget);
  if (!pointer_looks_like_cpp_object(style) || !memory_readable(*static_cast<void***>(style), 23 * sizeof(void*))) {
    return false;
  }
  return install_style_control_slot(*static_cast<void***>(style) + 22);
}

bool install_style_control_slot(void** slot) {
  for (const VcbStatusPaintHook& hook : g_scene_search_hooks) {
    if (hook.slot == slot) {
      return *slot == reinterpret_cast<void*>(&scene_search_label_hook);
    }
  }
  MEMORY_BASIC_INFORMATION memory = {};
  if (!*slot || !VirtualQuery(slot, &memory, sizeof(memory)) || memory.Type != MEM_IMAGE) {
    return false;
  }
  g_scene_search_hooks.push_back({slot, *slot});
  DWORD previous_protection = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &previous_protection)) {
    g_scene_search_hooks.pop_back();
    return false;
  }
  *slot = reinterpret_cast<void*>(&scene_search_label_hook);
  DWORD ignored_protection = 0;
  VirtualProtect(slot, sizeof(void*), previous_protection, &ignored_protection);
  return true;
}

bool install_scene_search_label_hook(const QtLookup& lookup, const void* widget) {
  if (!resolve_toolbar_title_paint_api(lookup) || !install_scene_search_pixmap_hooks(lookup) ||
      !install_style_control_hook(lookup, widget)) {
    return false;
  }
  g_scene_search_dark = true;
  return true;
}

void remove_unused_style_control_hooks() {
  if (g_scene_search_dark || g_toolbar_title_dark || g_modern_workspace_dark) {
    return;
  }
  for (auto hook = g_scene_search_hooks.begin(); hook != g_scene_search_hooks.end();) {
    if (*hook->slot == reinterpret_cast<void*>(&scene_search_label_hook)) {
      DWORD previous_protection = 0;
      if (!VirtualProtect(hook->slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &previous_protection)) {
        ++hook;
        continue;
      }
      *hook->slot = hook->original_proc;
      DWORD ignored_protection = 0;
      VirtualProtect(hook->slot, sizeof(void*), previous_protection, &ignored_protection);
    }
    hook = g_scene_search_hooks.erase(hook);
  }
}

void remove_scene_search_label_hooks() {
  g_scene_search_dark = false;
  remove_scene_search_pixmap_hooks();
  remove_unused_style_control_hooks();
}

size_t refresh_content_browser_filter_icons(const QtLookup& lookup) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_update_proc || !lookup.widget_repaint_proc) {
    return 0;
  }
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  size_t refreshed = 0;
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }
    const bool search = *static_cast<const void* const*>(widget) == content_browser_search_vtable();
    const bool navigation = !search && is_content_browser_navigation_button(lookup, widget);
    if (!search && !navigation) {
      continue;
    }
    if (navigation && g_scene_search_dark && !install_scene_search_label_hook(lookup, widget)) {
      continue;
    }
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.widget_update_proc)(widget);
    reinterpret_cast<void(__cdecl*)(void*)>(lookup.widget_repaint_proc)(widget);
    ++refreshed;
  }
  return refreshed;
}

bool refresh_page_tabs_search_icon(const QtLookup& lookup, void* shown_widget) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_parent_widget_proc ||
      !lookup.widget_update_proc || !lookup.widget_repaint_proc) {
    return false;
  }

  const auto& expected_vtables = page_tabs_vtables();
  const void* expected_primary_vtable = scene_search_owner_vtable();
  if (!expected_primary_vtable) {
    return false;
  }

  void* page_tabs = nullptr;
  if (shown_widget) {
    // Most show events belong to unrelated dialogs or controls. Check only a
    // bounded parent chain before resolving icon symbols or walking children.
    using QtShownWidgetParentFn = void*(__cdecl*)(const void*);
    void* current = shown_widget;
    for (int depth = 0; current && depth < 8; ++depth) {
      if (!pointer_looks_like_cpp_object(current)) {
        return false;
      }
      const void* vtable = *reinterpret_cast<const void* const*>(current);
      if (std::find(expected_vtables.begin(), expected_vtables.end(), vtable) != expected_vtables.end()) {
        page_tabs = current;
        break;
      }
      current = reinterpret_cast<QtShownWidgetParentFn>(lookup.widget_parent_widget_proc)(current);
    }
    if (!page_tabs) {
      return false;
    }
  }

  std::string icon_getter_module;
  std::string icon_getter_symbol;
  const FARPROC icon_getter_proc = find_export(lookup.widgets_modules, {
    "?icon@QAbstractButton@@QEBA?AVQIcon@@XZ"
  }, icon_getter_module, icon_getter_symbol);
  std::string icon_setter_module;
  std::string icon_setter_symbol;
  const FARPROC icon_setter_proc = find_export(lookup.widgets_modules, {
    "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z"
  }, icon_setter_module, icon_setter_symbol);
  std::string icon_destructor_module;
  std::string icon_destructor_symbol;
  const FARPROC icon_destructor_proc = find_export(lookup.gui_modules, {
    "??1QIcon@@QEAA@XZ"
  }, icon_destructor_module, icon_destructor_symbol);
  if (!icon_getter_proc || !icon_setter_proc || !icon_destructor_proc) {
    return false;
  }

  using QtParentWidgetFn = void*(__cdecl*)(const void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);

  std::vector<void*> widgets;
  if (page_tabs) {
    std::unordered_set<std::uintptr_t> visited;
    collect_qwidget_descendants_from_qobject(lookup, page_tabs, 0, 8, widgets, 256, visited, "Page tabs", true);
  } else {
    // Full discovery is reserved for explicit theme application at startup.
    static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  }
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QPushButton")) {
      continue;
    }
    void* parent = reinterpret_cast<QtParentWidgetFn>(lookup.widget_parent_widget_proc)(widget);
    if (!scene_search_owner_matches(lookup, parent, widget, expected_primary_vtable)) {
      continue;
    }
    if (g_scene_search_dark &&
        !install_scene_search_label_hook(lookup, widget)) {
      return false;
    }
    if (!g_scene_search_dark &&
      !refresh_toolbutton_icon_pixmap(widget, icon_getter_proc, icon_setter_proc, icon_destructor_proc)) {
      return false;
    }

    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    return true;
  }
  return false;
}

void write_page_tabs_pushbutton_inventory(const QtLookup& lookup) {
  if (g_icon_theme_inventory_directory.empty() || !lookup.app_all_widgets_proc || !lookup.widget_parent_widget_proc) {
    return;
  }

  const std::filesystem::path inventory_path = g_icon_theme_inventory_directory / L"page-tabs-pushbutton-inventory.txt";
  std::ofstream output(inventory_path, std::ios::trunc);
  if (!output) {
    return;
  }

  using QtParentWidgetFn = void*(__cdecl*)(const void*);
  std::vector<void*> widgets;
  const size_t widget_count = dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096);
  output << "all-widgets=" << widget_count << '\n';

  size_t button_count = 0;
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QPushButton")) {
      continue;
    }

    void* parent = reinterpret_cast<QtParentWidgetFn>(lookup.widget_parent_widget_proc)(widget);
    const void* button_vtable = *reinterpret_cast<const void* const*>(widget);
    const void* parent_vtable = pointer_looks_like_cpp_object(parent)
      ? *reinterpret_cast<const void* const*>(parent)
      : nullptr;
    output << "button=" << pointer_hex(widget) << " button-vtable=" << pointer_hex(button_vtable)
           << " button-class=" << qobject_class_name_quiet(lookup, widget)
           << " parent=" << pointer_hex(parent) << " parent-vtable=" << pointer_hex(parent_vtable)
           << " parent-class=" << qobject_class_name_quiet(lookup, parent) << '\n';
    button_count += 1;
  }
  output << "pushbuttons=" << button_count << '\n';
}

bool refresh_component_navigator_search_icon(const QtLookup& lookup) {
  if (!lookup.app_all_widgets_proc || !lookup.from_utf8_proc || !lookup.qstring_destructor_proc ||
      !lookup.object_object_name_proc || !lookup.widget_set_stylesheet_proc || !lookup.widget_stylesheet_proc ||
      !lookup.widget_update_proc || !lookup.widget_repaint_proc) {
    return false;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtSetStyleSheetFn = void(__cdecl*)(void*, const void*);
  using QtDestructorFn = void(__cdecl*)(void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  static constexpr char search_icon_rule[] =
    "QPushButton { qproperty-icon: url(:/dlg_search); }\n";

  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QPushButton") ||
        qt_string_return_to_utf8(lookup, lookup.object_object_name_proc, widget) != "m_menuBtn") {
      continue;
    }

    const std::string existing_stylesheet = qt_string_return_to_utf8(
      lookup, lookup.widget_stylesheet_proc, widget);
    if (existing_stylesheet.find(search_icon_rule) == std::string::npos) {
      const std::string stylesheet = existing_stylesheet.empty()
        ? search_icon_rule
        : existing_stylesheet + "\n" + search_icon_rule;
      alignas(16) unsigned char qstring_storage[64] = {};
      if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
        reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(
          qstring_storage, stylesheet.c_str(), static_cast<long long>(stylesheet.size()));
      } else {
        reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(
          qstring_storage, stylesheet.c_str(), static_cast<int>(stylesheet.size()));
      }
      reinterpret_cast<QtSetStyleSheetFn>(lookup.widget_set_stylesheet_proc)(widget, qstring_storage);
      reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
    }

    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    return true;
  }
  return false;
}

bool qt_object_has_ancestor_class(const QtLookup& lookup, const void* object, const char* class_name) {
  using QtParentFn = void*(__cdecl*)(const void*);
  if (!object || !class_name || !lookup.object_parent_proc) {
    return false;
  }

  const void* current = object;
  for (size_t depth = 0; current && depth < 16; ++depth) {
    if (!pointer_looks_like_cpp_object(current)) {
      return false;
    }
    if (qobject_class_name_quiet(lookup, current) == class_name) {
      return true;
    }
    current = reinterpret_cast<QtParentFn>(lookup.object_parent_proc)(current);
  }
  return false;
}

bool qt_object_has_ancestor_vtable(const QtLookup& lookup, const void* object, const void* expected_vtable) {
  using QtParentFn = void*(__cdecl*)(const void*);
  if (!object || !expected_vtable || !lookup.object_parent_proc) {
    return false;
  }

  const void* current = object;
  for (size_t depth = 0; current && depth < 16; ++depth) {
    if (!pointer_looks_like_cpp_object(current)) {
      return false;
    }
    if (*reinterpret_cast<const void* const*>(current) == expected_vtable) {
      return true;
    }
    current = reinterpret_cast<QtParentFn>(lookup.object_parent_proc)(current);
  }
  return false;
}

void write_scene_search_widget_inventory(const QtLookup& lookup) {
  if (g_icon_theme_inventory_directory.empty() || !lookup.app_all_widgets_proc || !lookup.object_parent_proc) {
    return;
  }

  const std::filesystem::path inventory_path = g_icon_theme_inventory_directory / L"scene-search-widget-inventory.txt";
  std::ofstream output(inventory_path, std::ios::trunc);
  if (!output) {
    return;
  }

  output << "parent-class=CComponentNavigatorDlg\n";
  std::vector<void*> widgets;
  const size_t widget_count = dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096);
  output << "all-widgets=" << widget_count << '\n';

  size_t matching_count = 0;
  for (void* widget : widgets) {
    if (!qt_object_has_ancestor_class(lookup, widget, "CComponentNavigatorDlg")) {
      continue;
    }
    const void* vtable = *reinterpret_cast<const void* const*>(widget);
    output << "index=" << matching_count << " vtable=" << pointer_hex(vtable) << ' '
           << qt_object_detail(lookup, widget) << '\n';
    matching_count += 1;
  }
  output << "matching-widgets=" << matching_count << '\n';
}

size_t patch_compressed_control_svg(const QtLookup& lookup, unsigned char* data, size_t size) {
  if (!data || size < 4 || size > 1024 * 1024 || !memory_readable(data, size) ||
      !lookup.qbytearray_size_proc || !lookup.qbytearray_const_data_proc || !lookup.qbytearray_destructor_proc) {
    return 0;
  }
  const size_t expected_size = (static_cast<std::uint32_t>(data[0]) << 24) |
    (static_cast<std::uint32_t>(data[1]) << 16) | (static_cast<std::uint32_t>(data[2]) << 8) | data[3];
  if (expected_size == 0 || expected_size > 1024 * 1024) {
    return 0;
  }
  std::string module;
  std::string symbol;
  const auto uncompress = reinterpret_cast<void*(__cdecl*)(void*, const unsigned char*, long long)>(
    find_export(lookup.core_modules, {"?qUncompress@@YA?AVQByteArray@@PEBE_J@Z"}, module, symbol));
  const auto compress = reinterpret_cast<void*(__cdecl*)(void*, const unsigned char*, long long, int)>(
    find_export(lookup.core_modules, {"?qCompress@@YA?AVQByteArray@@PEBE_JH@Z"}, module, symbol));
  if (!uncompress || !compress) {
    return 0;
  }
  const auto array_size = reinterpret_cast<long long(__cdecl*)(const void*)>(lookup.qbytearray_size_proc);
  const auto array_data = reinterpret_cast<const unsigned char*(__cdecl*)(const void*)>(lookup.qbytearray_const_data_proc);
  const auto destroy = reinterpret_cast<void(__cdecl*)(void*)>(lookup.qbytearray_destructor_proc);
  alignas(16) unsigned char decoded[64] = {};
  uncompress(decoded, data, static_cast<long long>(size));
  std::vector<unsigned char> svg;
  if (array_size(decoded) == static_cast<long long>(expected_size) && memory_readable(array_data(decoded), expected_size)) {
    svg.assign(array_data(decoded), array_data(decoded) + expected_size);
  }
  destroy(decoded);
  constexpr char svg_open[] = "<svg";
  constexpr char svg_close[] = "</svg>";
  if (svg.empty() || std::search(svg.begin(), svg.end(), svg_open, svg_open + sizeof(svg_open) - 1) == svg.end() ||
      std::search(svg.begin(), svg.end(), svg_close, svg_close + sizeof(svg_close) - 1) == svg.end()) {
    return 0;
  }
  const size_t changed = patch_icon_theme_svg_colors(svg.data(), svg.size(), true);
  if (changed == 0) {
    return 0;
  }
  alignas(16) unsigned char encoded[64] = {};
  compress(encoded, svg.data(), static_cast<long long>(svg.size()), 9);
  const long long encoded_size = array_size(encoded);
  size_t patched = 0;
  DWORD protection = 0;
  if (encoded_size > 0 && static_cast<size_t>(encoded_size) <= size &&
      memory_readable(array_data(encoded), static_cast<size_t>(encoded_size)) &&
      VirtualProtect(data, size, PAGE_READWRITE, &protection)) {
    std::memcpy(data, array_data(encoded), static_cast<size_t>(encoded_size));
    std::memset(data + encoded_size, 0, size - static_cast<size_t>(encoded_size));
    DWORD unused = 0;
    VirtualProtect(data, size, protection, &unused);
    patched = changed;
  }
  destroy(encoded);
  return patched;
}

size_t patch_icon_theme_qresource_svg(const QtLookup& lookup, const char* alias, bool monochrome_control = false) {
  if (!alias || !lookup.from_utf8_proc || !lookup.qstring_destructor_proc) {
    return 0;
  }

  std::string resource_module;
  std::string resource_symbol;
  const FARPROC resource_constructor_proc = find_export(lookup.core_modules, {
    "??0QResource@@QEAA@AEBVQString@@AEBVQLocale@@@Z"
  }, resource_module, resource_symbol);
  const FARPROC resource_destructor_proc = find_export(lookup.core_modules, {
    "??1QResource@@QEAA@XZ"
  }, resource_module, resource_symbol);
  const FARPROC locale_constructor_proc = find_export(lookup.core_modules, {
    "??0QLocale@@QEAA@XZ"
  }, resource_module, resource_symbol);
  const FARPROC locale_destructor_proc = find_export(lookup.core_modules, {
    "??1QLocale@@QEAA@XZ"
  }, resource_module, resource_symbol);
  const FARPROC resource_is_valid_proc = find_export(lookup.core_modules, {
    "?isValid@QResource@@QEBA_NXZ"
  }, resource_module, resource_symbol);
  const FARPROC resource_compression_proc = find_export(lookup.core_modules, {
    "?compressionAlgorithm@QResource@@QEBA?AW4Compression@1@XZ"
  }, resource_module, resource_symbol);
  const FARPROC resource_data_proc = find_export(lookup.core_modules, {
    "?data@QResource@@QEBAPEBEXZ"
  }, resource_module, resource_symbol);
  const FARPROC resource_size_proc = find_export(lookup.core_modules, {
    "?size@QResource@@QEBA_JXZ"
  }, resource_module, resource_symbol);
  if (!resource_constructor_proc || !resource_destructor_proc || !locale_constructor_proc || !locale_destructor_proc ||
      !resource_is_valid_proc || !resource_compression_proc || !resource_data_proc || !resource_size_proc) {
    return 0;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtNoArgCtorFn = void(__cdecl*)(void*);
  using QtDestructorFn = void(__cdecl*)(void*);
  using QtResourceCtorFn = void(__cdecl*)(void*, const void*, const void*);
  using QtResourceBoolFn = bool(__cdecl*)(const void*);
  using QtResourceDataFn = const unsigned char*(__cdecl*)(const void*);
  using QtResourceSizeFn = long long(__cdecl*)(const void*);

  alignas(16) unsigned char qstring_storage[64] = {};
  alignas(16) unsigned char locale_storage[64] = {};
  alignas(16) unsigned char resource_storage[256] = {};
  const size_t alias_length = std::strlen(alias);
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(qstring_storage, alias, static_cast<long long>(alias_length));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(qstring_storage, alias, static_cast<int>(alias_length));
  }

  reinterpret_cast<QtNoArgCtorFn>(locale_constructor_proc)(locale_storage);
  reinterpret_cast<QtResourceCtorFn>(resource_constructor_proc)(resource_storage, qstring_storage, locale_storage);

  size_t patched_count = 0;
  const bool valid = reinterpret_cast<QtResourceBoolFn>(resource_is_valid_proc)(resource_storage);
  const int compression = valid ? reinterpret_cast<int(__cdecl*)(const void*)>(resource_compression_proc)(resource_storage) : -1;
  const long long resource_size = valid && (compression == 0 || (compression == 1 && monochrome_control))
    ? reinterpret_cast<QtResourceSizeFn>(resource_size_proc)(resource_storage)
    : 0;
  const auto* data = resource_size > 0
    ? reinterpret_cast<QtResourceDataFn>(resource_data_proc)(resource_storage)
    : nullptr;
  if (data && compression == 1) {
    patched_count = patch_compressed_control_svg(lookup, const_cast<unsigned char*>(data), static_cast<size_t>(resource_size));
  } else if (data && resource_size > 0 && resource_size <= 1024 * 1024 &&
      memory_readable(data, static_cast<size_t>(resource_size))) {
    constexpr char svg_open[] = "<svg";
    constexpr char svg_close[] = "</svg>";
    const auto* resource_end = data + static_cast<size_t>(resource_size);
    const auto* svg_begin = std::search(data, resource_end, svg_open, svg_open + std::size(svg_open) - 1);
    if (svg_begin != resource_end) {
      const auto* svg_close_begin = std::search(svg_begin, resource_end, svg_close, svg_close + std::size(svg_close) - 1);
      if (svg_close_begin != resource_end) {
        const auto* svg_end = svg_close_begin + std::size(svg_close) - 1;
        patched_count = patch_icon_theme_svg_colors(
          const_cast<unsigned char*>(svg_begin), static_cast<size_t>(svg_end - svg_begin), monochrome_control);
      }
    }
  }

  reinterpret_cast<QtDestructorFn>(resource_destructor_proc)(resource_storage);
  reinterpret_cast<QtDestructorFn>(locale_destructor_proc)(locale_storage);
  reinterpret_cast<QtDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  return patched_count;
}

bool refresh_modern_tray_hide_icon(const QtLookup& lookup, void* widget) {
  if (!qt_object_inherits(lookup, widget, "QToolButton") ||
      !qt_object_inherits(lookup, qwidget_parent_widget(lookup, widget), "CPanelHeader") ||
      !lookup.object_object_name_proc || !lookup.qstring_destructor_proc || !resolve_scene_search_icon_api(lookup)) {
    return false;
  }
  alignas(16) unsigned char name[64] = {};
  reinterpret_cast<void*(__cdecl*)(const void*, void*)>(lookup.object_object_name_proc)(widget, name);
  const bool close_button = qstring_to_utf8(lookup, name) == "hide_button_";
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(name);
  if (!close_button) {
    return false;
  }
  HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
  HMODULE widgets = GetModuleHandleW(L"Qt6Widgets.dll");
  const auto icon_get = reinterpret_cast<void*(__cdecl*)(const void*, void*)>(
    GetProcAddress(widgets, "?icon@QAbstractButton@@QEBA?AVQIcon@@XZ"));
  const auto icon_set = reinterpret_cast<void(__cdecl*)(void*, const void*)>(
    GetProcAddress(widgets, "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z"));
  const auto icon_size = reinterpret_cast<void*(__cdecl*)(const void*, void*)>(
    GetProcAddress(widgets, "?iconSize@QAbstractButton@@QEBA?AVQSize@@XZ"));
  const auto ratio = reinterpret_cast<double(__cdecl*)(const void*)>(
    GetProcAddress(gui, "?devicePixelRatioF@QPaintDevice@@QEBANXZ"));
  const auto icon_ctor = reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(gui, "??0QIcon@@QEAA@XZ"));
  const auto icon_destroy = reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(gui, "??1QIcon@@QEAA@XZ"));
  const auto add_pixmap = reinterpret_cast<void(__cdecl*)(void*, const void*, int, int)>(
    GetProcAddress(gui, "?addPixmap@QIcon@@QEAAXAEBVQPixmap@@W4Mode@1@W4State@1@@Z"));
  if (!icon_get || !icon_set || !icon_size || !ratio || !icon_ctor || !icon_destroy || !add_pixmap) {
    return false;
  }
  int size[2] = {};
  icon_size(widget, size);
  const double scale = ratio(static_cast<const unsigned char*>(widget) + 16);
  if (size[0] < 1 || size[1] < 1 || size[0] > 256 || size[1] > 256 || scale <= 0 || scale > 8) {
    return false;
  }
  alignas(16) unsigned char original[64] = {};
  alignas(16) unsigned char replacement[64] = {};
  icon_get(widget, original);
  icon_ctor(replacement);
  bool complete = true;
  for (int mode : {0, 1, 2, 3}) {
    for (int state : {0, 1}) {
      alignas(16) unsigned char source[256] = {};
      alignas(16) unsigned char tinted[256] = {};
      reinterpret_cast<void*(__cdecl*)(const void*, void*, const void*, double, int, int)>(
        g_scene_search_icon_api.icon_pixmap)(original, source, size, scale, mode, state);
      if (create_content_browser_search_pixmap(lookup, source, tinted, mode != 1, 0xffffff)) {
        add_pixmap(replacement, tinted, mode, state);
        reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(tinted);
      } else {
        complete = false;
      }
      reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(source);
    }
  }
  if (complete) {
    icon_set(widget, replacement);
  }
  icon_destroy(replacement);
  icon_destroy(original);
  return complete;
}

std::vector<void*> collect_modern_control_icon_objects(const QtLookup& lookup) {
  std::vector<void*> objects;
  std::vector<void*> owners;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, objects, static_cast<size_t>(-1)));
  std::unordered_set<void*> visited;
  for (size_t index = 0; index < objects.size() && visited.size() < 65536; ++index) {
    void* object = objects[index];
    if (!visited.insert(object).second || !pointer_looks_like_cpp_object(object)) {
      continue;
    }
    if (qt_object_inherits(lookup, object, "QAbstractButton") || qt_object_inherits(lookup, object, "QAction")) {
      owners.push_back(object);
    }
    const void* list = nullptr;
    if (try_qobject_children_list(lookup.object_children_proc, object, &list)) {
      const auto children = decode_qt_pointer_list(list);
      if (children.items && children.size > 0 && children.size <= 65536 &&
          memory_readable(children.items, static_cast<size_t>(children.size) * sizeof(void*))) {
        objects.insert(objects.end(), children.items, children.items + children.size);
      }
    }
  }
  return owners;
}

size_t patch_modern_workspace_control_resource(const QtLookup& lookup, const char* alias,
                                               const std::vector<void*>* inventory = nullptr) {
  const bool refresh_cached_icon = std::strcmp(alias, ":/tb_copy") == 0 ||
    std::strcmp(alias, ":/dlg_tray_dialog_hide") == 0 || std::strcmp(alias, ":/dlg_in_model") == 0 ||
    std::strcmp(alias, ":/dlg_update") == 0 || std::strcmp(alias, ":/dlg_move_item_up") == 0 ||
    std::strcmp(alias, ":/dlg_move_item_down") == 0;
  if (!refresh_cached_icon || !lookup.app_all_widgets_proc ||
      !lookup.from_utf8_proc || !lookup.qstring_destructor_proc) {
    return patch_icon_theme_qresource_svg(lookup, alias, true);
  }
  HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
  HMODULE widgets = GetModuleHandleW(L"Qt6Widgets.dll");
  const auto icon_ctor = reinterpret_cast<void(__cdecl*)(void*, const void*)>(
    GetProcAddress(gui, "??0QIcon@@QEAA@AEBVQString@@@Z"));
  const auto icon_destroy = reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(gui, "??1QIcon@@QEAA@XZ"));
  const auto pixmap = reinterpret_cast<void*(__cdecl*)(const void*, void*, int, int, int, int)>(
    GetProcAddress(gui, "?pixmap@QIcon@@QEBA?AVQPixmap@@HHW4Mode@1@W4State@1@@Z"));
  const auto to_image = reinterpret_cast<void*(__cdecl*)(const void*, void*)>(
    GetProcAddress(gui, "?toImage@QPixmap@@QEBA?AVQImage@@XZ"));
  const auto pixmap_destroy = reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(gui, "??1QPixmap@@UEAA@XZ"));
  const auto image_destroy = reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(gui, "??1QImage@@UEAA@XZ"));
  const auto image_equal = reinterpret_cast<bool(__cdecl*)(const void*, const void*)>(
    GetProcAddress(gui, "??8QImage@@QEBA_NAEBV0@@Z"));
  const auto image_null = reinterpret_cast<bool(__cdecl*)(const void*)>(GetProcAddress(gui, "?isNull@QImage@@QEBA_NXZ"));
  const auto clear_cache = reinterpret_cast<void(__cdecl*)()>(GetProcAddress(gui, "?clear@QPixmapCache@@SAXXZ"));
  const FARPROC button_get = GetProcAddress(widgets, "?icon@QAbstractButton@@QEBA?AVQIcon@@XZ");
  const FARPROC button_set = GetProcAddress(widgets, "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z");
  const FARPROC action_get = GetProcAddress(gui, "?icon@QAction@@QEBA?AVQIcon@@XZ");
  const FARPROC action_set = GetProcAddress(gui, "?setIcon@QAction@@QEAAXAEBVQIcon@@@Z");
  if (!icon_ctor || !icon_destroy || !pixmap || !to_image || !pixmap_destroy || !image_destroy ||
      !image_equal || !image_null || !clear_cache || !button_get || !button_set || !action_get || !action_set) {
    return patch_icon_theme_qresource_svg(lookup, alias, true);
  }
  alignas(16) unsigned char filename[64] = {};
  alignas(16) unsigned char original_icon[64] = {};
  reinterpret_cast<void(__cdecl*)(void*, const char*, long long)>(lookup.from_utf8_proc)(
    filename, alias, static_cast<long long>(std::strlen(alias)));
  icon_ctor(original_icon, filename);
  alignas(16) unsigned char reference_images[8][256] = {};
  size_t reference_index = 0;
  for (int extent : {16, 32}) {
    for (int mode : {0, 1}) {
      for (int state : {0, 1}) {
        alignas(16) unsigned char reference_pixmap[256] = {};
        pixmap(original_icon, reference_pixmap, extent, extent, mode, state);
        to_image(reference_pixmap, reference_images[reference_index++]);
        pixmap_destroy(reference_pixmap);
      }
    }
  }
  const auto matches_original = [&](const void* candidate) {
    size_t image_index = 0;
    for (int extent : {16, 32}) {
      for (int mode : {0, 1}) {
        for (int state : {0, 1}) {
          alignas(16) unsigned char candidate_pixmap[256] = {};
          alignas(16) unsigned char candidate_image[256] = {};
          pixmap(candidate, candidate_pixmap, extent, extent, mode, state);
          to_image(candidate_pixmap, candidate_image);
          const void* reference_image = reference_images[image_index++];
          const bool matches = !image_null(reference_image) && image_equal(reference_image, candidate_image);
          image_destroy(candidate_image);
          pixmap_destroy(candidate_pixmap);
          if (!matches) {
            return false;
          }
        }
      }
    }
    return true;
  };
  struct IconOwner { void* object; FARPROC setter; };
  std::vector<IconOwner> owners;
  size_t raster_closes = 0;
  const auto local_inventory = inventory ? std::vector<void*>{} : collect_modern_control_icon_objects(lookup);
  for (void* object : inventory ? *inventory : local_inventory) {
    if (!pointer_looks_like_cpp_object(object)) {
      continue;
    }
    const bool button = qt_object_inherits(lookup, object, "QAbstractButton");
    if (button && std::strcmp(alias, ":/dlg_tray_dialog_hide") == 0 && refresh_modern_tray_hide_icon(lookup, object)) {
      ++raster_closes;
    } else if (button || qt_object_inherits(lookup, object, "QAction")) {
      alignas(16) unsigned char candidate[64] = {};
      reinterpret_cast<void*(__cdecl*)(const void*, void*)>(button ? button_get : action_get)(object, candidate);
      if (matches_original(candidate)) {
        owners.push_back({object, button ? button_set : action_set});
      }
      icon_destroy(candidate);
    }
  }
  const size_t patched = patch_icon_theme_qresource_svg(lookup, alias, true);
  if (patched && !owners.empty()) {
    clear_cache();
    alignas(16) unsigned char replacement[64] = {};
    icon_ctor(replacement, filename);
    for (const IconOwner& owner : owners) {
      if (pointer_looks_like_cpp_object(owner.object)) {
        reinterpret_cast<void(__cdecl*)(void*, const void*)>(owner.setter)(owner.object, replacement);
      }
    }
    icon_destroy(replacement);
  }
  for (auto& image : reference_images) {
    image_destroy(image);
  }
  icon_destroy(original_icon);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qstring_destructor_proc)(filename);
  return patched + raster_closes;
}

size_t patch_modern_workspace_control_resources(const QtLookup& lookup) {
  if (modern_workspace_types().empty()) {
    return 0;
  }
  const auto inventory = collect_modern_control_icon_objects(lookup);
  size_t patched = 0;
  for (const char* alias : {":/scene_search.svg", ":/u_visible", ":/u_notvisible", ":/u_lock", ":/u_unlock",
       ":/dlg_info", ":/dlg_entity_panel_controls_tab.svg", ":/dlg_entity_panel_tags_tab.svg",
       ":/dlg_classification_mode", ":/dlg_classification_tags_mode", ":/dlg_classification_add_simple",
       ":/dlg_classification_tag_remove.svg", ":/dlg_agent_aimm_blue", ":/tb_copy", ":/options",
      ":/list_view.svg", ":/plus_circled", ":/dlg_tray_dialog_hide", ":/dlg_in_model",
      ":/dlg_update", ":/dlg_move_item_up", ":/dlg_move_item_down"}) {
    patched += patch_modern_workspace_control_resource(lookup, alias, &inventory);
  }
  return patched;
}

size_t patch_icon_theme_chevron_resources(const QtLookup& lookup) {
  const char* aliases[] = {
    ":/dlg_chevron_down",
    ":/dlg_chevron_right",
    ":/dlg_chevron_up",
    ":dlg_chevron_down",
    ":dlg_chevron_right",
    ":dlg_chevron_up"
  };
  size_t patched_count = 0;
  for (const char* alias : aliases) {
    patched_count += patch_icon_theme_qresource_svg(lookup, alias);
  }
  return patched_count;
}

constexpr char k_tray_hide_icon_resource_alias[] = ":/dlg_tray_dialog_hide";

void __fastcall tray_hide_icon_add_file_hook(void* icon, const void* file_name, const void* size,
                                             int mode, int state) {
  QIconAddFileFn original_proc = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_tray_hide_icon_mutex);
    original_proc = g_original_tray_hide_icon_add_file;
  }

  if (original_proc && icon_theme_enabled() && !g_tray_hide_icon_patching) {
    g_tray_hide_icon_patching = true;
    const QtLookup& lookup = resolve_qt_lookup();
    if (qstring_to_utf8(lookup, file_name) == k_tray_hide_icon_resource_alias) {
      const size_t patched_count = patch_icon_theme_qresource_svg(lookup, k_tray_hide_icon_resource_alias);
      if (patched_count > 0) {
        InterlockedExchange(&g_icon_theme_tray_hide_native_patch_count, static_cast<LONG>(patched_count));
      }
    }
    g_tray_hide_icon_patching = false;
  }

  if (original_proc) {
    original_proc(icon, file_name, size, mode, state);
  }
}

bool install_tray_hide_icon_add_file_hook() {
  std::lock_guard<std::mutex> lock(g_tray_hide_icon_mutex);
  if (g_original_tray_hide_icon_add_file && !g_tray_hide_icon_iat_slots.empty()) {
    return true;
  }

  const std::vector<void**> slots = find_main_import_iat_slots_for_symbols({
    "?addFile@QIcon@@QEAAXAEBVQString@@AEBVQSize@@W4Mode@1@W4State@1@@Z"
  });
  if (slots.empty() || !memory_readable(slots.front(), sizeof(*slots.front())) || !*slots.front()) {
    return false;
  }

  const QIconAddFileFn original_proc = reinterpret_cast<QIconAddFileFn>(*slots.front());
  std::vector<void**> installed_slots;
  for (void** slot : slots) {
    if (!slot || !memory_readable(slot, sizeof(*slot)) || !*slot) {
      continue;
    }
    DWORD original_protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
      continue;
    }
    *slot = reinterpret_cast<void*>(&tray_hide_icon_add_file_hook);
    DWORD unused_protection = 0;
    VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
    installed_slots.push_back(slot);
  }
  if (installed_slots.empty()) {
    return false;
  }

  g_original_tray_hide_icon_add_file = original_proc;
  g_tray_hide_icon_iat_slots = installed_slots;
  return true;
}

size_t rebuild_tray_hide_icons(const QtLookup& lookup) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_parent_widget_proc || !lookup.from_utf8_proc ||
      !lookup.widget_update_proc || !lookup.widget_repaint_proc || !g_original_tray_hide_icon_add_file) {
    return 0;
  }

  const size_t patched_count = patch_icon_theme_qresource_svg(lookup, k_tray_hide_icon_resource_alias);
  if (patched_count > 0) {
    InterlockedExchange(&g_icon_theme_tray_hide_native_patch_count, static_cast<LONG>(patched_count));
  }

  if (!modern_workspace_types().empty()) {
    InterlockedExchange(&g_icon_theme_tray_hide_native_rebuild_count, 0);
    return 0;
  }

  std::string icon_constructor_module;
  std::string icon_constructor_symbol;
  const FARPROC icon_constructor_proc = find_export(lookup.gui_modules, {
    "??0QIcon@@QEAA@XZ"
  }, icon_constructor_module, icon_constructor_symbol);
  std::string icon_setter_module;
  std::string icon_setter_symbol;
  const FARPROC icon_setter_proc = find_export(lookup.widgets_modules, {
    "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z"
  }, icon_setter_module, icon_setter_symbol);
  std::string icon_destructor_module;
  std::string icon_destructor_symbol;
  const FARPROC icon_destructor_proc = find_export(lookup.gui_modules, {
    "??1QIcon@@QEAA@XZ"
  }, icon_destructor_module, icon_destructor_symbol);
  if (!icon_constructor_proc || !icon_setter_proc || !icon_destructor_proc) {
    return 0;
  }

  using QtFromUtf8IntFn = void(__cdecl*)(void*, const char*, int);
  using QtFromUtf8LongLongFn = void(__cdecl*)(void*, const char*, long long);
  using QtIconConstructorFn = void(__cdecl*)(void*);
  using QtButtonSetIconFn = void(__cdecl*)(void*, const void*);
  using QtIconDestructorFn = void(__cdecl*)(void*);
  using QtParentWidgetFn = void*(__cdecl*)(const void*);
  using QtWidgetNoArgFn = void(__cdecl*)(void*);

  alignas(16) unsigned char qstring_storage[64] = {};
  const size_t alias_length = std::strlen(k_tray_hide_icon_resource_alias);
  if (lookup.from_utf8_symbol.find("_J") != std::string::npos) {
    reinterpret_cast<QtFromUtf8LongLongFn>(lookup.from_utf8_proc)(
      qstring_storage, k_tray_hide_icon_resource_alias, static_cast<long long>(alias_length));
  } else {
    reinterpret_cast<QtFromUtf8IntFn>(lookup.from_utf8_proc)(
      qstring_storage, k_tray_hide_icon_resource_alias, static_cast<int>(alias_length));
  }

  size_t rebuilt_count = 0;
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QToolButton")) {
      continue;
    }
    void* parent = reinterpret_cast<QtParentWidgetFn>(lookup.widget_parent_widget_proc)(widget);
    if (!pointer_looks_like_cpp_object(parent) || !qt_object_inherits(lookup, parent, "CPanelHeader")) {
      continue;
    }

    alignas(16) unsigned char icon_storage[64] = {};
    reinterpret_cast<QtIconConstructorFn>(icon_constructor_proc)(icon_storage);
    const int default_size[] = {-1, -1};
    g_original_tray_hide_icon_add_file(icon_storage, qstring_storage, default_size, 0, 1);
    reinterpret_cast<QtButtonSetIconFn>(icon_setter_proc)(widget, icon_storage);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    reinterpret_cast<QtIconDestructorFn>(icon_destructor_proc)(icon_storage);
    rebuilt_count += 1;
  }

  reinterpret_cast<QtIconDestructorFn>(lookup.qstring_destructor_proc)(qstring_storage);
  InterlockedExchange(&g_icon_theme_tray_hide_native_rebuild_count, static_cast<LONG>(rebuilt_count));
  return rebuilt_count;
}

size_t patch_all_icon_theme_svg_colors() {
  HMODULE executable = GetModuleHandleW(nullptr);
  const auto* image_base = reinterpret_cast<const unsigned char*>(executable);
  const auto* dos_header = image_base && memory_readable(image_base, sizeof(IMAGE_DOS_HEADER))
    ? reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base)
    : nullptr;
  const auto* nt_headers = dos_header && dos_header->e_magic == IMAGE_DOS_SIGNATURE && dos_header->e_lfanew > 0
    ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew)
    : nullptr;
  if (!nt_headers || !memory_readable(nt_headers, sizeof(*nt_headers)) || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return 0;
  }

  constexpr char svg_open[] = "<svg";
  constexpr char svg_close[] = "</svg>";
  const auto* const image_end = image_base + nt_headers->OptionalHeader.SizeOfImage;
  const auto* cursor = image_base;
  size_t patched_count = 0;
  while (cursor + std::size(svg_open) - 1 <= image_end) {
    const auto* svg_begin = std::search(cursor, image_end, svg_open, svg_open + std::size(svg_open) - 1);
    if (svg_begin == image_end) {
      break;
    }
    const auto* svg_close_begin = std::search(svg_begin, image_end, svg_close, svg_close + std::size(svg_close) - 1);
    if (svg_close_begin == image_end) {
      break;
    }
    const auto* const svg_end = svg_close_begin + std::size(svg_close) - 1;
    patched_count += patch_icon_theme_svg_colors(
      const_cast<unsigned char*>(svg_begin), static_cast<size_t>(svg_end - svg_begin));
    cursor = svg_end;
  }
  return patched_count;
}

size_t apply_icon_theme_resource_patches(const QtLookup& lookup) {
  constexpr bool direct_chevron_resource_patch_enabled = false;
  const size_t workspace_control_patch_count = patch_modern_workspace_control_resources(lookup);
  const size_t compiled_patch_count = patch_all_icon_theme_svg_colors();
  const size_t chevron_patch_count = direct_chevron_resource_patch_enabled
    ? patch_icon_theme_chevron_resources(lookup)
    : 0;
  const size_t default_env_patch_count = patch_default_environment_svg();
  const size_t patched_count = workspace_control_patch_count + compiled_patch_count + chevron_patch_count + default_env_patch_count;
  InterlockedExchange(&g_icon_theme_resource_color_count, static_cast<LONG>(patched_count));
  InterlockedExchange(&g_icon_theme_chevron_resource_color_count, static_cast<LONG>(chevron_patch_count));
  InterlockedExchange(&g_icon_theme_default_env_color_count, static_cast<LONG>(default_env_patch_count));
  if (patched_count == 0) {
    return 0;
  }

  std::string cache_module;
  std::string cache_symbol;
  const FARPROC pixmap_cache_clear_proc = find_export(lookup.gui_modules, {
    "?clear@QPixmapCache@@SAXXZ"
  }, cache_module, cache_symbol);
  if (pixmap_cache_clear_proc) {
    using QtNoArgFn = void(__cdecl*)();
    reinterpret_cast<QtNoArgFn>(pixmap_cache_clear_proc)();
  }
  static_cast<void>(refresh_toolbutton_icon_pixmaps(lookup));
  static_cast<void>(rebuild_tray_hide_icons(lookup));
  const bool page_tabs_search_icon_refreshed = refresh_page_tabs_search_icon(lookup);
  const size_t page_tabs_search_icon_count = page_tabs_search_icon_refreshed ? 1 : 0;
  if (!page_tabs_search_icon_refreshed) {
    write_page_tabs_pushbutton_inventory(lookup);
  }
  const size_t tray_chevron_pixmap_count = refresh_tray_chevron_pixmaps(lookup);
  InterlockedExchange(&g_icon_theme_tray_chevron_pixmap_count, static_cast<LONG>(tray_chevron_pixmap_count));
  InterlockedExchange(&g_icon_theme_component_navigator_search_icon_count, 0);
  InterlockedExchange(&g_icon_theme_page_tabs_search_icon_count, static_cast<LONG>(page_tabs_search_icon_count));

  for (const NativeWindowCandidate& window : process_top_level_windows()) {
    RedrawWindow(window.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  }
  return patched_count;
}

bool configure_icon_theme(bool enabled) {
  std::lock_guard<std::mutex> lock(g_icon_theme_mutex);
  if (!enabled) {
    InterlockedExchange(&g_icon_theme_enabled, 0);
    return true;
  }

  if (icon_theme_enabled()) {
    return true;
  }

  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&configure_icon_theme), &module)) {
    set_theme_info("Dark Mode SKP icon patch: extension module lookup failed.");
    return false;
  }
  wchar_t module_path[MAX_PATH] = {};
  if (GetModuleFileNameW(module, module_path, MAX_PATH) == 0) {
    set_theme_info("Dark Mode SKP icon patch: extension module path lookup failed.");
    return false;
  }
  g_icon_theme_inventory_directory = std::filesystem::path(module_path).parent_path();
  InterlockedExchange(&g_icon_theme_resource_color_count, 0);
  InterlockedExchange(&g_icon_theme_chevron_resource_color_count, 0);
  InterlockedExchange(&g_icon_theme_default_env_color_count, 0);
  InterlockedExchange(&g_icon_theme_tray_chevron_pixmap_count, 0);
  InterlockedExchange(&g_icon_theme_tray_chevron_widget_count, 0);
  InterlockedExchange(&g_icon_theme_tray_chevron_header_count, 0);
  InterlockedExchange(&g_icon_theme_tray_hide_native_patch_count, 0);
  InterlockedExchange(&g_icon_theme_tray_hide_native_rebuild_count, 0);
  InterlockedExchange(&g_icon_theme_component_navigator_search_icon_count, 0);
  InterlockedExchange(&g_icon_theme_page_tabs_search_icon_count, 0);

  const QtLookup& lookup = resolve_qt_lookup();
  const size_t resource_patch_count = apply_icon_theme_resource_patches(lookup);
  if (resource_patch_count == 0) {
    set_theme_info("Dark Mode SKP icon patch: stock SVG resources were unavailable.");
    return false;
  }

  InterlockedExchange(&g_icon_theme_enabled, 1);
  set_theme_info("Dark Mode SKP icon patch: enabled colors=" + std::to_string(resource_patch_count));
  return true;
}

std::vector<void**> find_function_indirect_call_slots(void* function_proc, const std::vector<void**>& target_slots) {
  std::vector<void**> slots;
  if (!function_proc || target_slots.empty()) {
    return slots;
  }
  function_proc = const_cast<void*>(resolve_native_function_body(function_proc));
  if (!function_proc) {
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
  static const auto rgba = reinterpret_cast<unsigned int(__cdecl*)(const void*)>(
    GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"), "?rgba@QColor@@QEBAIXZ"));
  for (size_t index = 0; index < std::size(kTagHeaderColorOffsets); ++index) {
    auto* color = reinterpret_cast<unsigned char*>(object) + kTagHeaderColorOffsets[index];
    if (!memory_readable(color, sizeof(active_backup->color_storage[index]))) {
      changed = false;
      break;
    }
    const unsigned int expected = (static_cast<unsigned int>(kTagHeaderDarkColors[index][3]) << 24) |
      (kTagHeaderDarkColors[index][0] << 16) | (kTagHeaderDarkColors[index][1] << 8) | kTagHeaderDarkColors[index][2];
    if (rgba && rgba(color) == expected) {
      continue;
    }
    if (!set_qcolor_rgba(lookup, color,
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
    clear_control_pixmaps();
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

using ContentBrowserDrawRectsFn = void(__cdecl*)(void*, const void*, int);

struct ContentBrowserFolderOutlineApi {
  FARPROC draw_rects = nullptr;
  FARPROC painter_pen = nullptr;
  FARPROC painter_set_pen = nullptr;
  FARPROC pen_copy = nullptr;
  FARPROC pen_color = nullptr;
  FARPROC pen_set_color = nullptr;
  FARPROC pen_destructor = nullptr;
};

ContentBrowserFolderOutlineApi g_content_browser_folder_outline_api;

bool resolve_content_browser_folder_outline_api(const QtLookup& lookup) {
  HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
  if (!gui || !lookup.qpainter_save_proc || !lookup.qpainter_restore_proc ||
      !lookup.qcolor_rgb_ctor_proc || !lookup.qcolor_rgb_proc) {
    return false;
  }
  auto& api = g_content_browser_folder_outline_api;
  if (!api.draw_rects) {
    api.draw_rects = GetProcAddress(gui, "?drawRects@QPainter@@QEAAXPEBVQRect@@H@Z");
    api.painter_pen = GetProcAddress(gui, "?pen@QPainter@@QEBAAEBVQPen@@XZ");
    api.painter_set_pen = GetProcAddress(gui, "?setPen@QPainter@@QEAAXAEBVQPen@@@Z");
    api.pen_copy = GetProcAddress(gui, "??0QPen@@QEAA@AEBV0@@Z");
    api.pen_color = GetProcAddress(gui, "?color@QPen@@QEBA?AVQColor@@XZ");
    api.pen_set_color = GetProcAddress(gui, "?setColor@QPen@@QEAAXAEBVQColor@@@Z");
    api.pen_destructor = GetProcAddress(gui, "??1QPen@@QEAA@XZ");
  }
  return api.draw_rects && api.painter_pen && api.painter_set_pen && api.pen_copy &&
    api.pen_color && api.pen_set_color && api.pen_destructor;
}

struct NativeImageView {
  const unsigned char* base = nullptr;
  const IMAGE_NT_HEADERS64* headers = nullptr;
  size_t size = 0;

  explicit NativeImageView(HMODULE module) {
    base = reinterpret_cast<const unsigned char*>(module);
    if (!memory_readable(base, sizeof(IMAGE_DOS_HEADER))) {
      return;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
      return;
    }
    const auto* candidate = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (!memory_readable(candidate, sizeof(*candidate)) || candidate->Signature != IMAGE_NT_SIGNATURE ||
        candidate->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        candidate->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
      return;
    }
    headers = candidate;
    size = headers->OptionalHeader.SizeOfImage;
  }

  bool contains(size_t rva, size_t length) const {
    return headers && rva < size && length <= size - rva && memory_readable(base + rva, length);
  }

  void** import_slot(const char* name) const {
    if (!headers) {
      return nullptr;
    }
    const auto directory = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!contains(directory.VirtualAddress, directory.Size)) {
      return nullptr;
    }
    void** result = nullptr;
    const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (size_t index = 0; index < directory.Size / sizeof(*descriptors) && descriptors[index].Name; ++index) {
      const auto& descriptor = descriptors[index];
      if (!descriptor.OriginalFirstThunk || !descriptor.FirstThunk) {
        continue;
      }
      for (size_t offset = 0; contains(descriptor.OriginalFirstThunk + offset, sizeof(IMAGE_THUNK_DATA64));
           offset += sizeof(IMAGE_THUNK_DATA64)) {
        const auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + descriptor.OriginalFirstThunk + offset)->u1.AddressOfData;
        if (!thunk) {
          break;
        }
        if (IMAGE_SNAP_BY_ORDINAL64(thunk) || !contains(thunk, sizeof(WORD) + std::strlen(name) + 1)) {
          continue;
        }
        const char* symbol = reinterpret_cast<const char*>(base + thunk + sizeof(WORD));
        if (std::memcmp(symbol, name, std::strlen(name) + 1) == 0) {
          if (result || !contains(descriptor.FirstThunk + offset, sizeof(void*))) {
            return nullptr;
          }
          result = reinterpret_cast<void**>(const_cast<unsigned char*>(base) + descriptor.FirstThunk + offset);
        }
      }
    }
    return result;
  }

  size_t function_size(const void* function) const {
    if (!headers || reinterpret_cast<std::uintptr_t>(function) < reinterpret_cast<std::uintptr_t>(base)) {
      return 0;
    }
    const size_t rva = reinterpret_cast<std::uintptr_t>(function) - reinterpret_cast<std::uintptr_t>(base);
    const auto directory = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (!contains(rva, 1) || !contains(directory.VirtualAddress, directory.Size)) {
      return 0;
    }
    const auto* entries = reinterpret_cast<const RUNTIME_FUNCTION*>(base + directory.VirtualAddress);
    const size_t count = directory.Size / sizeof(*entries);
    const auto* found = std::lower_bound(entries, entries + count, rva,
      [](const RUNTIME_FUNCTION& entry, size_t address) { return entry.BeginAddress < address; });
    if (found == entries + count || found->BeginAddress != rva || found->EndAddress <= rva ||
        found->EndAddress - rva > 65536 || !contains(rva, found->EndAddress - rva)) {
      return 0;
    }
    return found->EndAddress - rva;
  }

  const void* function_body(const void* function) const {
    for (size_t depth = 0; headers && depth < 8; ++depth) {
      const auto address = reinterpret_cast<std::uintptr_t>(function);
      const auto image_address = reinterpret_cast<std::uintptr_t>(base);
      if (address < image_address || !contains(address - image_address, 1)) {
        return nullptr;
      }
      const size_t rva = address - image_address;
      const auto* sections = IMAGE_FIRST_SECTION(headers);
      bool executable = false;
      for (WORD index = 0; index < headers->FileHeader.NumberOfSections; ++index) {
        const auto& section = sections[index];
        if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) && rva >= section.VirtualAddress &&
            rva - section.VirtualAddress < section.Misc.VirtualSize) {
          executable = true;
          break;
        }
      }
      if (!executable) {
        return nullptr;
      }
      if (base[rva] != 0xe9) {
        return function_size(function) ? function : nullptr;
      }
      if (!contains(rva, 5)) {
        return nullptr;
      }
      std::int32_t displacement = 0;
      std::memcpy(&displacement, base + rva + 1, sizeof(displacement));
      const auto target = static_cast<std::int64_t>(rva) + 5 + displacement;
      if (target < 0 || static_cast<std::uint64_t>(target) >= size) {
        return nullptr;
      }
      function = base + target;
    }
    return nullptr;
  }
};

const void* resolve_native_function_body(const void* function) {
  HMODULE module = nullptr;
  if (!function || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(function), &module)) {
    return nullptr;
  }
  return NativeImageView(module).function_body(function);
}

int native_register_index(unsigned int reg) {
  switch (reg) {
    case X86_REG_RAX: case X86_REG_EAX: case X86_REG_AX: case X86_REG_AL: case X86_REG_AH: return 0;
    case X86_REG_RCX: case X86_REG_ECX: case X86_REG_CX: case X86_REG_CL: case X86_REG_CH: return 1;
    case X86_REG_RDX: case X86_REG_EDX: case X86_REG_DX: case X86_REG_DL: case X86_REG_DH: return 2;
    case X86_REG_RBX: case X86_REG_EBX: case X86_REG_BX: case X86_REG_BL: case X86_REG_BH: return 3;
    case X86_REG_RSP: case X86_REG_ESP: case X86_REG_SP: case X86_REG_SPL: return 4;
    case X86_REG_RBP: case X86_REG_EBP: case X86_REG_BP: case X86_REG_BPL: return 5;
    case X86_REG_RSI: case X86_REG_ESI: case X86_REG_SI: case X86_REG_SIL: return 6;
    case X86_REG_RDI: case X86_REG_EDI: case X86_REG_DI: case X86_REG_DIL: return 7;
    case X86_REG_R8: case X86_REG_R8D: case X86_REG_R8W: case X86_REG_R8B: return 8;
    case X86_REG_R9: case X86_REG_R9D: case X86_REG_R9W: case X86_REG_R9B: return 9;
    case X86_REG_R10: case X86_REG_R10D: case X86_REG_R10W: case X86_REG_R10B: return 10;
    case X86_REG_R11: case X86_REG_R11D: case X86_REG_R11W: case X86_REG_R11B: return 11;
    case X86_REG_R12: case X86_REG_R12D: case X86_REG_R12W: case X86_REG_R12B: return 12;
    case X86_REG_R13: case X86_REG_R13D: case X86_REG_R13W: case X86_REG_R13B: return 13;
    case X86_REG_R14: case X86_REG_R14D: case X86_REG_R14W: case X86_REG_R14B: return 14;
    case X86_REG_R15: case X86_REG_R15D: case X86_REG_R15W: case X86_REG_R15B: return 15;
    default: return -1;
  }
}

struct DiscoveredFolderColor {
  size_t return_rva = 0;
  int native_shade = 0;
  size_t outline_return_rva = 0;
};

struct NativeFunctionCalls {
  bool complete = false;
  std::vector<const void*> direct;
  std::vector<void**> imports;
  std::vector<size_t> import_returns;
};

NativeFunctionCalls decode_native_function_calls(const NativeImageView& image, const void* function, csh decoder) {
  NativeFunctionCalls result;
  function = image.function_body(function);
  const size_t length = image.function_size(function);
  if (!length) {
    return result;
  }
  cs_insn* instructions = nullptr;
  const size_t count = cs_disasm(decoder, static_cast<const uint8_t*>(function), length,
    reinterpret_cast<uint64_t>(function), 0, &instructions);
  for (size_t index = 0; index < count; ++index) {
    const auto& instruction = instructions[index];
    if (instruction.id != X86_INS_CALL || instruction.detail->x86.op_count != 1) {
      continue;
    }
    const auto& target = instruction.detail->x86.operands[0];
    if (target.type == X86_OP_IMM) {
      if (const void* body = image.function_body(reinterpret_cast<const void*>(target.imm))) {
        result.direct.push_back(body);
      }
    } else if (target.type == X86_OP_MEM && target.mem.base == X86_REG_RIP &&
               target.mem.index == X86_REG_INVALID) {
      const auto address = instruction.address + instruction.size + target.mem.disp;
      if (address >= reinterpret_cast<uint64_t>(image.base) &&
          image.contains(address - reinterpret_cast<uint64_t>(image.base), sizeof(void*))) {
        result.imports.push_back(reinterpret_cast<void**>(address));
        result.import_returns.push_back(instruction.address + instruction.size - reinterpret_cast<uint64_t>(image.base));
      }
    }
  }
  result.complete = count && instructions[count - 1].address + instructions[count - 1].size ==
    reinterpret_cast<uint64_t>(function) + length;
  cs_free(instructions, count);
  return result;
}

struct ModernWorkspaceFrameCalls {
  void** slot = nullptr;
  std::vector<std::uintptr_t> returns;
};

ModernWorkspaceFrameCalls discover_modern_workspace_frame_calls(HMODULE module) {
  const NativeImageView image(module);
  void** slot = image.import_slot("??0QPen@@QEAA@AEBVQColor@@@Z");
  if (!slot) {
    return {};
  }
  csh decoder = 0;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK) {
    return {};
  }
  cs_option(decoder, CS_OPT_DETAIL, CS_OPT_ON);
  ModernWorkspaceFrameCalls result{slot, {}};
  for (const char* name : {".?AVGroup@QtWidgets@KDDockWidgets@@", ".?AVFloatingWindow@QtWidgets@KDDockWidgets@@"}) {
    const auto* table = static_cast<const void* const*>(primary_rtti_vtable(find_vtables_for_type(name, module)));
    if (!memory_readable(table, 28 * sizeof(void*))) {
      result = {};
      break;
    }
    const auto calls = decode_native_function_calls(image, table[27], decoder);
    if (!calls.complete || std::count(calls.imports.begin(), calls.imports.end(), slot) != 1) {
      result = {};
      break;
    }
    const auto index = std::find(calls.imports.begin(), calls.imports.end(), slot) - calls.imports.begin();
    result.returns.push_back(reinterpret_cast<std::uintptr_t>(image.base) + calls.import_returns[index]);
  }
  cs_close(&decoder);
  return result;
}

VcbStatusPaintHook g_modern_workspace_frame_hook;
std::vector<std::uintptr_t> g_modern_workspace_frame_returns;
LONG g_modern_workspace_frame_calls = 0;
LONG g_modern_workspace_frame_tints = 0;

void construct_modern_workspace_frame_pen(void* pen, const void* color, std::uintptr_t caller) {
  const auto original = reinterpret_cast<void(__cdecl*)(void*, const void*)>(g_modern_workspace_frame_hook.original_proc);
  if (!original) {
    return;
  }
  InterlockedIncrement(&g_modern_workspace_frame_calls);
  alignas(16) unsigned char replacement[64] = {};
  if (g_modern_workspace_dark && color && g_modern_workspace_paint_api.rgba &&
      std::find(g_modern_workspace_frame_returns.begin(), g_modern_workspace_frame_returns.end(), caller) !=
        g_modern_workspace_frame_returns.end()) {
    const unsigned int rgba = g_modern_workspace_paint_api.rgba(color);
    const unsigned int shade = (rgba >> 16) & 255;
    if ((shade >= 128 || (rgba & 0xffffff) == 0x666666) &&
      ((rgba >> 8) & 255) == shade && (rgba & 255) == shade) {
      reinterpret_cast<QtColorRgbCtorFn>(resolve_qt_lookup().qcolor_rgb_ctor_proc)(replacement, 58, 58, 58, rgba >> 24);
      color = replacement;
      InterlockedIncrement(&g_modern_workspace_frame_tints);
    }
  }
  original(pen, color);
}

void __cdecl modern_workspace_frame_pen_hook(void* pen, const void* color) {
  construct_modern_workspace_frame_pen(pen, color, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
}

bool set_modern_workspace_frame_hook(bool enabled, HMODULE module) {
  auto& hook = g_modern_workspace_frame_hook;
  if (!enabled) {
    if (hook.slot && *hook.slot == reinterpret_cast<void*>(&modern_workspace_frame_pen_hook)) {
      DWORD protection = 0;
      if (!VirtualProtect(hook.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
        return false;
      }
      *hook.slot = hook.original_proc;
      DWORD unused = 0;
      VirtualProtect(hook.slot, sizeof(void*), protection, &unused);
    }
    hook = {};
    g_modern_workspace_frame_returns.clear();
    return true;
  }
  if (hook.slot) {
    return *hook.slot == reinterpret_cast<void*>(&modern_workspace_frame_pen_hook);
  }
  const auto calls = discover_modern_workspace_frame_calls(module ? module : GetModuleHandleW(L"kddockwidgets-qt6.dll"));
  if (!calls.slot || calls.returns.size() != 2 || !*calls.slot) {
    return false;
  }
  DWORD protection = 0;
  if (!VirtualProtect(calls.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
    return false;
  }
  hook = {calls.slot, *calls.slot};
  g_modern_workspace_frame_returns = calls.returns;
  InterlockedExchange(&g_modern_workspace_frame_calls, 0);
  InterlockedExchange(&g_modern_workspace_frame_tints, 0);
  *calls.slot = reinterpret_cast<void*>(&modern_workspace_frame_pen_hook);
  DWORD unused = 0;
  VirtualProtect(calls.slot, sizeof(void*), protection, &unused);
  return true;
}

struct ContentBrowserImageFunctions {
  const void* generator = nullptr;
  const void* renderer = nullptr;
  void** fill_slot = nullptr;
  size_t fill_return_rva = 0;
};

struct TagEyePixmapCall {
  void** slot = nullptr;
  size_t return_rva = 0;
  bool cached = false;
};

bool create_legacy_about_image(const QtLookup& lookup, const void* source, void* result) {
  const HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
  const auto width_proc = reinterpret_cast<int(__cdecl*)(const void*)>(GetProcAddress(gui, "?width@QImage@@QEBAHXZ"));
  const auto height_proc = reinterpret_cast<int(__cdecl*)(const void*)>(GetProcAddress(gui, "?height@QImage@@QEBAHXZ"));
  const auto format_proc = reinterpret_cast<int(__cdecl*)(const void*)>(GetProcAddress(gui, "?format@QImage@@QEBA?AW4Format@1@XZ"));
  const auto pixel_proc = reinterpret_cast<unsigned int(__cdecl*)(const void*, int, int)>(GetProcAddress(gui, "?pixel@QImage@@QEBAIHH@Z"));
  const auto set_pixel = reinterpret_cast<void(__cdecl*)(void*, int, int, unsigned int)>(GetProcAddress(gui, "?setPixel@QImage@@QEAAXHHI@Z"));
  const auto copy_proc = reinterpret_cast<void(__cdecl*)(void*, const void*)>(GetProcAddress(gui, "??0QImage@@QEAA@AEBV0@@Z"));
  if (!source || !result || !width_proc || !height_proc || !format_proc || !pixel_proc || !set_pixel || !copy_proc ||
      !resolve_toolbar_title_paint_api(lookup)) {
    return false;
  }
  const int width = width_proc(source);
  const int height = height_proc(source);
  const int format = format_proc(source);
  if (width <= 0 || height <= 0 || width > 2048 || height > 2048 ||
      (format != 4 && format != 5 && format != 6 && format != 13 && format != 29)) {
    return false;
  }
  for (const auto& corner : {std::pair<int, int>{0, 0}, {width - 1, 0}, {0, height - 1}, {width - 1, height - 1}}) {
    const unsigned int pixel = pixel_proc(source, corner.first, corner.second);
    const int red = (pixel >> 16) & 255;
    const int green = (pixel >> 8) & 255;
    const int blue = pixel & 255;
    if ((pixel >> 24) != 255 || std::min({red, green, blue}) < 230 ||
        std::max({red, green, blue}) - std::min({red, green, blue}) > 24) {
      return false;
    }
  }
  copy_proc(result, source);
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const unsigned int pixel = pixel_proc(source, column, row);
      const int red = (pixel >> 16) & 255;
      const int green = (pixel >> 8) & 255;
      const int blue = pixel & 255;
      const int low = std::min({red, green, blue});
      const int high = std::max({red, green, blue});
      if ((pixel >> 24) == 255 && high - low <= 24) {
        const int shade = 43 + std::max(0, 230 - high) * 173 / 230;
        set_pixel(result, column, row, 0xff000000u | (shade << 16) | (shade << 8) | shade);
      }
    }
  }
  return true;
}

struct AboutImageCall {
  void** slot = nullptr;
  size_t return_rva = 0;
};

AboutImageCall discover_about_image_call(HMODULE module) {
  const NativeImageView image(module);
  const void* table = primary_rtti_vtable(find_vtables_for_type(".?AVAboutSketchUp@@", module));
  void** draw_slot = image.import_slot("?drawImage@QPainter@@QEAAXAEBVQRect@@AEBVQImage@@@Z");
  if (!draw_slot || !memory_readable(table, 28 * sizeof(void*))) {
    return {};
  }
  csh decoder = 0;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK) {
    return {};
  }
  cs_option(decoder, CS_OPT_DETAIL, CS_OPT_ON);
  const auto calls = decode_native_function_calls(image, static_cast<const void* const*>(table)[27], decoder);
  cs_close(&decoder);
  if (!calls.complete || std::count(calls.imports.begin(), calls.imports.end(), draw_slot) != 1) {
    return {};
  }
  const auto index = std::find(calls.imports.begin(), calls.imports.end(), draw_slot) - calls.imports.begin();
  return {draw_slot, calls.import_returns[index]};
}

VcbStatusPaintHook g_about_image_hook;
std::uintptr_t g_about_image_return = 0;
long long g_about_image_cache_key = 0;
alignas(16) unsigned char g_about_image_cache[256] = {};
bool g_about_image_cache_constructed = false;

void clear_about_image_cache() {
  if (g_about_image_cache_constructed) {
    reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(g_about_image_cache);
    g_about_image_cache_constructed = false;
  }
  g_about_image_cache_key = 0;
}

bool draw_legacy_material_image(void* painter, const void* rect, const void* image,
  std::uintptr_t caller, ContentBrowserDrawPixmapFn original);

void __cdecl about_draw_image_hook(void* painter, const void* rect, const void* image) {
  const auto original = reinterpret_cast<ContentBrowserDrawPixmapFn>(g_about_image_hook.original_proc);
  if (!original) {
    return;
  }
  if (draw_legacy_material_image(painter, rect, image, reinterpret_cast<std::uintptr_t>(_ReturnAddress()), original)) {
    return;
  }
  if (g_about_image_return && reinterpret_cast<std::uintptr_t>(_ReturnAddress()) == g_about_image_return) {
    const auto cache_key = reinterpret_cast<long long(__cdecl*)(const void*)>(
      GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"), "?cacheKey@QImage@@QEBA_JXZ"));
    const long long key = cache_key ? cache_key(image) : 0;
    if (key && key != g_about_image_cache_key) {
      clear_about_image_cache();
      g_about_image_cache_constructed = create_legacy_about_image(resolve_qt_lookup(), image, g_about_image_cache);
      g_about_image_cache_key = key;
    }
    if (key && g_about_image_cache_constructed) {
      original(painter, rect, g_about_image_cache);
      return;
    }
  }
  original(painter, rect, image);
}

bool set_about_image_hook(const QtLookup& lookup, bool dark) {
  if (!dark) {
    g_about_image_return = 0;
    clear_about_image_cache();
    if (!g_about_image_hook.slot) {
      return true;
    }
    DWORD protection = 0;
    if (!VirtualProtect(g_about_image_hook.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      return false;
    }
    if (*g_about_image_hook.slot == reinterpret_cast<void*>(&about_draw_image_hook)) {
      *g_about_image_hook.slot = g_about_image_hook.original_proc;
    }
    DWORD unused = 0;
    VirtualProtect(g_about_image_hook.slot, sizeof(void*), protection, &unused);
    g_about_image_hook = {};
    return true;
  }
  if (g_about_image_hook.slot) {
    return *g_about_image_hook.slot == reinterpret_cast<void*>(&about_draw_image_hook);
  }
  const HMODULE module = GetModuleHandleW(nullptr);
  const auto call = discover_about_image_call(module);
  if (!call.slot) {
    return true;
  }
  const auto original = GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"),
    "?drawImage@QPainter@@QEAAXAEBVQRect@@AEBVQImage@@@Z");
  if (!original || *call.slot != reinterpret_cast<void*>(original) || !resolve_toolbar_title_paint_api(lookup)) {
    return false;
  }
  DWORD protection = 0;
  if (!VirtualProtect(call.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
    return false;
  }
  g_about_image_hook = {call.slot, *call.slot};
  g_about_image_return = reinterpret_cast<std::uintptr_t>(module) + call.return_rva;
  *call.slot = reinterpret_cast<void*>(&about_draw_image_hook);
  DWORD unused = 0;
  VirtualProtect(call.slot, sizeof(void*), protection, &unused);
  return true;
}

TagEyePixmapCall discover_tag_eye_pixmap_call(HMODULE module) {
  const NativeImageView image(module);
  const void* table = primary_rtti_vtable(find_vtables_for_type(".?AVTagDialogShowHideDelegate@@", module));
  void** pixmap_slot = image.import_slot("?drawPixmap@QPainter@@QEAAXAEBVQPoint@@AEBVQPixmap@@@Z");
  if (!memory_readable(table, 12 * sizeof(void*)) || !pixmap_slot) {
    return {};
  }
  csh decoder = 0;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK) {
    return {};
  }
  cs_option(decoder, CS_OPT_DETAIL, CS_OPT_ON);
  const auto calls = decode_native_function_calls(image, static_cast<const void* const*>(table)[11], decoder);
  cs_close(&decoder);
  if (!calls.complete || std::count(calls.imports.begin(), calls.imports.end(), pixmap_slot) != 1) {
    return {};
  }
  const auto index = std::find(calls.imports.begin(), calls.imports.end(), pixmap_slot) - calls.imports.begin();
  void** icon_slot = image.import_slot("?pixmap@QIcon@@QEBA?AVQPixmap@@AEBVQSize@@NW4Mode@1@W4State@1@@Z");
  return {pixmap_slot, calls.import_returns[index],
    std::find(calls.imports.begin(), calls.imports.end(), icon_slot) == calls.imports.end()};
}

VcbStatusPaintHook g_tag_eye_pixmap_hook;
std::uintptr_t g_tag_eye_pixmap_return = 0;

void __cdecl tag_eye_draw_pixmap_hook(void* painter, const void* point, const void* pixmap) {
  const auto original = reinterpret_cast<ContentBrowserDrawPixmapFn>(g_tag_eye_pixmap_hook.original_proc);
  if (!original) {
    return;
  }
  if (g_tag_eye_pixmap_return && reinterpret_cast<std::uintptr_t>(_ReturnAddress()) == g_tag_eye_pixmap_return) {
    alignas(16) unsigned char tinted[256] = {};
    if (create_content_browser_search_pixmap(resolve_qt_lookup(), pixmap, tinted, true)) {
      original(painter, point, tinted);
      reinterpret_cast<void(__cdecl*)(void*)>(g_scene_search_icon_api.pixmap_destructor)(tinted);
      return;
    }
  }
  original(painter, point, pixmap);
}

bool set_tag_eye_icon_hook(const QtLookup& lookup, bool dark) {
  if (!dark) {
    g_tag_eye_pixmap_return = 0;
    if (!g_tag_eye_pixmap_hook.slot) {
      return true;
    }
    DWORD protection = 0;
    if (!VirtualProtect(g_tag_eye_pixmap_hook.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      return false;
    }
    if (*g_tag_eye_pixmap_hook.slot == reinterpret_cast<void*>(&tag_eye_draw_pixmap_hook)) {
      *g_tag_eye_pixmap_hook.slot = g_tag_eye_pixmap_hook.original_proc;
    }
    DWORD unused = 0;
    VirtualProtect(g_tag_eye_pixmap_hook.slot, sizeof(void*), protection, &unused);
    g_tag_eye_pixmap_hook = {};
    return true;
  }
  if (g_tag_eye_pixmap_hook.slot) {
    return *g_tag_eye_pixmap_hook.slot == reinterpret_cast<void*>(&tag_eye_draw_pixmap_hook);
  }
  const HMODULE module = GetModuleHandleW(nullptr);
  const auto call = discover_tag_eye_pixmap_call(module);
  if (!call.slot) {
    return false;
  }
  if (!call.cached) {
    return true;
  }
  const auto original = GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"),
    "?drawPixmap@QPainter@@QEAAXAEBVQPoint@@AEBVQPixmap@@@Z");
  if (!original || *call.slot != reinterpret_cast<void*>(original) ||
      !resolve_toolbar_title_paint_api(lookup) || !resolve_scene_search_icon_api(lookup)) {
    return false;
  }
  DWORD protection = 0;
  if (!VirtualProtect(call.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
    return false;
  }
  g_tag_eye_pixmap_hook = {call.slot, *call.slot};
  g_tag_eye_pixmap_return = reinterpret_cast<std::uintptr_t>(module) + call.return_rva;
  *call.slot = reinterpret_cast<void*>(&tag_eye_draw_pixmap_hook);
  DWORD unused = 0;
  VirtualProtect(call.slot, sizeof(void*), protection, &unused);
  return true;
}

ContentBrowserImageFunctions discover_content_browser_image_functions(HMODULE module) {
  const NativeImageView image(module);
  const void* table = primary_rtti_vtable(find_vtables_for_type(".?AVContentBrowserListCtrl@@", module));
  const std::vector<void**> generator_imports = {
    image.import_slot("??0QImage@@QEAA@XZ"),
    image.import_slot("??0QImage@@QEAA@AEBV0@@Z"),
    image.import_slot("?setDevicePixelRatio@QImage@@QEAAXN@Z"),
    image.import_slot("??0QColor@@QEAA@W4GlobalColor@Qt@@@Z")
  };
  const std::vector<void**> renderer_imports = {
    image.import_slot("?fill@QImage@@QEAAXAEBVQColor@@@Z"),
    image.import_slot("??0QSvgRenderer@@QEAA@AEBVQString@@PEAVQObject@@@Z"),
    image.import_slot("?render@QSvgRenderer@@QEAAXPEAVQPainter@@@Z")
  };
  if (!memory_readable(table, 28 * sizeof(void*)) ||
      std::find(generator_imports.begin(), generator_imports.end(), nullptr) != generator_imports.end() ||
      std::find(renderer_imports.begin(), renderer_imports.end(), nullptr) != renderer_imports.end()) {
    return {};
  }
  csh decoder = 0;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK) {
    return {};
  }
  cs_option(decoder, CS_OPT_DETAIL, CS_OPT_ON);
  auto find_matches = [&image, decoder](const void* root, unsigned int depth_limit,
                                        const std::vector<void**>& required) {
    std::vector<const void*> matches;
    std::vector<std::pair<const void*, unsigned int>> pending = {{root, 0}};
    std::unordered_set<const void*> visited;
    for (size_t index = 0; index < pending.size(); ++index) {
      const auto current = pending[index];
      if (!visited.insert(current.first).second) {
        continue;
      }
      if (visited.size() > 256) {
        return std::vector<const void*>{};
      }
      const auto calls = decode_native_function_calls(image, current.first, decoder);
      if (!calls.complete) {
        return std::vector<const void*>{};
      }
      if (std::all_of(required.begin(), required.end(), [&calls](void** slot) {
            return std::find(calls.imports.begin(), calls.imports.end(), slot) != calls.imports.end();
          })) {
        matches.push_back(current.first);
      }
      if (current.second < depth_limit) {
        for (const void* target : calls.direct) {
          pending.push_back({target, current.second + 1});
        }
      }
    }
    return matches;
  };
  ContentBrowserImageFunctions result;
  const auto generators = find_matches(static_cast<const void* const*>(table)[27], 2, generator_imports);
  if (generators.size() == 1) {
    const auto renderers = find_matches(generators.front(), 4, renderer_imports);
    if (renderers.size() == 1) {
      const auto calls = decode_native_function_calls(image, renderers.front(), decoder);
      if (std::count(calls.imports.begin(), calls.imports.end(), renderer_imports.front()) == 1) {
        const auto index = std::find(calls.imports.begin(), calls.imports.end(), renderer_imports.front()) - calls.imports.begin();
        result = {generators.front(), renderers.front(), renderer_imports.front(), calls.import_returns[index]};
      }
    }
  }
  cs_close(&decoder);
  return result;
}

struct LegacyMaterialLayout {
  void** row_paint_slot = nullptr;
  void** image_slot = nullptr;
  size_t image_return_rva = 0;
  const void* data_vtable = nullptr;
  size_t row_data_offset = 0;
  size_t flag_offset = 0;
  size_t pointer_offset = 0;
  size_t path_size_offset = 0;
};

LegacyMaterialLayout discover_legacy_material_layout(HMODULE module) {
  const NativeImageView image(module);
  const void* table = primary_rtti_vtable(find_vtables_for_type(".?AVCMaterialListCtrl@@", module));
  const void* data_table = primary_rtti_vtable(find_vtables_for_type(".?AVCBrowserData@@", module));
  void** draw_slot = image.import_slot("?drawImage@QPainter@@QEAAXAEBVQRect@@AEBVQImage@@@Z");
  if (!draw_slot || !data_table || !memory_readable(table, 54 * sizeof(void*))) {
    return {};
  }
  void** row_slot = const_cast<void**>(static_cast<void* const*>(table)) + 53;
  if (!image.function_size(*row_slot)) {
    return {};
  }
  const auto images = discover_content_browser_image_functions(module);
  if (!images.generator) {
    return {};
  }
  csh decoder = 0;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK) {
    return {};
  }
  cs_option(decoder, CS_OPT_DETAIL, CS_OPT_ON);
  const auto row_calls = decode_native_function_calls(image, *row_slot, decoder);
  LegacyMaterialLayout result;
  if (row_calls.complete && std::count(row_calls.imports.begin(), row_calls.imports.end(), draw_slot) == 1) {
    const auto index = std::find(row_calls.imports.begin(), row_calls.imports.end(), draw_slot) - row_calls.imports.begin();
    result = {row_slot, draw_slot, row_calls.import_returns[index], data_table};
  }
  auto matches = [&image](const unsigned char* code, std::initializer_list<int> pattern) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    const auto base = reinterpret_cast<std::uintptr_t>(image.base);
    if (address < base || !image.contains(address - base, pattern.size())) {
      return false;
    }
    size_t index = 0;
    for (int value : pattern) {
      if (value >= 0 && code[index] != value) {
        return false;
      }
      ++index;
    }
    return true;
  };
  size_t row_accessors = 0;
  size_t pointer_predicates = 0;
  size_t path_predicates = 0;
  size_t path_flag = 0;
  for (const void* function : {static_cast<const void*>(*row_slot), images.generator}) {
    cs_insn* instructions = nullptr;
    const size_t count = cs_disasm(decoder, static_cast<const uint8_t*>(function), image.function_size(function),
      reinterpret_cast<uint64_t>(function), 0, &instructions);
    for (size_t index = 0; index < count; ++index) {
      const auto& instruction = instructions[index];
      if (instruction.id != X86_INS_CALL || instruction.detail->x86.op_count != 1 ||
          instruction.detail->x86.operands[0].type != X86_OP_IMM) {
        continue;
      }
      const auto* target = reinterpret_cast<const unsigned char*>(instruction.detail->x86.operands[0].imm);
      if (function == *row_slot) {
        if (matches(target, {0x48, 0x8b, 0x41, -1, 0xc3})) {
          result.row_data_offset = target[3];
          ++row_accessors;
        }
        continue;
      }
      if (matches(target, {0x80, 0xb9, -1, -1, -1, -1, 0, 0x75, 0x0d,
          0x48, 0x83, 0xb9, -1, -1, -1, -1, 0, 0x74, 3, 0xb0, 1, 0xc3, 0x32, 0xc0, 0xc3})) {
        result.flag_offset = read_unaligned_u32(target + 2);
        result.pointer_offset = read_unaligned_u32(target + 12);
        ++pointer_predicates;
      }
      if (matches(target, {0x48, 0x83, 0xec, 0x28, 0x80, 0xb9, -1, -1, -1, -1, 0,
          0x75, 0x17, 0x48, 0x81, 0xc1, -1, -1, -1, -1, 0xe8, -1, -1, -1, -1,
          0x84, 0xc0, 0x75, 7, 0xb0, 1, 0x48, 0x83, 0xc4, 0x28, 0xc3, 0x32, 0xc0,
          0x48, 0x83, 0xc4, 0x28, 0xc3})) {
        const auto displacement = static_cast<std::int32_t>(read_unaligned_u32(target + 21));
        const auto* empty_check = target + 25 + displacement;
        if (matches(empty_check, {0x48, 0x83, 0x79, -1, 0, 0x0f, 0x94, 0xc0, 0xc3})) {
          path_flag = read_unaligned_u32(target + 6);
          result.path_size_offset = read_unaligned_u32(target + 16) + empty_check[3];
          ++path_predicates;
        }
      }
    }
    cs_free(instructions, count);
  }
  cs_close(&decoder);
  if (!result.row_paint_slot || row_accessors != 1 || pointer_predicates != 1 || path_predicates != 1 ||
      path_flag != result.flag_offset || result.row_data_offset < 8 || result.row_data_offset % 8 ||
      result.flag_offset < 8 || result.flag_offset > 1024 || result.pointer_offset < 8 ||
      result.pointer_offset > 1024 || result.pointer_offset % 8 || result.path_size_offset < 8 ||
      result.path_size_offset > 1024 || result.path_size_offset % 8) {
    return {};
  }
  return result;
}

bool is_legacy_material_folder(const LegacyMaterialLayout& layout, const void* row) {
  if (!layout.row_paint_slot || !row || !memory_readable(row, layout.row_data_offset + sizeof(void*))) {
    return false;
  }
  const auto* data = *reinterpret_cast<const unsigned char* const*>(static_cast<const unsigned char*>(row) + layout.row_data_offset);
  const size_t size = std::max({layout.flag_offset, layout.pointer_offset, layout.path_size_offset}) + sizeof(void*);
  return memory_readable(data, size) && *reinterpret_cast<const void* const*>(data) == layout.data_vtable &&
    data[layout.flag_offset] == 0 && (read_unaligned_pointer(data + layout.pointer_offset) != 0 ||
      read_unaligned_pointer(data + layout.path_size_offset) != 0);
}

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_material_image_builds = 0;
#endif

bool create_legacy_material_image(const void* source, void* result) {
  static const HMODULE gui = GetModuleHandleW(L"Qt6Gui.dll");
  static const auto width_proc = reinterpret_cast<int(__cdecl*)(const void*)>(GetProcAddress(gui, "?width@QImage@@QEBAHXZ"));
  static const auto height_proc = reinterpret_cast<int(__cdecl*)(const void*)>(GetProcAddress(gui, "?height@QImage@@QEBAHXZ"));
  static const auto format_proc = reinterpret_cast<int(__cdecl*)(const void*)>(GetProcAddress(gui, "?format@QImage@@QEBA?AW4Format@1@XZ"));
  static const auto pixel_proc = reinterpret_cast<unsigned int(__cdecl*)(const void*, int, int)>(GetProcAddress(gui, "?pixel@QImage@@QEBAIHH@Z"));
  static const auto set_pixel = reinterpret_cast<void(__cdecl*)(void*, int, int, unsigned int)>(GetProcAddress(gui, "?setPixel@QImage@@QEAAXHHI@Z"));
  static const auto copy_proc = reinterpret_cast<void(__cdecl*)(void*, const void*)>(GetProcAddress(gui, "??0QImage@@QEAA@AEBV0@@Z"));
  static const auto read_row = reinterpret_cast<const unsigned char*(__cdecl*)(const void*, int)>(GetProcAddress(gui, "?constScanLine@QImage@@QEBAPEBEH@Z"));
  static const auto write_row = reinterpret_cast<unsigned char*(__cdecl*)(void*, int)>(GetProcAddress(gui, "?scanLine@QImage@@QEAAPEAEH@Z"));
  if (!source || !result || !width_proc || !height_proc || !format_proc || !pixel_proc || !set_pixel || !copy_proc) {
    return false;
  }
  const int width = width_proc(source);
  const int height = height_proc(source);
  const int format = format_proc(source);
  if (width <= 0 || height <= 0 || width > 512 || height > 512 ||
      (format != 4 && format != 5 && format != 6 && format != 13 && format != 29)) {
    return false;
  }
  std::vector<const unsigned char*> source_rows;
  if (read_row && write_row) {
    for (int row = 0; row < height; ++row) {
      source_rows.push_back(read_row(source, row));
    }
  }
  auto is_backing = [&](int column, int row) {
    unsigned int pixel = 0;
    if (source_rows.empty()) {
      pixel = pixel_proc(source, column, row);
    } else if (format == 13 || format == 29) {
      const auto* channels = source_rows[row] + column * 3;
      pixel = 0xff000000u | (static_cast<unsigned int>(channels[format == 13 ? 0 : 2]) << 16) |
        (static_cast<unsigned int>(channels[1]) << 8) | channels[format == 13 ? 2 : 0];
    } else {
      std::memcpy(&pixel, source_rows[row] + column * 4, sizeof(pixel));
    }
    const int red = (pixel >> 16) & 255;
    const int green = (pixel >> 8) & 255;
    const int blue = pixel & 255;
    return (pixel >> 24) == 255 && std::min({red, green, blue}) >= 245 &&
      std::max({red, green, blue}) - std::min({red, green, blue}) <= 8;
  };
  if (!is_backing(0, 0) || !is_backing(width - 1, 0) ||
      !is_backing(0, height - 1) || !is_backing(width - 1, height - 1)) {
    return false;
  }
  copy_proc(result, source);
#if defined(DARKMODE_RUNTIME_TESTING)
  ++g_test_material_image_builds;
#endif
  std::vector<unsigned char*> destination_rows;
  if (!source_rows.empty()) {
    for (int row = 0; row < height; ++row) {
      destination_rows.push_back(write_row(result, row));
    }
  }
  std::vector<unsigned char> visited(static_cast<size_t>(width) * height);
  std::vector<size_t> pending;
  pending.reserve(static_cast<size_t>(width) * height);
  auto add = [&](int column, int row) {
    if (column < 0 || row < 0 || column >= width || row >= height) {
      return;
    }
    const size_t position = static_cast<size_t>(row) * width + column;
    if (!visited[position]) {
      visited[position] = 1;
      if (is_backing(column, row)) {
        pending.push_back(position);
      }
    }
  };
  for (int column = 0; column < width; ++column) {
    add(column, 0);
    add(column, height - 1);
  }
  for (int row = 0; row < height; ++row) {
    add(0, row);
    add(width - 1, row);
  }
  for (size_t index = 0; index < pending.size(); ++index) {
    const int column = static_cast<int>(pending[index] % width);
    const int row = static_cast<int>(pending[index] / width);
    if (destination_rows.empty()) {
      set_pixel(result, column, row, 0xff2b2b2b);
    } else if (format == 13 || format == 29) {
      std::memset(destination_rows[row] + column * 3, 43, 3);
    } else {
      const unsigned int color = 0xff2b2b2bu;
      std::memcpy(destination_rows[row] + column * 4, &color, sizeof(color));
    }
    add(column - 1, row);
    add(column + 1, row);
    add(column, row - 1);
    add(column, row + 1);
  }
  return true;
}

LegacyMaterialLayout g_legacy_material_layout;
VcbStatusPaintHook g_legacy_material_row_hook;
std::uintptr_t g_legacy_material_image_return = 0;
thread_local bool g_legacy_material_folder_scope = false;
struct LegacyMaterialImageCache {
  long long key = 0;
  bool constructed = false;
  size_t bytes = 0;
  std::uint64_t age = 0;
  alignas(16) unsigned char image[256] = {};
};
std::vector<std::unique_ptr<LegacyMaterialImageCache>> g_legacy_material_images;
size_t g_legacy_material_bytes = 0;
std::uint64_t g_legacy_material_age = 0;
constexpr size_t legacy_material_budget = 16 * 1024 * 1024;

using LegacyMaterialRowFn = void(__cdecl*)(void*, void*, void*, const void*);

void __cdecl legacy_material_row_hook(void* widget, void* painter, void* row, const void* rect) {
  const auto original = reinterpret_cast<LegacyMaterialRowFn>(g_legacy_material_row_hook.original_proc);
  if (!original) {
    return;
  }
  const bool previous = g_legacy_material_folder_scope;
  g_legacy_material_folder_scope = is_legacy_material_folder(g_legacy_material_layout, row);
  original(widget, painter, row, rect);
  g_legacy_material_folder_scope = previous;
}

bool draw_legacy_material_image(void* painter, const void* rect, const void* image,
    std::uintptr_t caller, ContentBrowserDrawPixmapFn original) {
  if (!g_legacy_material_folder_scope || !g_legacy_material_image_return || caller != g_legacy_material_image_return) {
    return false;
  }
  static const auto cache_key = reinterpret_cast<long long(__cdecl*)(const void*)>(
    GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"), "?cacheKey@QImage@@QEBA_JXZ"));
  const long long key = cache_key ? cache_key(image) : 0;
  if (!key) {
    return false;
  }
  auto found = std::find_if(g_legacy_material_images.begin(), g_legacy_material_images.end(),
    [key](const auto& cached) { return cached->key == key; });
  if (found == g_legacy_material_images.end()) {
    auto cached = std::make_unique<LegacyMaterialImageCache>();
    cached->key = key;
    cached->constructed = create_legacy_material_image(image, cached->image);
    static const auto image_bytes = reinterpret_cast<long long(__cdecl*)(const void*)>(
      GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"), "?sizeInBytes@QImage@@QEBA_JXZ"));
    cached->bytes = cached->constructed ? (image_bytes ? static_cast<size_t>(image_bytes(cached->image)) : 1024 * 1024) : 0;
    while (!g_legacy_material_images.empty() &&
        (g_legacy_material_images.size() >= 512 || g_legacy_material_bytes + cached->bytes > legacy_material_budget)) {
      const auto oldest = std::min_element(g_legacy_material_images.begin(), g_legacy_material_images.end(),
        [](const auto& first, const auto& second) { return first->age < second->age; });
      if ((*oldest)->constructed) {
        reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)((*oldest)->image);
      }
      g_legacy_material_bytes -= (*oldest)->bytes;
      g_legacy_material_images.erase(oldest);
    }
    g_legacy_material_bytes += cached->bytes;
    g_legacy_material_images.push_back(std::move(cached));
    found = g_legacy_material_images.end() - 1;
  }
  (*found)->age = ++g_legacy_material_age;
  if (!(*found)->constructed) {
    return false;
  }
  original(painter, rect, (*found)->image);
  return true;
}

bool set_legacy_material_image_hook(const QtLookup& lookup, bool dark) {
  if (!dark) {
    g_legacy_material_image_return = 0;
    g_legacy_material_layout = {};
    for (const auto& cached : g_legacy_material_images) {
      if (cached->constructed) {
        reinterpret_cast<void(__cdecl*)(void*)>(g_toolbar_title_api.image_destructor)(cached->image);
      }
    }
    g_legacy_material_images.clear();
    g_legacy_material_bytes = 0;
    if (!g_legacy_material_row_hook.slot) {
      return true;
    }
    DWORD protection = 0;
    if (!VirtualProtect(g_legacy_material_row_hook.slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      return false;
    }
    if (*g_legacy_material_row_hook.slot == reinterpret_cast<void*>(&legacy_material_row_hook)) {
      *g_legacy_material_row_hook.slot = g_legacy_material_row_hook.original_proc;
    }
    DWORD unused = 0;
    VirtualProtect(g_legacy_material_row_hook.slot, sizeof(void*), protection, &unused);
    g_legacy_material_row_hook = {};
    return true;
  }
  if (g_legacy_material_row_hook.slot) {
    return *g_legacy_material_row_hook.slot == reinterpret_cast<void*>(&legacy_material_row_hook);
  }
  const HMODULE module = GetModuleHandleW(nullptr);
  const auto layout = discover_legacy_material_layout(module);
  if (!layout.row_paint_slot) {
    return true;
  }
  if (!resolve_toolbar_title_paint_api(lookup)) {
    return false;
  }
  if (!g_about_image_hook.slot) {
    const auto original = GetProcAddress(GetModuleHandleW(L"Qt6Gui.dll"),
      "?drawImage@QPainter@@QEAAXAEBVQRect@@AEBVQImage@@@Z");
    DWORD protection = 0;
    if (!original || *layout.image_slot != reinterpret_cast<void*>(original) ||
        !VirtualProtect(layout.image_slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      return false;
    }
    g_about_image_hook = {layout.image_slot, *layout.image_slot};
    *layout.image_slot = reinterpret_cast<void*>(&about_draw_image_hook);
    DWORD unused = 0;
    VirtualProtect(layout.image_slot, sizeof(void*), protection, &unused);
  }
  if (g_about_image_hook.slot != layout.image_slot || *layout.image_slot != reinterpret_cast<void*>(&about_draw_image_hook)) {
    return false;
  }
  DWORD protection = 0;
  if (!VirtualProtect(layout.row_paint_slot, sizeof(void*), PAGE_READWRITE, &protection)) {
    return false;
  }
  g_legacy_material_layout = layout;
  g_legacy_material_row_hook = {layout.row_paint_slot, *layout.row_paint_slot};
  g_legacy_material_image_return = reinterpret_cast<std::uintptr_t>(module) + layout.image_return_rva;
  *layout.row_paint_slot = reinterpret_cast<void*>(&legacy_material_row_hook);
  DWORD unused = 0;
  VirtualProtect(layout.row_paint_slot, sizeof(void*), protection, &unused);
  return true;
}

std::vector<DiscoveredFolderColor> discover_content_browser_folder_colors(HMODULE module) {
  const NativeImageView image(module);
  auto** color_slot = image.import_slot("??0QColor@@QEAA@HHHH@Z");
  auto** outline_slot = image.import_slot("?drawRects@QPainter@@QEAAXPEBVQRect@@H@Z");
  const void* table = primary_rtti_vtable(find_vtables_for_type(".?AVContentBrowserListCtrl@@", module));
  if (!color_slot || !outline_slot || !memory_readable(table, 28 * sizeof(void*))) {
    return {};
  }
  const void* paint = image.function_body(static_cast<const void* const*>(table)[27]);
  const size_t length = image.function_size(paint);
  csh decoder = 0;
  if (!length || cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK) {
    return {};
  }
  cs_option(decoder, CS_OPT_DETAIL, CS_OPT_ON);
  cs_insn* instructions = nullptr;
  const size_t count = cs_disasm(decoder, static_cast<const uint8_t*>(paint), length,
    reinterpret_cast<uint64_t>(paint), 0, &instructions);
  std::unordered_set<uint64_t> branch_targets;
  for (size_t index = 0; index < count; ++index) {
    const auto& instruction = instructions[index];
    if (cs_insn_group(decoder, &instruction, CS_GRP_JUMP) &&
        instruction.detail->x86.operands[0].type == X86_OP_IMM) {
      branch_targets.insert(instruction.detail->x86.operands[0].imm);
    }
  }
  long long values[16];
  std::fill(std::begin(values), std::end(values), -1);
  int alpha = -1;
  DiscoveredFolderColor pending;
  std::vector<DiscoveredFolderColor> result;
  for (size_t index = 0; index < count; ++index) {
    const auto& instruction = instructions[index];
    const auto& operands = instruction.detail->x86;
    if (branch_targets.count(instruction.address)) {
      std::fill(std::begin(values), std::end(values), -1);
      alpha = -1;
      pending = {};
    }
    long long assigned = -1;
    int destination = -1;
    if (instruction.id == X86_INS_MOV && operands.op_count == 2) {
      const auto& target = operands.operands[0];
      const auto& source = operands.operands[1];
      if (target.type == X86_OP_REG && target.size >= 4) {
        destination = native_register_index(target.reg);
        const int source_index = source.type == X86_OP_REG ? native_register_index(source.reg) : -1;
        assigned = source.type == X86_OP_IMM ? source.imm : (source_index >= 0 ? values[source_index] : -1);
      } else if (target.type == X86_OP_MEM && target.mem.base == X86_REG_RSP &&
                 target.mem.index == X86_REG_INVALID && target.mem.disp == 0x20) {
        alpha = source.type == X86_OP_IMM ? static_cast<int>(source.imm) : -1;
      }
    }
    if (instruction.id == X86_INS_CALL) {
      const auto& target = operands.operands[0];
      const auto slot = target.type == X86_OP_MEM && target.mem.base == X86_REG_RIP
        ? instruction.address + instruction.size + target.mem.disp : 0;
      const size_t return_rva = instruction.address + instruction.size - reinterpret_cast<uint64_t>(image.base);
      if (slot == reinterpret_cast<uint64_t>(color_slot)) {
        pending = {};
        if (alpha == 255 && values[2] == values[8] && values[2] == values[9] &&
            (values[2] == 230 || values[2] == 240 || values[2] == 255)) {
          pending = {return_rva, static_cast<int>(values[2]), 0};
        }
      } else if (slot == reinterpret_cast<uint64_t>(outline_slot) && pending.return_rva) {
        if (values[8] == 1 && return_rva - pending.return_rva < 512) {
          pending.outline_return_rva = return_rva;
          result.push_back(pending);
        }
        pending = {};
      }
      for (int volatile_register : {0, 1, 2, 8, 9, 10, 11}) {
        values[volatile_register] = -1;
      }
      alpha = -1;
    } else if (cs_insn_group(decoder, &instruction, CS_GRP_JUMP) ||
               cs_insn_group(decoder, &instruction, CS_GRP_RET)) {
      std::fill(std::begin(values), std::end(values), -1);
      alpha = -1;
      pending = {};
    } else {
      cs_regs read = {}, written = {};
      uint8_t read_count = 0, written_count = 0;
      if (cs_regs_access(decoder, &instruction, read, &read_count, written, &written_count) == CS_ERR_OK) {
        for (uint8_t register_index = 0; register_index < written_count; ++register_index) {
          const int affected = native_register_index(written[register_index]);
          if (affected >= 0) {
            values[affected] = -1;
          }
        }
      }
      if (destination >= 0) {
        values[destination] = assigned;
      }
    }
  }
  const bool complete = count && instructions[count - 1].address + instructions[count - 1].size ==
    reinterpret_cast<uint64_t>(paint) + length;
  cs_free(instructions, count);
  cs_close(&decoder);
  if (!complete || result.size() != 3 || result[0].native_shade != 230 ||
      result[1].native_shade != 240 || result[2].native_shade != 255) {
    return {};
  }
  return result;
}

std::vector<void**> content_browser_folder_color_slots(const unsigned char* image_base, size_t image_size,
                                                     void* original_proc, bool outline = false) {
  std::vector<void**> slots;
  if (!image_base || !original_proc) {
    return slots;
  }
  for (const auto& color : g_content_browser_folder_colors) {
    const size_t return_rva = outline ? color.outline_return_rva : color.return_rva;
    if (return_rva < 6 || return_rva > image_size) {
      return {};
    }
    const auto* instruction = image_base + return_rva - 6;
    if (!memory_readable(instruction, 6) || instruction[0] != 0xff || instruction[1] != 0x15) {
      return {};
    }
    std::int32_t displacement = 0;
    std::memcpy(&displacement, instruction + 2, sizeof(displacement));
    const auto slot_rva = static_cast<std::int64_t>(return_rva) + displacement;
    if (slot_rva < 0 || static_cast<size_t>(slot_rva) > image_size ||
        image_size - static_cast<size_t>(slot_rva) < sizeof(void*)) {
      return {};
    }
    auto** slot = reinterpret_cast<void**>(const_cast<unsigned char*>(image_base) + slot_rva);
    if (!memory_readable(slot, sizeof(*slot)) || *slot != original_proc) {
      return {};
    }
    if (std::find(slots.begin(), slots.end(), slot) == slots.end()) {
      slots.push_back(slot);
    }
  }
  return slots;
}

int content_browser_folder_background_shade(const void* return_address, int red, int green, int blue, int alpha) {
  if (!g_content_browser_text_dark || g_content_browser_paint_scope_depth <= 0 ||
      !g_content_browser_folder_color_image_base || red != green || red != blue || alpha != 255) {
    return -1;
  }
  const auto return_value = reinterpret_cast<std::uintptr_t>(return_address);
  const auto image_base = reinterpret_cast<std::uintptr_t>(g_content_browser_folder_color_image_base);
  for (const auto& color : g_content_browser_folder_colors) {
    if (return_value == image_base + color.return_rva && red == color.native_shade) {
      return color.dark_shade;
    }
  }
  return -1;
}

bool draw_content_browser_folder_outline(const QtLookup& lookup, void* painter, const void* rectangles,
                                        int count, const void* return_address) {
  if (!g_content_browser_text_dark || g_content_browser_paint_scope_depth <= 0 ||
      !g_content_browser_folder_color_image_base || !painter || !rectangles || count != 1) {
    return false;
  }
  const auto return_value = reinterpret_cast<std::uintptr_t>(return_address);
  const auto image_base = reinterpret_cast<std::uintptr_t>(g_content_browser_folder_color_image_base);
  const bool folder_rectangle = std::any_of(g_content_browser_folder_colors.begin(),
    g_content_browser_folder_colors.end(), [return_value, image_base](const auto& color) {
      return return_value == image_base + color.outline_return_rva;
    });
  if (!folder_rectangle) {
    return false;
  }
  const auto& api = g_content_browser_folder_outline_api;
  const void* native_pen = reinterpret_cast<const void*(__cdecl*)(const void*)>(api.painter_pen)(painter);
  alignas(16) unsigned char color[64] = {};
  reinterpret_cast<void*(__cdecl*)(const void*, void*)>(api.pen_color)(native_pen, color);
  if (reinterpret_cast<QtColorRgbFn>(lookup.qcolor_rgb_proc)(color) != 0xffa0a0a0u) {
    return false;
  }
  alignas(16) unsigned char pen[64] = {};
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(api.pen_copy)(pen, native_pen);
  reinterpret_cast<QtColorRgbCtorFn>(lookup.qcolor_rgb_ctor_proc)(color, 90, 90, 90, 255);
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(api.pen_set_color)(pen, color);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_save_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*, const void*)>(api.painter_set_pen)(painter, pen);
  reinterpret_cast<ContentBrowserDrawRectsFn>(api.draw_rects)(painter, rectangles, count);
  reinterpret_cast<void(__cdecl*)(void*)>(lookup.qpainter_restore_proc)(painter);
  reinterpret_cast<void(__cdecl*)(void*)>(api.pen_destructor)(pen);
  return true;
}

void __cdecl content_browser_folder_outline_hook(void* painter, const void* rectangles, int count) {
  if (!draw_content_browser_folder_outline(resolve_qt_lookup(), painter, rectangles, count, _ReturnAddress())) {
    reinterpret_cast<ContentBrowserDrawRectsFn>(g_content_browser_folder_outline_api.draw_rects)(painter, rectangles, count);
  }
}

void* __cdecl content_browser_folder_color_hook(void* color, int red, int green, int blue, int alpha) {
  const int shade = content_browser_folder_background_shade(_ReturnAddress(), red, green, blue, alpha);
  if (shade >= 0) {
    red = green = blue = shade;
  }
  g_content_browser_qcolor_rgb_ctor_proc(color, red, green, blue, alpha);
  return color;
}

size_t clear_content_browser_cached_images(void* widget) {
  if (!pointer_looks_like_cpp_object(widget) || !g_content_browser_qimage_default_ctor_proc ||
      !g_content_browser_qimage_destructor_proc) {
    return 0;
  }

  alignas(16) unsigned char empty_image[64] = {};
  g_content_browser_qimage_default_ctor_proc(empty_image);
  const void* expected_image_vtable = *reinterpret_cast<const void* const*>(empty_image);
  g_content_browser_qimage_destructor_proc(empty_image);

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
    if (!row || !memory_readable(row + 0x60, 24) ||
        *reinterpret_cast<const void* const*>(row + 0x60) != expected_image_vtable) {
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
    if (!g_content_browser_qimage_fill_color_hooks.empty() &&
        g_content_browser_image_cache_reset_widgets.insert(widget).second) {
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
  if (!g_content_browser_text_dark || !resource_image_renderer) {
    g_content_browser_qimage_fill_color_proc(image, color);
    return;
  }
  const unsigned int rgb = color && g_content_browser_qcolor_rgb_proc
    ? g_content_browser_qcolor_rgb_proc(color) & 0x00ffffffu
    : 0xffffffffu;
  const bool white = rgb == 0x00ffffffu && content_browser_image_generator_on_stack();
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
  const QtLookup& lookup = resolve_qt_lookup();
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
  restore_hooks(g_content_browser_folder_color_hooks, reinterpret_cast<void*>(&content_browser_folder_color_hook));
  restore_hooks(g_content_browser_folder_outline_hooks, reinterpret_cast<void*>(&content_browser_folder_outline_hook));
  g_content_browser_draw_text_option_hooks.clear();
  g_content_browser_draw_text_flags_hooks.clear();
  g_content_browser_qimage_fill_color_hooks.clear();
  g_content_browser_folder_color_hooks.clear();
  g_content_browser_folder_outline_hooks.clear();
  g_content_browser_folder_outline_api = {};
  g_content_browser_folder_color_image_base = nullptr;
  g_content_browser_folder_colors.clear();
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

  const QtLookup& lookup = resolve_qt_lookup();
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
  if (const void* vtable = primary_rtti_vtable(content_browser_vtables)) {
    void** slots = const_cast<void**>(reinterpret_cast<void* const*>(vtable));
    if (memory_readable(slots, (paint_event_slot + 1) * sizeof(*slots)) &&
        is_executable_address(reinterpret_cast<const unsigned char*>(slots[paint_event_slot]))) {
      paint_slot = &slots[paint_event_slot];
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
  g_content_browser_folder_colors.clear();
  for (const auto& color : discover_content_browser_folder_colors(GetModuleHandleW(nullptr))) {
    const int dark_shade = color.native_shade == 230 ? 36 : (color.native_shade == 240 ? 40 : 43);
    g_content_browser_folder_colors.push_back({color.return_rva, color.native_shade, dark_shade, color.outline_return_rva});
  }
  const std::vector<void**> folder_color_slots = content_browser_folder_color_slots(
    image_base, image_size, reinterpret_cast<void*>(lookup.qcolor_rgb_ctor_proc));
  const std::vector<void**> folder_outline_slots = resolve_content_browser_folder_outline_api(lookup)
    ? content_browser_folder_color_slots(image_base, image_size,
        reinterpret_cast<void*>(g_content_browser_folder_outline_api.draw_rects), true)
    : std::vector<void**>{};
  const std::vector<void**> draw_text_option_slots = find_function_indirect_call_slots(
    const_cast<void*>(original_paint_proc), {reinterpret_cast<void*>(draw_text_option_proc)});
  const std::vector<void**> draw_text_flags_slots = find_function_indirect_call_slots(
    const_cast<void*>(original_paint_proc), {
      reinterpret_cast<void*>(draw_text_flags_proc),
      reinterpret_cast<void*>(draw_text_rect_flags_proc)
    });
  std::vector<void**> qimage_fill_color_slots;
  g_content_browser_image_generator_begin = nullptr;
  g_content_browser_image_generator_end = nullptr;
  g_content_browser_resource_image_renderer_begin = nullptr;
  g_content_browser_resource_image_renderer_end = nullptr;
  const auto image_functions = discover_content_browser_image_functions(executable);
  if (image_functions.fill_slot && *image_functions.fill_slot == reinterpret_cast<void*>(qimage_fill_color_proc)) {
    const NativeImageView image(executable);
    g_content_browser_image_generator_begin = static_cast<const unsigned char*>(image_functions.generator);
    g_content_browser_image_generator_end = g_content_browser_image_generator_begin + image.function_size(image_functions.generator);
    g_content_browser_resource_image_renderer_begin = static_cast<const unsigned char*>(image_functions.renderer);
    g_content_browser_resource_image_renderer_end = g_content_browser_resource_image_renderer_begin + image.function_size(image_functions.renderer);
    qimage_fill_color_slots.push_back(image_functions.fill_slot);
  }
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
  g_content_browser_folder_color_image_base = image_base;
  install_import_hooks(folder_color_slots, reinterpret_cast<void*>(&content_browser_folder_color_hook),
    g_content_browser_folder_color_hooks);
  install_import_hooks(folder_outline_slots, reinterpret_cast<void*>(&content_browser_folder_outline_hook),
    g_content_browser_folder_outline_hooks);
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

  const QtLookup& lookup = resolve_qt_lookup();
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
  const void* vtable = nullptr;
};

std::vector<EntityInfoInputPaintHook> g_entity_info_input_paint_hooks;
bool g_entity_info_input_paint_dark = false;
LONG g_entity_info_input_paint_hook_invocation_count = 0;
thread_local bool g_entity_info_input_palette_applying = false;

bool is_entity_info_input_widget(const QtLookup& lookup, void* widget) {
  static const auto tables = find_vtables_for_type(".?AVCEntityInfoDlg@@");
  if (tables.empty()) {
    return false;
  }
  for (int depth = 0; widget && depth < 16; ++depth) {
    if (!memory_readable(widget, sizeof(void*))) {
      return false;
    }
    if (std::find(tables.begin(), tables.end(), *static_cast<const void* const*>(widget)) != tables.end()) {
      return true;
    }
    widget = qwidget_parent_widget(lookup, widget);
  }
  return false;
}

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_entity_palette_preparations = 0;
#endif

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

  if (dark && widget_theme_stamp(lookup, widget, "_darkmode_entity_palette", false)) {
    return true;
  }
#if defined(DARKMODE_RUNTIME_TESTING)
  ++g_test_entity_palette_preparations;
#endif
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
  if (dark) {
    widget_theme_stamp(lookup, widget, "_darkmode_entity_palette", true);
  }
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
  std::string current;
  if (read_omnibar_stylesheet(lookup, widget, current) && current == stylesheet) {
    return true;
  }
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
  for (const auto& hook : g_entity_info_input_paint_hooks) {
    if (hook.vtable == vtable) {
      return reinterpret_cast<EntityInfoInputPaintFn>(hook.original_proc);
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
    const QtLookup& lookup = resolve_qt_lookup();
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
      g_entity_info_input_paint_hooks.push_back({slot, original_proc, vtable});
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

std::vector<void*> find_overlays_dialog_widgets(
  const QtLookup& lookup,
  const std::vector<const void*>& vtables = find_vtables_for_type(".?AVCModelServiceDialog@@")
) {
  std::vector<void*> matches;
  if (!lookup.app_all_widgets_proc || vtables.empty()) {
    return matches;
  }

  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, static_cast<size_t>(-1)));
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }
    const void* vtable = *reinterpret_cast<const void* const*>(widget);
    if (std::find(vtables.begin(), vtables.end(), vtable) != vtables.end()) {
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

#if defined(DARKMODE_RUNTIME_TESTING)
size_t g_test_vcb_palette_preparations = 0;
#endif

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

  if (dark && widget_theme_stamp(lookup, widget, "_darkmode_vcb_palette", false)) {
    return true;
  }
#if defined(DARKMODE_RUNTIME_TESTING)
  ++g_test_vcb_palette_preparations;
#endif
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
  if (dark) {
    widget_theme_stamp(lookup, widget, "_darkmode_vcb_palette", true);
  }
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
  const size_t background_offset = modern_workspace_types().empty() ? kVcbEditTypedInputBackgroundOffset : 0x48;
  auto* typed_input_background = reinterpret_cast<unsigned char*>(widget) + background_offset;
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
    const QtLookup& lookup = resolve_qt_lookup();
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

bool is_extension_manager_url(const std::string& url) {
  const std::string lower = lower_ascii(url);
  const size_t end = lower.find_first_of("?#");
  const std::string path = lower.substr(0, end);
  constexpr char suffix[] = "/dialogs/extensionmanager/extension_manager.html";
  return path.compare(0, 7, "file://") == 0 && path.size() >= sizeof(suffix) - 1 &&
    path.compare(path.size() - (sizeof(suffix) - 1), sizeof(suffix) - 1, suffix) == 0;
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
    ? R"DARKMODE((function(){
      var probeKey='__darkModeSkpModelServicesProbe';
      if(!window[probeKey]){
        window[probeKey]=true;
        if(window.sketchup&&typeof window.sketchup.get_l10n_strings==='function'){
          window.sketchup.get_l10n_strings({onCompleted:function(){}});
        }
      }
      var key='__darkModeSkpModelServicesStyle';
      var root=document.head||document.documentElement;
      if(!root||document.getElementById(key)){return;}
      var style=document.createElement('style');
      style.id=key;
      style.textContent=
        'html,body,#app,.list-group.list-group-condensed .list-group-item{background-color:rgb(43,43,43)!important;color:rgb(216,219,226)!important;}' +
        '.su-service-header{background-color:rgb(48,48,48)!important;border-bottom-color:rgb(68,68,68)!important;color:rgb(216,219,226)!important;}' +
        '.su-service-header .align-items-center > div{background-color:rgb(48,48,48)!important;border-left-color:rgb(68,68,68)!important;border-right-color:rgb(68,68,68)!important;color:rgb(216,219,226)!important;}' +
        '.su-source,.su-drag-handle span{color:rgb(160,164,172)!important;}';
      root.appendChild(style);
    })();)DARKMODE"
    : R"DARKMODE((function(){
      var style=document.getElementById('__darkModeSkpModelServicesStyle');
      if(style&&style.parentNode){style.parentNode.removeChild(style);}
    })();)DARKMODE";
}

constexpr const char* k_extension_manager_ui_icons[] = {
  "dlg_blue_check_mark", "dlg_settings", "dlg_help", "dlg_expand_less", "dlg_expand_more",
  "dlg_xMark", "dlg_sign_in", "dlg_selectall", "dlg_deselectall"
};

std::string extension_manager_icon_svg(const std::string& alias, std::string svg) {
  if (std::find(std::begin(k_extension_manager_ui_icons), std::end(k_extension_manager_ui_icons), alias) ==
      std::end(k_extension_manager_ui_icons)) {
    return svg;
  }
  const size_t root = svg.find("<svg");
  if (root == std::string::npos) {
    return {};
  }
  for (const char* attribute : {"fill", "stroke"}) {
    for (const char quote : {'\'', '"'}) {
      for (const std::string tone : {"#000", "#000000", "#333333", "#4a4a4a", "#cccccc", "#005f9e", "#144689"}) {
        const bool blue = tone == "#005f9e" || tone == "#144689";
        const char* ink = alias == "dlg_blue_check_mark" ? "#ffffff" : blue ? "#4772b3" : "#d8dbe2";
        const std::string original = std::string(attribute) + '=' + quote + tone + quote;
        const std::string replacement = std::string(attribute) + '=' + quote + ink + quote;
        size_t offset = 0;
        while ((offset = lower_ascii(svg).find(original, offset)) != std::string::npos) {
          svg.replace(offset, original.size(), replacement);
          offset += replacement.size();
        }
      }
    }
  }
  if (alias == "dlg_sign_in") {
    svg.insert(root + 4, " fill=\"#d8dbe2\"");
  }
  return svg;
}

std::string extension_manager_qml_source(std::string source, const std::vector<std::string>& ui_icons = {}) {
  const auto replace = [&](const std::string& before, const std::string& after) {
    size_t offset = 0;
    while ((offset = source.find(before, offset)) != std::string::npos) {
      source.replace(offset, before.size(), after);
      offset += after.size();
    }
  };
  for (const auto& alias : ui_icons) {
    const std::string original = "\"image://ui/" + alias + "\"";
    replace(original, "(__darkModeSkpPalette.window.hslLightness < 0.5 ? \"qrc:/__darkmode_skp_extension_manager/" +
      alias + ".svg\" : " + original + ")");
  }
  if (source.find("extension_selected ? \"white\"") != std::string::npos) {
    replace("color: root.fontColor", "color: (__darkModeSkpPalette.window.hslLightness < 0.5 && "
      "typeof extension_selected !== \"undefined\" && extension_selected) ? \"white\" : root.fontColor");
  }
  replace("case Button.ButtonType.Warning:\n            \"#000000\";",
    "case Button.ButtonType.Warning:\n            \"black\";");
  replace("background: Rectangle {\n        color: \"white\"",
    "background: Rectangle {\n        color: (__darkModeSkpPalette.window.hslLightness < 0.5 ? \"#303030\" : \"white\")");
  replace("color: tbDiscard.hovered ? \"#e6e6e6\" : \"white\"",
    "color: tbDiscard.hovered ? \"#e6e6e6\" : (__darkModeSkpPalette.window.hslLightness < 0.5 ? \"#303030\" : \"white\")");
  const std::pair<const char*, const char*> colors[] = {
    {"#f3f3f7", "#2b2b2b"}, {"#f9f9fb", "#303030"}, {"#e2e2e7", "#242424"},
    {"#e6e6e6", "#444444"}, {"#d2d2d2", "#555555"}, {"#d0d0d7", "#444444"},
    {"#787878", "#d8dbe2"}, {"#727272", "#d8dbe2"}, {"#979797", "#555555"},
    {"#1a1a1a", "#709ce6"}, {"#000000", "#f0f2f7"},
    {"#005f9e", "#4772b3"}, {"#00548c", "#4772b3"}, {"#2d7bb8", "#4772b3"},
    {"#4a90e2", "#4772b3"}, {"#197cb6", "#5684c4"}, {"#00406b", "#5684c4"},
    {"#002b46", "#385b8f"}, {"#002b47", "#385b8f"},
    {"#4a92e2", "#709ce6"}, {"#003152", "#98bdf0"},
    {"#aeaeb6", "#555555"}, {"#8c8c8c", "#a5abb5"}
  };
  for (const auto& color : colors) {
    for (const char quote : {'\'', '"'}) {
      const std::string original = std::string(1, quote) + color.first + quote;
      replace(original, "(__darkModeSkpPalette.window.hslLightness < 0.5 ? \"" +
        std::string(color.second) + "\" : " + original + ")");
    }
  }
  for (const auto& expression : {std::make_pair("extension_selected ? \"white\"", "#4772b3"),
      std::make_pair("cancelBtnHoveredColor : \"white\"", "#383838")}) {
    const std::string native = expression.first;
    const size_t white = native.find("\"white\"");
    replace(native, native.substr(0, white) + "(__darkModeSkpPalette.window.hslLightness < 0.5 ? \"" +
      expression.second + "\" : \"white\")");
  }
  if (source.find("__darkModeSkpPalette") == std::string::npos) {
    return source;
  }
  const size_t root = source.find('{');
  if (root == std::string::npos || source.find("import QtQuick") == std::string::npos) {
    return {};
  }
  source.insert(root + 1, "\n    readonly property SystemPalette __darkModeSkpPalette: SystemPalette {}\n");
  return source;
}

struct ExtensionManagerQmlApi {
  using Destroy = void(__cdecl*)(void*);
  using ReturnValue = void*(__cdecl*)(const void*, void*);
  void(__cdecl* string_create)(void*, const char*, long long) = nullptr;
  Destroy string_destroy = nullptr;
  ReturnValue string_bytes = nullptr;
  Destroy bytes_destroy = nullptr;
  const char*(__cdecl* bytes_data)(const void*) = nullptr;
  long long(__cdecl* bytes_size)(const void*) = nullptr;
  void(__cdecl* locale_create)(void*) = nullptr;
  Destroy locale_destroy = nullptr;
  void(__cdecl* resource_create)(void*, const void*, const void*) = nullptr;
  Destroy resource_destroy = nullptr;
  ReturnValue resource_bytes = nullptr;
  void(__cdecl* iterator_create)(void*, const void*, unsigned int) = nullptr;
  Destroy iterator_destroy = nullptr;
  bool(__cdecl* iterator_has_next)(const void*) = nullptr;
  void*(__cdecl* iterator_next)(void*, void*) = nullptr;
  bool(__cdecl* register_data)(int, const void*, const void*, const void*) = nullptr;
  bool(__cdecl* unregister_data)(int, const void*, const void*, const void*) = nullptr;
  void*(__cdecl* url_bytes)(const void*, void*, int) = nullptr;
  void(__cdecl* url_create)(void*, const void*, int) = nullptr;
  Destroy url_destroy = nullptr;

  bool ready() const {
    return string_create && string_destroy && string_bytes && bytes_destroy && bytes_data && bytes_size &&
      locale_create && locale_destroy && resource_create && resource_destroy && resource_bytes &&
      iterator_create && iterator_destroy && iterator_has_next && iterator_next && register_data && unregister_data &&
      url_bytes && url_create && url_destroy;
  }
  std::string take_bytes(void* bytes) const {
    const long long size = bytes_size(bytes);
    const char* data = bytes_data(bytes);
    std::string result;
    if (size > 0 && size <= 4 * 1024 * 1024 && memory_readable(data, static_cast<size_t>(size))) {
      result.assign(data, static_cast<size_t>(size));
    }
    bytes_destroy(bytes);
    return result;
  }
  std::string read(const std::string& path) const {
    if (!ready()) {
      return {};
    }
    alignas(16) unsigned char name[64] = {}, locale[64] = {}, resource[256] = {}, bytes[64] = {};
    string_create(name, path.data(), static_cast<long long>(path.size()));
    locale_create(locale);
    resource_create(resource, name, locale);
    resource_bytes(resource, bytes);
    auto result = take_bytes(bytes);
    resource_destroy(resource);
    locale_destroy(locale);
    string_destroy(name);
    return result;
  }
};

const ExtensionManagerQmlApi& extension_manager_qml_api() {
  static const ExtensionManagerQmlApi api = [] {
    ExtensionManagerQmlApi value;
    const auto& lookup = resolve_qt_lookup();
    value.string_create = reinterpret_cast<decltype(value.string_create)>(lookup.from_utf8_proc);
    value.string_destroy = reinterpret_cast<decltype(value.string_destroy)>(lookup.qstring_destructor_proc);
    value.bytes_destroy = reinterpret_cast<decltype(value.bytes_destroy)>(lookup.qbytearray_destructor_proc);
    value.bytes_data = reinterpret_cast<decltype(value.bytes_data)>(lookup.qbytearray_const_data_proc);
    value.bytes_size = reinterpret_cast<decltype(value.bytes_size)>(lookup.qbytearray_size_proc);
    HMODULE core = GetModuleHandleW(L"Qt6Core.dll");
    value.string_bytes = reinterpret_cast<decltype(value.string_bytes)>(lookup.qstring_to_utf8_proc);
    value.locale_create = reinterpret_cast<decltype(value.locale_create)>(GetProcAddress(core, "??0QLocale@@QEAA@XZ"));
    value.locale_destroy = reinterpret_cast<decltype(value.locale_destroy)>(GetProcAddress(core, "??1QLocale@@QEAA@XZ"));
    value.resource_create = reinterpret_cast<decltype(value.resource_create)>(GetProcAddress(core, "??0QResource@@QEAA@AEBVQString@@AEBVQLocale@@@Z"));
    value.resource_destroy = reinterpret_cast<decltype(value.resource_destroy)>(GetProcAddress(core, "??1QResource@@QEAA@XZ"));
    value.resource_bytes = reinterpret_cast<decltype(value.resource_bytes)>(GetProcAddress(core, "?uncompressedData@QResource@@QEBA?AVQByteArray@@XZ"));
    value.iterator_create = reinterpret_cast<decltype(value.iterator_create)>(GetProcAddress(core, "??0QDirIterator@@QEAA@AEBVQString@@V?$QFlags@W4IteratorFlag@QDirIterator@@@@@Z"));
    value.iterator_destroy = reinterpret_cast<decltype(value.iterator_destroy)>(GetProcAddress(core, "??1QDirIterator@@QEAA@XZ"));
    value.iterator_has_next = reinterpret_cast<decltype(value.iterator_has_next)>(GetProcAddress(core, "?hasNext@QDirIterator@@QEBA_NXZ"));
    value.iterator_next = reinterpret_cast<decltype(value.iterator_next)>(GetProcAddress(core, "?next@QDirIterator@@QEAA?AVQString@@XZ"));
    value.register_data = reinterpret_cast<decltype(value.register_data)>(GetProcAddress(core, "?qRegisterResourceData@@YA_NHPEBE00@Z"));
    value.unregister_data = reinterpret_cast<decltype(value.unregister_data)>(GetProcAddress(core, "?qUnregisterResourceData@@YA_NHPEBE00@Z"));
    value.url_bytes = reinterpret_cast<decltype(value.url_bytes)>(GetProcAddress(core, "?toEncoded@QUrl@@QEBA?AVQByteArray@@V?$QUrlTwoFlags@W4UrlFormattingOption@QUrl@@W4ComponentFormattingOption@2@@@@Z"));
    value.url_create = reinterpret_cast<decltype(value.url_create)>(GetProcAddress(core, "??0QUrl@@QEAA@AEBVQString@@W4ParsingMode@0@@Z"));
    value.url_destroy = reinterpret_cast<decltype(value.url_destroy)>(GetProcAddress(core, "??1QUrl@@QEAA@XZ"));
    return value;
  }();
  return api;
}

struct ExtensionManagerQmlResources {
  std::vector<unsigned char> tree, names, data;
  bool registered = false;
  ~ExtensionManagerQmlResources() {
    if (registered && GetModuleHandleW(L"Qt6Core.dll")) {
      extension_manager_qml_api().unregister_data(1, tree.data(), names.data(), data.data());
    }
  }
};

std::unique_ptr<ExtensionManagerQmlResources> create_extension_manager_qml_resources() {
  const auto& api = extension_manager_qml_api();
  if (!api.ready() || api.read(":/dialogs/ExtensionManagerDialog.qml").empty()) {
    return {};
  }
  struct Node {
    std::string name, content;
    std::vector<size_t> children;
    unsigned int hash = 0;
    size_t index = 0;
    bool file = false;
  };
  std::vector<Node> nodes(1);
  alignas(16) unsigned char directory[64] = {}, iterator[256] = {};
  api.string_create(directory, ":/dialogs", 9);
  api.iterator_create(iterator, directory, 2);
  size_t count = 0, total = 0;
  size_t icon_index = 0;
  std::vector<std::string> ui_icons;
  bool complete = true;
  while (icon_index < std::size(k_extension_manager_ui_icons) || api.iterator_has_next(iterator)) {
    const bool control_icon = icon_index < std::size(k_extension_manager_ui_icons);
    std::string path;
    if (control_icon) {
      path = std::string(":/") + k_extension_manager_ui_icons[icon_index++];
    } else {
      alignas(16) unsigned char name[64] = {}, bytes[64] = {};
      api.iterator_next(iterator, name);
      api.string_bytes(name, bytes);
      path = api.take_bytes(bytes);
      api.string_destroy(name);
    }
    if (++count > 4096 || (!control_icon && path.rfind(":/dialogs/", 0) != 0) ||
        std::any_of(path.begin(), path.end(), [](unsigned char character) { return character >= 128; })) {
      complete = false;
      break;
    }
    std::string content = api.read(path);
    if (content.empty()) {
      continue;
    }
    if (control_icon) {
      content = extension_manager_icon_svg(path.substr(2), std::move(content));
      if (content.empty()) {
        continue;
      }
      ui_icons.push_back(path.substr(2));
    }
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".qml") == 0) {
      content = extension_manager_qml_source(std::move(content), ui_icons);
      if (content.empty()) {
        complete = false;
        break;
      }
    }
    total += content.size();
    if (total > 16 * 1024 * 1024) {
      complete = false;
      break;
    }
    const std::string private_path = "__darkmode_skp_extension_manager" + path.substr(1) + (control_icon ? ".svg" : "");
    size_t parent = 0, start = 0;
    while (start < private_path.size()) {
      const size_t slash = private_path.find('/', start);
      const std::string part = private_path.substr(start, slash == std::string::npos ? slash : slash - start);
      size_t child = nodes.size();
      for (size_t candidate : nodes[parent].children) {
        if (nodes[candidate].name == part) {
          child = candidate;
          break;
        }
      }
      if (child == nodes.size()) {
        nodes[parent].children.push_back(child);
        Node entry;
        entry.name = part;
        for (unsigned char character : part) {
          entry.hash = (entry.hash << 4) + character;
          entry.hash ^= (entry.hash & 0xf0000000u) >> 23;
          entry.hash &= 0x0fffffffu;
        }
        nodes.push_back(std::move(entry));
      }
      parent = child;
      if (slash == std::string::npos) {
        nodes[parent].file = true;
        nodes[parent].content = std::move(content);
        break;
      }
      start = slash + 1;
    }
  }
  api.iterator_destroy(iterator);
  api.string_destroy(directory);
  if (!complete || nodes.size() == 1) {
    return {};
  }
  std::vector<size_t> order{0};
  for (size_t position = 0; position < order.size(); ++position) {
    auto& children = nodes[order[position]].children;
    std::sort(children.begin(), children.end(), [&](size_t left, size_t right) { return nodes[left].hash < nodes[right].hash; });
    for (size_t child : children) {
      nodes[child].index = order.size();
      order.push_back(child);
    }
  }
  auto bundle = std::make_unique<ExtensionManagerQmlResources>();
  const auto append = [](std::vector<unsigned char>& bytes, size_t value, size_t width) {
    for (size_t remaining = width; remaining > 0; --remaining) {
      bytes.push_back(static_cast<unsigned char>(value >> ((remaining - 1) * 8)));
    }
  };
  for (size_t index : order) {
    const auto& node = nodes[index];
    append(bundle->tree, bundle->names.size(), 4);
    append(bundle->names, node.name.size(), 2);
    append(bundle->names, node.hash, 4);
    for (unsigned char character : node.name) {
      append(bundle->names, character, 2);
    }
    append(bundle->tree, node.file ? 0 : 2, 2);
    if (node.file) {
      append(bundle->tree, 0, 2);
      append(bundle->tree, 1, 2);
      append(bundle->tree, bundle->data.size(), 4);
      append(bundle->data, node.content.size(), 4);
      bundle->data.insert(bundle->data.end(), node.content.begin(), node.content.end());
    } else {
      append(bundle->tree, node.children.size(), 4);
      append(bundle->tree, node.children.empty() ? 0 : nodes[node.children.front()].index, 4);
    }
  }
  bundle->registered = api.register_data(1, bundle->tree.data(), bundle->names.data(), bundle->data.data());
  return bundle->registered ? std::move(bundle) : nullptr;
}

using ExtensionManagerQmlSetSource = void(__cdecl*)(void*, const void*);
ExtensionManagerQmlSetSource g_extension_manager_qml_set_source = nullptr;

void __cdecl extension_manager_qml_set_source_hook(void* view, const void* url) {
  const auto& api = extension_manager_qml_api();
  if (!g_extension_manager_qml_set_source) {
    return;
  }
  std::string source;
  if (api.ready() && url) {
    alignas(16) unsigned char bytes[64] = {};
    api.url_bytes(url, bytes, 0);
    source = api.take_bytes(bytes);
  }
  if (source != "qrc:/dialogs/ExtensionManagerDialog.qml" && source != "qrc:///dialogs/ExtensionManagerDialog.qml") {
    g_extension_manager_qml_set_source(view, url);
    return;
  }
  static auto resources = create_extension_manager_qml_resources();
  if (!resources) {
    g_extension_manager_qml_set_source(view, url);
    return;
  }
  const std::string target = "qrc:/__darkmode_skp_extension_manager/dialogs/ExtensionManagerDialog.qml";
  alignas(16) unsigned char name[64] = {}, themed_url[64] = {};
  api.string_create(name, target.data(), static_cast<long long>(target.size()));
  api.url_create(themed_url, name, 0);
  g_extension_manager_qml_set_source(view, themed_url);
  api.url_destroy(themed_url);
  api.string_destroy(name);
}

bool install_extension_manager_qml_hook() {
  if (g_extension_manager_qml_set_source) {
    return true;
  }
  if (find_vtables_for_type(".?AVExtensionManagerDialogQt@@").empty() || !extension_manager_qml_api().ready()) {
    return false;
  }
  constexpr const char* symbol = "?setSource@QQuickView@@QEAAXAEBVQUrl@@@Z";
  const auto original = reinterpret_cast<ExtensionManagerQmlSetSource>(
    GetProcAddress(GetModuleHandleW(L"Qt6Quick.dll"), symbol));
  const auto slots = find_main_import_iat_slots_for_symbols({symbol});
  if (!original || slots.size() != 1 || !memory_readable(slots.front(), sizeof(void*)) ||
      *slots.front() != reinterpret_cast<void*>(original)) {
    return false;
  }
  DWORD protection = 0;
  if (!VirtualProtect(slots.front(), sizeof(void*), PAGE_READWRITE, &protection)) {
    return false;
  }
  g_extension_manager_qml_set_source = original;
  *slots.front() = reinterpret_cast<void*>(&extension_manager_qml_set_source_hook);
  DWORD unused = 0;
  VirtualProtect(slots.front(), sizeof(void*), protection, &unused);
  return true;
}

const char* extension_manager_style_script(bool enabled) {
  return enabled
    ? R"DARKMODE((function(){
      if(!window.location||window.location.protocol!=='file:'||
          !/\/dialogs\/extensionmanager\/extension_manager\.html$/i.test(window.location.pathname)){return;}
      var key='__darkModeSkpExtensionManagerStyle';
      var root=document.head||document.documentElement;
      if(!root||document.getElementById(key)){return;}
      var style=document.createElement('style');
      style.id=key;
      style.textContent=
        'html,body{background-color:#2b2b2b!important;color:#d8dbe2!important;color-scheme:dark;}' +
        '.page,.extension-list .em-extension,.su-option-list li{background:#2b2b2b!important;color:#d8dbe2!important;border-color:#444!important;}' +
        '.footer,.page .em-header,.navbar-default,.modal-content,.modal-header,.modal-body,.modal-footer,.dropdown-menu,.panel,.panel-heading{background:#303030!important;color:#d8dbe2!important;border-color:#444!important;}' +
        '.extension-list .em-extension-body,.well,.panel-body,.su-option-list .su-option-list-description{background:#242424!important;color:#d8dbe2!important;border-color:#444!important;}' +
        '.extension-list .em-extension.selected .em-extension-header,.su-option-list li.selected .su-option-list-title{background:#4772b3!important;color:#fff!important;}' +
        '.extension-list .em-extension.selected .em-extension-header .glyphicon,.extension-list .em-extension.selected .em-extension-header .extension-signed,.extension-list .em-extension.selected .em-extension-header .su-thumb-selected,.su-option-list li.selected .su-option-list-title .glyphicon{color:#fff!important;}' +
        '.navbar-default .navbar-brand,.navbar-default .navbar-nav > li > a,.page .em-header a,.dropdown-menu > li > a,.close{color:#d8dbe2!important;}' +
        '#navbar .glyphicon,.page .em-header .glyphicon,.glyphicon-chevron-right,.glyphicon-chevron-down{color:#d8dbe2!important;}' +
        '#navbar .glyphicon-cog:before{color:#d8dbe2!important;text-shadow:none!important;}' +
        '.navbar-default .navbar-nav > .active > a,.navbar-default .navbar-nav > .active > a:hover,.navbar-default .navbar-nav > .active > a:focus,.navbar-default .navbar-nav > .open > a,.dropdown-menu > li > a:hover,.dropdown-menu > li > a:focus{background:#3b3b3b!important;color:#fff!important;}' +
        '.navbar-default .navbar-nav.su-tabs > li.active > a{border-bottom-color:#4772b3!important;}' +
        '.navbar-default .navbar-nav.su-tabs > li > a:hover{border-bottom-color:#5684c4!important;}' +
        '.footer{border-top-color:#4772b3!important;}' +
        'a,.su-url-external{color:#709ce6!important;}a:hover,a:focus,.su-url-external:hover{color:#98bdf0!important;}' +
        '.text-muted,.help-block{color:#a5abb5!important;}' +
        '.form-control,select,textarea,input[type="text"],input[type="search"]{background:#1f1f1f!important;color:#d8dbe2!important;border-color:#555!important;}' +
        '.form-control:focus{border-color:#709ce6!important;}' +
        '.btn-default,.toggle-handle{background:#383838!important;color:#d8dbe2!important;border-color:#5a5a5a!important;}' +
        '.btn-default:hover,.btn-default:focus,.btn-default:active,.btn-default.active{background:#484848!important;color:#fff!important;border-color:#777!important;}' +
        '.btn-primary{background:#4772b3!important;color:#fff!important;border-color:#4772b3!important;}' +
        '.btn-primary:hover,.btn-primary:focus{background:#5684c4!important;color:#fff!important;}' +
        '.btn-primary:active{background:#385b8f!important;color:#fff!important;}' +
        '.btn.toggle .toggle-on{background:#4772b3!important;color:#fff!important;}' +
        '.btn.toggle .toggle-off{background:#555!important;color:#fff!important;}' +
        '.btn.toggle .toggle-handle{background:#d8dbe2!important;border-color:#a5abb5!important;}' +
        '.btn.toggle{border-color:#777!important;}' +
        '.btn.disabled,.btn[disabled],fieldset[disabled] .btn,.form-control[disabled]{color:#a5abb5!important;}' +
        '.su-ui input[type="radio"]::before{background:#4772b3!important;}' +
        '.su-option-list .su-ui input[type="radio"]::before{background:#383838!important;color:#d8dbe2!important;}' +
        '.su-option-list .selected .su-ui > input[type="radio"]:before{background:#fff!important;color:#4772b3!important;}' +
        '.badge-info{background-color:#4772b3!important;color:#fff!important;}' +
        '.su-alert:not(.su-alert-success):not(.su-alert-danger){border-color:#4772b3!important;}' +
        '.su-dropzone{background:rgba(71,114,179,0.8)!important;}' +
        '.dropdown-menu .divider,hr{background-color:#444!important;border-color:#444!important;}' +
        '.extension-error-bag .extension-error-item,.alert-danger{background:#4a2d30!important;color:#ffc8c8!important;border-color:#805154!important;}' +
        '.alert-warning{background:#443d26!important;color:#f0d98c!important;border-color:#786738!important;}' +
        '.alert-info{background:#26344a!important;color:#c6d8f3!important;border-color:#4772b3!important;}' +
        '.alert-success{background:#2c4333!important;color:#bce2c7!important;border-color:#4f785b!important;}' +
        '::-webkit-scrollbar-track{background:#242424!important;}::-webkit-scrollbar-thumb{background:#606060!important;}';
      root.appendChild(style);
    })();)DARKMODE"
    : R"DARKMODE((function(){
      var style=document.getElementById('__darkModeSkpExtensionManagerStyle');
      if(style&&style.parentNode){style.parentNode.removeChild(style);}
    })();)DARKMODE";
}

bool execute_model_services_style(CefFrame* frame, bool enabled) {
  if (!frame || !cef_struct_has_member(&frame->base, offsetof(CefFrame, execute_java_script) + sizeof(frame->execute_java_script)) ||
      !frame->execute_java_script) {
    return false;
  }

  const std::string url = cef_frame_url_lower(frame);
  const bool extension_manager = is_extension_manager_url(url);
  if (!extension_manager && !is_model_services_url(url)) {
    return false;
  }
  const char* script = extension_manager ? extension_manager_style_script(enabled) : model_services_style_script(enabled);
  std::u16string script_utf16 = utf16_from_ascii(script);
  std::u16string source_utf16 = extension_manager ? u"dark-mode-skp://extension-manager" : u"dark-mode-skp://model-services";
  CefString script_string = {script_utf16.data(), script_utf16.size(), nullptr};
  CefString source_string = {source_utf16.data(), source_utf16.size(), nullptr};
  frame->execute_java_script(frame, &script_string, &source_string, 1);
  return true;
}

bool is_main_executable_code_address(const void* address, HMODULE executable = GetModuleHandleW(nullptr)) {
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

const unsigned char* resolve_model_services_bridge_thunk(const unsigned char* target, HMODULE executable) {
  for (size_t depth = 0; depth < 8; ++depth) {
    if (!is_main_executable_code_address(target, executable) || !memory_readable(target, 5)) {
      return nullptr;
    }
    if (target[0] != 0xe9) {
      return target;
    }
    std::int32_t displacement = 0;
    std::memcpy(&displacement, target + 1, sizeof(displacement));
    target = reinterpret_cast<const unsigned char*>(reinterpret_cast<std::intptr_t>(target + 5) + displacement);
  }
  return nullptr;
}

const unsigned char* find_forwarded_model_services_script_bridge(
  const unsigned char* instruction,
  const unsigned char* function_end,
  HMODULE executable
) {
  if (!instruction || !function_end || function_end - instruction < 13) {
    return nullptr;
  }
  size_t lea_size = 0;
  if (instruction[0] == 0x48 && instruction[1] == 0x8d && instruction[2] == 0x54 && instruction[3] == 0x24) {
    lea_size = 5;
  } else if (instruction[0] == 0x48 && instruction[1] == 0x8d && instruction[2] == 0x94 && instruction[3] == 0x24) {
    lea_size = 8;
  } else {
    return nullptr;
  }
  const auto* receiver = instruction + lea_size;
  if (function_end - receiver < 8 || receiver[0] != 0x48 || receiver[1] != 0x8b ||
      receiver[2] != 0xce || receiver[3] != 0xe8) {
    return nullptr;
  }
  std::int32_t displacement = 0;
  std::memcpy(&displacement, receiver + 4, sizeof(displacement));
  const auto* wrapper = resolve_model_services_bridge_thunk(
    reinterpret_cast<const unsigned char*>(reinterpret_cast<std::intptr_t>(receiver + 8) + displacement), executable);
  static constexpr unsigned char prefix[] = {0x48, 0x8b, 0x49, 0x08, 0x48, 0x85, 0xc9, 0x0f, 0x85};
  if (!wrapper || !memory_readable(wrapper, 14) ||
      !std::equal(std::begin(prefix), std::end(prefix), wrapper) || wrapper[13] != 0xc3) {
    return nullptr;
  }
  std::memcpy(&displacement, wrapper + 9, sizeof(displacement));
  return resolve_model_services_bridge_thunk(
    reinterpret_cast<const unsigned char*>(reinterpret_cast<std::intptr_t>(wrapper + 13) + displacement), executable);
}

void* find_atlast_webview_javascript_bridge(HMODULE executable = GetModuleHandleW(nullptr)) {
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
      const unsigned char* candidate = nullptr;
      if (call) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, call + 1, sizeof(displacement));
        candidate = reinterpret_cast<const unsigned char*>(reinterpret_cast<std::intptr_t>(call + 5) + displacement);
      } else {
        candidate = find_forwarded_model_services_script_bridge(instruction, function_end, executable);
      }
      if (!is_main_executable_code_address(candidate, executable)) {
        continue;
      }
      bridge = const_cast<unsigned char*>(candidate);
      matches += 1;
    }
  }

  return matches == 1 ? bridge : nullptr;
}

std::vector<const void*> extension_manager_dialog_vtables(HMODULE executable = GetModuleHandleW(nullptr)) {
  std::vector<const void*> tables;
  for (const char* type : {".?AVExtensionManagerDialog@@", ".?AVExtensionManagerDialogQt@@"}) {
    const auto found = find_vtables_for_type(type, executable);
    tables.insert(tables.end(), found.begin(), found.end());
  }
  return tables;
}

std::vector<void*> find_model_services_atlast_webviews(const QtLookup& lookup,
  const std::vector<const void*>& dialog_types = find_vtables_for_type(".?AVCModelServiceDialog@@"),
  const std::vector<const void*>& webview_vtables = find_vtables_for_type(".?AVWebView@webengine@atlast@@")) {
  constexpr size_t object_scan_bytes = 0x200;
  std::vector<void*> webviews;
  const std::vector<void*> dialogs = find_overlays_dialog_widgets(lookup, dialog_types);
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

size_t apply_extension_manager_style_to_live_webviews(const QtLookup& lookup, bool enabled) {
  const auto webviews = find_model_services_atlast_webviews(lookup, extension_manager_dialog_vtables());
  if (webviews.empty()) {
    return 0;
  }
  static void* bridge = find_atlast_webview_javascript_bridge();
  const auto script = utf16_from_ascii(extension_manager_style_script(enabled));
  size_t applied = 0;
  for (void* webview : webviews) {
    applied += execute_atlast_webview_javascript(bridge, webview, script) ? 1 : 0;
  }
  return applied;
}

void prune_model_services_frames() {
  std::vector<CefFrame*> expired;
  {
    std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
    auto frame = g_model_services_frames.begin();
    while (frame != g_model_services_frames.end()) {
      CefFrame* current = *frame;
      const bool valid = current && cef_struct_has_member(&current->base, offsetof(CefFrame, is_valid) + sizeof(current->is_valid)) &&
        current->is_valid && reinterpret_cast<int(__fastcall*)(CefFrame*)>(current->is_valid)(current) != 0;
      if (valid) {
        ++frame;
      } else {
        expired.push_back(current);
        frame = g_model_services_frames.erase(frame);
      }
    }
  }
  for (CefFrame* frame : expired) {
    if (frame && cef_struct_has_member(&frame->base, sizeof(CefBaseRefCounted)) && frame->base.release) {
      frame->base.release(&frame->base);
    }
  }
}

void track_model_services_frame(CefFrame* frame) {
  if (!frame || !cef_struct_has_member(&frame->base, sizeof(CefBaseRefCounted)) || !frame->base.add_ref || !frame->base.release) {
    return;
  }

  prune_model_services_frames();
  std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
  if (std::find(g_model_services_frames.begin(), g_model_services_frames.end(), frame) != g_model_services_frames.end()) {
    return;
  }
  frame->base.add_ref(&frame->base);
  g_model_services_frames.push_back(frame);
}

size_t apply_model_services_style_to_tracked_frames(bool enabled) {
  prune_model_services_frames();
  std::vector<CefFrame*> frames;
  {
    std::lock_guard<std::mutex> lock(g_model_services_cef_mutex);
    frames = g_model_services_frames;
    for (CefFrame* frame : frames) {
      frame->base.add_ref(&frame->base);
    }
  }
  size_t applied = 0;
  for (CefFrame* frame : frames) {
    applied += execute_model_services_style(frame, enabled) ? 1 : 0;
    frame->base.release(&frame->base);
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
  if (!main_frame || (!is_model_services_url(frame_url) && !is_extension_manager_url(frame_url))) {
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
  const auto release_handler = [](CefLoadHandler* value) {
    if (value && cef_struct_has_member(&value->base, sizeof(CefBaseRefCounted)) && value->base.release) {
      value->base.release(&value->base);
    }
  };
  std::unique_ptr<CefLoadHandler, decltype(release_handler)> reference(handler, release_handler);
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
  static_cast<void>(reference.release());
  return true;
}

struct ExtensionManagerLoadHandler {
  CefLoadHandler api;
  std::atomic<int> references{1};
  CefLoadHandler* original = nullptr;

  static ExtensionManagerLoadHandler* self(CefLoadHandler* handler) {
    return reinterpret_cast<ExtensionManagerLoadHandler*>(handler);
  }
  static void __fastcall add_ref(CefBaseRefCounted* base) {
    reinterpret_cast<ExtensionManagerLoadHandler*>(base)->references.fetch_add(1, std::memory_order_relaxed);
  }
  static int __fastcall release(CefBaseRefCounted* base) {
    auto* handler = reinterpret_cast<ExtensionManagerLoadHandler*>(base);
    if (handler->references.fetch_sub(1, std::memory_order_acq_rel) != 1) {
      return 0;
    }
    if (handler->original) {
      handler->original->base.release(&handler->original->base);
    }
    delete handler;
    return 1;
  }
  static int __fastcall has_one_ref(CefBaseRefCounted* base) {
    return reinterpret_cast<ExtensionManagerLoadHandler*>(base)->references.load(std::memory_order_acquire) == 1;
  }
  static int __fastcall has_at_least_one_ref(CefBaseRefCounted* base) {
    return reinterpret_cast<ExtensionManagerLoadHandler*>(base)->references.load(std::memory_order_acquire) > 0;
  }
  static void __fastcall loading(CefLoadHandler* handler, void* browser, int busy, int back, int forward) {
    auto* original = self(handler)->original;
    if (original && original->on_loading_state_change) {
      reinterpret_cast<void(__fastcall*)(CefLoadHandler*, void*, int, int, int)>(original->on_loading_state_change)(
        original, browser, busy, back, forward);
    }
  }
  static void __fastcall start(CefLoadHandler* handler, void* browser, CefFrame* frame, int transition) {
    auto* original = self(handler)->original;
    if (original && original->on_load_start) {
      reinterpret_cast<void(__fastcall*)(CefLoadHandler*, void*, CefFrame*, int)>(original->on_load_start)(
        original, browser, frame, transition);
    }
  }
  static void __fastcall end(CefLoadHandler* handler, void* browser, CefFrame* frame, int status) {
    auto* original = self(handler)->original;
    if (original && original->on_load_end) {
      original->on_load_end(original, browser, frame, status);
    }
    if (!frame || !cef_struct_has_member(&frame->base, offsetof(CefFrame, get_url) + sizeof(frame->get_url)) ||
        !frame->is_main || !frame->is_main(frame) || !frame->is_valid ||
        !reinterpret_cast<int(__fastcall*)(CefFrame*)>(frame->is_valid)(frame) ||
        !is_extension_manager_url(cef_frame_url_lower(frame))) {
      return;
    }
    track_model_services_frame(frame);
    if (InterlockedCompareExchange(&g_model_services_cef_observer_only, 0, 0) == 0) {
      execute_model_services_style(frame, InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) != 0);
    }
  }
  static void __fastcall error(CefLoadHandler* handler, void* browser, CefFrame* frame, int code,
                                const CefString* message, const CefString* url) {
    auto* original = self(handler)->original;
    if (original && original->on_load_error) {
      reinterpret_cast<void(__fastcall*)(CefLoadHandler*, void*, CefFrame*, int, const CefString*, const CefString*)>(
        original->on_load_error)(original, browser, frame, code, message, url);
    }
  }
};

struct ExtensionManagerClientHook {
  CefLoadHandler*(__fastcall* getter)(CefClient*) = nullptr;
  int(__fastcall* release)(CefBaseRefCounted*) = nullptr;
};
std::mutex g_extension_manager_clients_mutex;
std::unordered_map<CefClient*, std::shared_ptr<ExtensionManagerClientHook>> g_extension_manager_clients;

int __fastcall extension_manager_client_release(CefBaseRefCounted* base) {
  auto* client = reinterpret_cast<CefClient*>(base);
  std::shared_ptr<ExtensionManagerClientHook> hook;
  {
    std::lock_guard<std::mutex> lock(g_extension_manager_clients_mutex);
    const auto found = g_extension_manager_clients.find(client);
    if (found == g_extension_manager_clients.end()) {
      return 0;
    }
    hook = found->second;
  }
  const int destroyed = hook->release(base);
  if (destroyed) {
    std::lock_guard<std::mutex> lock(g_extension_manager_clients_mutex);
    const auto found = g_extension_manager_clients.find(client);
    if (found != g_extension_manager_clients.end() && found->second == hook) {
      g_extension_manager_clients.erase(found);
    }
  }
  return destroyed;
}

CefLoadHandler* __fastcall extension_manager_get_load_handler(CefClient* client) {
  std::shared_ptr<ExtensionManagerClientHook> hook;
  {
    std::lock_guard<std::mutex> lock(g_extension_manager_clients_mutex);
    const auto found = g_extension_manager_clients.find(client);
    if (found == g_extension_manager_clients.end()) {
      return nullptr;
    }
    hook = found->second;
  }
  CefLoadHandler* original = hook->getter ? hook->getter(client) : nullptr;
  if (original && (!cef_struct_has_member(&original->base, sizeof(CefLoadHandler)) || !original->base.release)) {
    return original;
  }
  auto* handler = new (std::nothrow) ExtensionManagerLoadHandler;
  if (!handler) {
    return original;
  }
  handler->original = original;
  handler->api.base = {sizeof(CefLoadHandler), ExtensionManagerLoadHandler::add_ref, ExtensionManagerLoadHandler::release,
    ExtensionManagerLoadHandler::has_one_ref, ExtensionManagerLoadHandler::has_at_least_one_ref};
  handler->api.on_loading_state_change = reinterpret_cast<void*>(&ExtensionManagerLoadHandler::loading);
  handler->api.on_load_start = reinterpret_cast<void*>(&ExtensionManagerLoadHandler::start);
  handler->api.on_load_end = &ExtensionManagerLoadHandler::end;
  handler->api.on_load_error = reinterpret_cast<void*>(&ExtensionManagerLoadHandler::error);
  return &handler->api;
}

bool hook_extension_manager_client(CefClient* client) {
  if (!client || !cef_struct_has_member(&client->base, sizeof(CefClient)) || !client->base.release) {
    return false;
  }
  std::lock_guard<std::mutex> lock(g_extension_manager_clients_mutex);
  if (g_extension_manager_clients.find(client) != g_extension_manager_clients.end()) {
    return client->get_load_handler == &extension_manager_get_load_handler;
  }
  auto hook = std::make_shared<ExtensionManagerClientHook>();
  hook->getter = client->get_load_handler;
  hook->release = client->base.release;
  DWORD protection = 0;
  if (!VirtualProtect(client, sizeof(CefClient), PAGE_READWRITE, &protection)) {
    return false;
  }
  g_extension_manager_clients.emplace(client, hook);
  client->get_load_handler = &extension_manager_get_load_handler;
  client->base.release = &extension_manager_client_release;
  DWORD unused = 0;
  VirtualProtect(client, sizeof(CefClient), protection, &unused);
  return true;
}

void** find_cef_import_iat_slot(const char* target_function_name, HMODULE executable = GetModuleHandleW(nullptr)) {
  if (!target_function_name || target_function_name[0] == '\0') {
    return nullptr;
  }
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

void* __cdecl qmenu_exec_hook(void* menu, const void* position, void* action) {
  QMenuExecFn original_proc = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_qmenu_exec_mutex);
    original_proc = g_original_qmenu_exec;
  }

  if (InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) != 0 && !g_qmenu_exec_styling) {
    g_qmenu_exec_styling = true;
    static_cast<void>(apply_qmenu_hover_stylesheet(menu));
    g_qmenu_exec_styling = false;
  }
  return original_proc ? original_proc(menu, position, action) : nullptr;
}

bool install_qmenu_exec_hook() {
  std::lock_guard<std::mutex> lock(g_qmenu_exec_mutex);
  if (g_original_qmenu_exec && !g_qmenu_exec_iat_slots.empty()) {
    return true;
  }

  const std::vector<void**> slots = find_main_import_iat_slots_for_symbols({
    "?exec@QMenu@@QEAAPEAVQAction@@AEBVQPoint@@PEAV2@@Z"
  });
  if (slots.empty() || !memory_readable(slots.front(), sizeof(*slots.front())) || !*slots.front()) {
    return false;
  }

  const QMenuExecFn original_proc = reinterpret_cast<QMenuExecFn>(*slots.front());
  std::vector<void**> installed_slots;
  for (void** slot : slots) {
    if (!slot || !memory_readable(slot, sizeof(*slot)) || !*slot) {
      continue;
    }
    DWORD original_protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
      continue;
    }
    *slot = reinterpret_cast<void*>(&qmenu_exec_hook);
    DWORD unused_protection = 0;
    VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
    installed_slots.push_back(slot);
  }
  if (installed_slots.empty()) {
    return false;
  }

  g_original_qmenu_exec = original_proc;
  g_qmenu_exec_iat_slots = installed_slots;
  return true;
}

void __cdecl qmenu_popup_hook(void* menu, const void* position, void* action) {
  QMenuPopupFn original_proc = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_qmenu_popup_mutex);
    original_proc = g_original_qmenu_popup;
  }

  if (InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) != 0 && menu) {
    const QtLookup& lookup = resolve_qt_lookup();
    static_cast<void>(apply_non_customizable_toolbar_hover_palette_to_widget(lookup, menu, true));
  }

  if (original_proc) {
    original_proc(menu, position, action);
  }
}

bool install_qmenu_popup_hook() {
  std::lock_guard<std::mutex> lock(g_qmenu_popup_mutex);
  if (g_original_qmenu_popup && !g_qmenu_popup_iat_slots.empty()) {
    return true;
  }

  const std::vector<void**> slots = find_main_import_iat_slots_for_symbols({
    "?popup@QMenu@@QEAAXAEBVQPoint@@PEAVQAction@@@Z"
  });
  if (slots.empty() || !memory_readable(slots.front(), sizeof(*slots.front())) || !*slots.front()) {
    return false;
  }

  const QMenuPopupFn original_proc = reinterpret_cast<QMenuPopupFn>(*slots.front());
  std::vector<void**> installed_slots;
  for (void** slot : slots) {
    if (!slot || !memory_readable(slot, sizeof(*slot)) || !*slot) {
      continue;
    }
    DWORD original_protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
      continue;
    }
    *slot = reinterpret_cast<void*>(&qmenu_popup_hook);
    DWORD unused_protection = 0;
    VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);
    installed_slots.push_back(slot);
  }
  if (installed_slots.empty()) {
    return false;
  }

  g_original_qmenu_popup = original_proc;
  g_qmenu_popup_iat_slots = installed_slots;
  return true;
}

bool spin_box_palette_event(const void* event) {
  static const auto event_type = reinterpret_cast<int(__cdecl*)(const void*)>(
    GetProcAddress(GetModuleHandleW(L"Qt6Core.dll"), "?type@QEvent@@QEBA?AW4Type@1@XZ"));
  if (!event || !event_type) {
    return true;
  }
  const int type = event_type(event);
  return type == 38 || type == 39 || type == 98 || type == 100 || type == 210;
}

void __fastcall focus_returner_double_spin_box_change_event_hook(void* spin_box, void* event) {
  QWidgetChangeEventFn original_proc = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_focus_returner_double_spin_box_change_event_mutex);
    original_proc = g_original_focus_returner_double_spin_box_change_event;
  }

  if (original_proc) {
    original_proc(spin_box, event);
  }

  if (!spin_box || InterlockedCompareExchange(&g_dark_mode_enabled, 0, 0) == 0 ||
      g_focus_returner_double_spin_box_palette_applying || !spin_box_palette_event(event)) {
    return;
  }

  g_focus_returner_double_spin_box_palette_applying = true;
  const QtLookup& lookup = resolve_qt_lookup();
  if (!widget_theme_stamp(lookup, spin_box, "_darkmode_spin_palette", false) &&
      apply_clicked_palette_to_widget(lookup, spin_box, true)) {
    widget_theme_stamp(lookup, spin_box, "_darkmode_spin_palette", true);
  }
  g_focus_returner_double_spin_box_palette_applying = false;
}

bool install_focus_returner_double_spin_box_change_event_hook() {
  std::lock_guard<std::mutex> lock(g_focus_returner_double_spin_box_change_event_mutex);
  if (g_original_focus_returner_double_spin_box_change_event &&
      g_focus_returner_double_spin_box_change_event_slot) {
    return true;
  }

  const void* table = primary_rtti_vtable(find_vtables_for_type(".?AVQtFocusReturnerDoubleSpinBox@@"));
  constexpr size_t change_event_slot = 41;
  void** slot = memory_readable(table, (change_event_slot + 1) * sizeof(void*))
    ? const_cast<void**>(static_cast<void* const*>(table)) + change_event_slot : nullptr;
  if (!slot || !memory_readable(slot, sizeof(*slot)) || !*slot) {
    return false;
  }

  const QWidgetChangeEventFn original_proc = reinterpret_cast<QWidgetChangeEventFn>(*slot);
  DWORD original_protection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &original_protection)) {
    return false;
  }
  *slot = reinterpret_cast<void*>(&focus_returner_double_spin_box_change_event_hook);
  DWORD unused_protection = 0;
  VirtualProtect(slot, sizeof(*slot), original_protection, &unused_protection);

  g_original_focus_returner_double_spin_box_change_event = original_proc;
  g_focus_returner_double_spin_box_change_event_slot = slot;
  return true;
}

size_t apply_scene_tab_search_dialog_palette(void* dialog) {
  const void* expected_vtable = scene_tab_search_dialog_vtable();
  if (!pointer_looks_like_cpp_object(dialog) || !expected_vtable ||
      *reinterpret_cast<const void* const*>(dialog) != expected_vtable) {
    return 0;
  }

  const QtLookup& lookup = resolve_qt_lookup();
  if (!lookup.app_all_widgets_proc || !lookup.object_parent_proc) {
    return 0;
  }

  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));

  size_t applied_count = 0;
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget) || !qt_object_inherits(lookup, widget, "QListWidget") ||
        !qt_object_has_ancestor_vtable(lookup, widget, expected_vtable)) {
      continue;
    }
    if (!apply_non_customizable_toolbar_hover_palette_to_widget(lookup, widget, true)) {
      continue;
    }
    if (lookup.widget_update_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    }
    if (lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    }
    applied_count += 1;
  }
  return applied_count;
}

size_t apply_spin_box_palette(const QtLookup& lookup, bool dark) {
  if (!lookup.app_all_widgets_proc || !lookup.widget_update_proc || !lookup.widget_repaint_proc) {
    return 0;
  }

  using QtWidgetNoArgFn = void(__cdecl*)(void*);
  std::vector<void*> widgets;
  static_cast<void>(dump_qwidget_list_return(lookup.app_all_widgets_proc, widgets, 4096));

  size_t applied_count = 0;
  for (void* widget : widgets) {
    if (!pointer_looks_like_cpp_object(widget)) {
      continue;
    }

    const bool is_spin_box = qt_object_inherits(lookup, widget, "QAbstractSpinBox");
    const bool is_spin_box_line_edit = qt_object_inherits(lookup, widget, "QLineEdit") &&
      qt_object_has_ancestor_class(lookup, widget, "QAbstractSpinBox");
    if (!is_spin_box && !is_spin_box_line_edit) {
      continue;
    }

    if (!apply_clicked_palette_to_widget(lookup, widget, dark)) {
      continue;
    }
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(widget);
    reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(widget);
    applied_count += 1;
  }
  return applied_count;
}

int __fastcall cef_browser_host_create_browser_hook(
  const void* window_info,
  CefClient* client,
  const CefString* url,
  const void* settings,
  void* extra_info,
  void* request_context
) {
  static_cast<void>(hook_extension_manager_client(client));
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
  static_cast<void>(hook_extension_manager_client(client));
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
  static_cast<void>(hook_extension_manager_client(client));
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

  const QtLookup& lookup = resolve_qt_lookup();

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
    if (lookup.widget_update_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_update_proc)(targets[index]);
    } else if (lookup.widget_repaint_proc) {
      reinterpret_cast<QtWidgetNoArgFn>(lookup.widget_repaint_proc)(targets[index]);
    }
  }

  const std::vector<HWND> tray_roots = find_tray_marker_hwnds();
  std::unordered_set<HWND> redraw_roots;
  for (HWND tray_root : tray_roots) {
    const HWND root = GetAncestor(tray_root, GA_ROOT);
    if (redraw_roots.insert(root ? root : tray_root).second) {
      RedrawWindow(root ? root : tray_root, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
  }

  set_theme_info("Dark Mode SKP helper stage " + std::to_string(stage) + " OK: auto palette applied=" + std::to_string(applied_count) + " separator=" + std::to_string(separator_applied) + " header=" + std::to_string(header_applied) + " VCB=" + std::to_string(vcb_edit_count) + " targets=" + std::to_string(targets.size()));
  return applied_count > 0;
}

bool call_qt_stylesheet(bool enabled) {
  g_scene_search_dark = enabled;
  const int palette_stage = enabled ? 45 : 46;
  const int windows_stage = enabled ? 47 : 48;
  const QtLookup& tags_header_lookup = resolve_qt_lookup();
  if (enabled) {
    prepare_dialog_show_types();
  }
  const bool workspace_style_hook_ok = set_modern_workspace_style_hook(tags_header_lookup, enabled);
  const bool qt_color_scheme_ok = apply_qt_color_scheme(tags_header_lookup, enabled);
  size_t tag_header_color_count = 0;
  size_t entity_info_input_surface_count = 0;
  if (!enabled) {
    remove_scene_search_label_hooks();
    remove_non_customizable_toolbar_title_hooks();
    InterlockedExchange(&g_model_services_cef_observer_only, 1);
    tag_header_color_count = apply_tag_dialog_header_colors(tags_header_lookup, false, 66);
  }
  const bool tag_header_paint_hook_ok = install_tag_header_paint_hook(enabled);
  static_cast<void>(set_tag_eye_icon_hook(tags_header_lookup, enabled));
  static_cast<void>(set_about_image_hook(tags_header_lookup, enabled));
  static_cast<void>(set_legacy_material_image_hook(tags_header_lookup, enabled));
  const bool component_text_hook_ok = install_content_browser_text_hooks(enabled);
  static_cast<void>(set_content_browser_search_icon_hook(tags_header_lookup, enabled));
  const bool outliner_text_hook_ok = install_component_navigator_paint_hook(enabled);
  const bool vcb_ok = set_vcb_typed_input_background(!enabled);

  const bool palette_ok = probe_qt_stage(palette_stage);
  const size_t tab_bar_palette_count = palette_ok ?
    apply_tab_bar_palette(resolve_qt_lookup(), enabled) : 0;
  const NonCustomizableToolbarStyleStats toolbar_style_stats = palette_ok ?
    apply_non_customizable_toolbar_flat_border_palette(resolve_qt_lookup(), enabled) : NonCustomizableToolbarStyleStats{};
  const size_t toolbar_hover_palette_count = palette_ok ?
    apply_non_customizable_toolbar_hover_palette(resolve_qt_lookup(), enabled) : 0;
  const size_t spin_box_palette_count = palette_ok ?
    apply_spin_box_palette(resolve_qt_lookup(), enabled) : 0;
  const bool vcb_paint_hook_ok = install_vcb_status_paint_hooks(resolve_qt_lookup(), enabled);
  flush_qt_pending_events(resolve_qt_lookup());
  const bool windows_ok = probe_qt_stage(windows_stage);
  const size_t floating_toolbar_caption_count = enabled && windows_ok ?
    apply_existing_floating_toolbar_dark_captions(tags_header_lookup) : 0;
  if (enabled && palette_ok && windows_ok) {
    tag_header_color_count = apply_tag_dialog_header_colors(tags_header_lookup, true, 64);
  }
  if (palette_ok && windows_ok) {
    entity_info_input_surface_count = apply_entity_info_input_surface(tags_header_lookup, enabled);
  }
  static_cast<void>(apply_existing_omnibar_dialog_styles(tags_header_lookup, enabled));
  static_cast<void>(apply_existing_notification_styles(tags_header_lookup, enabled));
  const size_t workspace_style_count = apply_modern_workspace_styles(tags_header_lookup, enabled);
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
  const size_t extension_manager_view_count = apply_extension_manager_style_to_live_webviews(tags_header_lookup, enabled);
  if (applied) {
    InterlockedExchange(&g_model_services_cef_observer_only, 0);
    model_services_frame_count = apply_model_services_style_to_tracked_frames(enabled);
    about_sketchup_palette_count = apply_about_sketchup_palette(enabled) ? 1 : 0;
  }
  const size_t page_tabs_search_icon_count =
    enabled && applied &&
    refresh_page_tabs_search_icon(tags_header_lookup) ? 1 : 0;
  if (applied) {
    static_cast<void>(refresh_content_browser_filter_icons(tags_header_lookup));
  }
  if (!enabled) {
    InterlockedExchange(&g_icon_theme_page_tabs_search_icon_count, 0);
  }
  if (page_tabs_search_icon_count > 0) {
    InterlockedExchange(&g_icon_theme_page_tabs_search_icon_count, static_cast<LONG>(page_tabs_search_icon_count));
  }
  set_theme_info("Dark Mode SKP helper main " + std::string(enabled ? "dark" : "clear") +
    ": palette=" + std::to_string(palette_ok ? 1 : 0) +
    " windows=" + std::to_string(windows_ok ? 1 : 0) +
    " qt-color-scheme=" + std::to_string(qt_color_scheme_ok ? 1 : 0) +
    " workspace-style-hook=" + std::to_string(workspace_style_hook_ok ? 1 : 0) +
    " workspace-styles=" + std::to_string(workspace_style_count) +
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
    " extension-manager-live-view=" + std::to_string(extension_manager_view_count) +
    " about=" + std::to_string(about_sketchup_palette_count) +
    " tabs=" + std::to_string(tab_bar_palette_count) +
    " toolbar-flat-border=" + std::to_string(toolbar_style_stats.surface_style_count) +
    " toolbar-floating=" + std::to_string(toolbar_style_stats.floating_count) +
    " toolbar-title-paint=" + std::to_string(toolbar_style_stats.title_paint_count) +
    " toolbar-hover=" + std::to_string(toolbar_hover_palette_count) +
    " toolbar-captions=" + std::to_string(floating_toolbar_caption_count) +
    " spin-boxes=" + std::to_string(spin_box_palette_count) +
    " entity-info-inputs=" + std::to_string(entity_info_input_surface_count) +
    " tags-header=" + std::to_string(tag_header_color_count) +
    " tags-paint=" + std::to_string(tag_header_paint_hook_ok ? 1 : 0) +
    " components-text=" + std::to_string(component_text_hook_ok ? 1 : 0) +
    " outliner-text=" + std::to_string(outliner_text_hook_ok ? 1 : 0) +
    " measurements=" + std::to_string(measurements_entry_pad_count) +
    " page-tabs-search-icon=" + std::to_string(page_tabs_search_icon_count));
  return applied;
}

} // namespace

extern "C" __declspec(dllexport) int darkmode_skp_initialize() {
  InterlockedExchange(&g_model_services_cef_observer_only, 1);
  const bool model_services_observer_installed = install_model_services_cef_hook();
  InterlockedExchange(&g_model_services_cef_hook_installed, model_services_observer_installed ? 1 : 0);
  const bool about_observer_installed = install_about_sketchup_window_hook();
  const bool qmenu_observer_installed = install_qmenu_window_hook();
  static_cast<void>(install_extension_manager_qml_hook());
  static_cast<void>(install_focus_returner_double_spin_box_change_event_hook());
  static_cast<void>(install_tray_hide_icon_add_file_hook());
  static_cast<void>(install_page_tabs_search_refresh_hook());
  return about_observer_installed && qmenu_observer_installed ? 1 : 0;
}

extern "C" __declspec(dllexport) int darkmode_skp_prepare_icon_theme(int enabled) {
  return configure_icon_theme(enabled != 0) ? 1 : 0;
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

  const std::string info = theme_info() +
    " workspace-dark=" + std::to_string(g_modern_workspace_dark ? 1 : 0) +
    " workspace-frame-module=" + std::to_string(GetModuleHandleW(L"kddockwidgets-qt6.dll") ? 1 : 0) +
    " workspace-frame-hook=" + std::to_string(g_modern_workspace_frame_hook.slot &&
      *g_modern_workspace_frame_hook.slot == reinterpret_cast<void*>(&modern_workspace_frame_pen_hook) ? 1 : 0) +
    " workspace-frame-calls=" + std::to_string(InterlockedCompareExchange(&g_modern_workspace_frame_calls, 0, 0)) +
    " workspace-frame-tints=" + std::to_string(InterlockedCompareExchange(&g_modern_workspace_frame_tints, 0, 0)) +
    " workspace-title-draws=" + std::to_string(InterlockedCompareExchange(&g_modern_workspace_title_draws, 0, 0)) +
    " workspace-tab-draws=" + std::to_string(InterlockedCompareExchange(&g_modern_workspace_tab_draws, 0, 0)) +
    " toolbar-title-draws=" + std::to_string(InterlockedCompareExchange(&g_toolbar_title_draw_count, 0, 0)) +
    " toolbar-close-draws=" + std::to_string(InterlockedCompareExchange(&g_toolbar_close_draw_count, 0, 0)) +
    " scene-search-draws=" + std::to_string(InterlockedCompareExchange(&g_scene_search_draw_count, 0, 0));
  const size_t copy_len = std::min(static_cast<size_t>(buffer_len - 1), info.size());
  if (copy_len > 0) {
    std::memcpy(buffer, info.data(), copy_len);
  }
  buffer[copy_len] = '\0';
  return static_cast<int>(copy_len);
}
