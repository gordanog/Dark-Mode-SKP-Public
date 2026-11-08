// Development-only probe. Never package this build as the release helper.
// Override DARKMODE_SOURCE_PATH to compare an older source revision.
#ifndef DARKMODE_SOURCE_PATH
#define DARKMODE_SOURCE_PATH "src/dark_mode_skp.cpp"
#endif
#include DARKMODE_SOURCE_PATH

#include <chrono>

namespace {
struct CallbackTiming {
  unsigned long long calls = 0;
  double total_ms = 0;
  double max_ms = 0;
};
CallbackTiming g_probe_about;
CallbackTiming g_probe_menu;
CallbackTiming g_probe_icon;
int g_probe_mode = 0;

void time_window_callback(WINEVENTPROC callback, CallbackTiming& timing,
    HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object_id, LONG child_id,
    DWORD event_thread, DWORD event_time) {
  const auto start = std::chrono::steady_clock::now();
  callback(hook, event, hwnd, object_id, child_id, event_thread, event_time);
  const double elapsed = std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - start).count();
  ++timing.calls;
  timing.total_ms += elapsed;
  timing.max_ms = std::max(timing.max_ms, elapsed);
}

void CALLBACK probe_about(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object_id,
    LONG child_id, DWORD event_thread, DWORD event_time) {
  time_window_callback(about_sketchup_window_event, g_probe_about,
    hook, event, hwnd, object_id, child_id, event_thread, event_time);
}
void CALLBACK probe_menu(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object_id,
    LONG child_id, DWORD event_thread, DWORD event_time) {
  time_window_callback(qmenu_window_event, g_probe_menu,
    hook, event, hwnd, object_id, child_id, event_thread, event_time);
}
void CALLBACK probe_icon(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object_id,
    LONG child_id, DWORD event_thread, DWORD event_time) {
  if (g_probe_mode != 1) {
    time_window_callback(page_tabs_search_refresh_window_event, g_probe_icon,
      hook, event, hwnd, object_id, child_id, event_thread, event_time);
  }
}

bool replace_probe_hook(HWINEVENTHOOK& hook, WINEVENTPROC callback) {
  if (hook && !UnhookWinEvent(hook)) {
    return false;
  }
  hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, callback,
    GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
  return hook != nullptr;
}
} // namespace

extern "C" __declspec(dllexport) int darkmode_skp_perf_begin(int mode) {
  g_probe_mode = mode;
  g_probe_about = {};
  g_probe_menu = {};
  g_probe_icon = {};
  const bool about_ok = replace_probe_hook(g_about_sketchup_window_hook, probe_about);
  const bool menu_ok = replace_probe_hook(g_qmenu_window_hook, probe_menu);
  const bool icon_ok = replace_probe_hook(g_page_tabs_search_refresh_hook, probe_icon);
  return about_ok && menu_ok && icon_ok ? 1 : 0;
}

extern "C" __declspec(dllexport) int darkmode_skp_perf_info(char* buffer, int capacity) {
  if (!buffer || capacity <= 0) {
    return 0;
  }
  return std::snprintf(buffer, static_cast<size_t>(capacity),
    "{\"mode\":%d,\"about\":{\"calls\":%llu,\"ms\":%.4f,\"max_ms\":%.4f},"
    "\"menu\":{\"calls\":%llu,\"ms\":%.4f,\"max_ms\":%.4f},"
    "\"icon\":{\"calls\":%llu,\"ms\":%.4f,\"max_ms\":%.4f}}",
    g_probe_mode, g_probe_about.calls, g_probe_about.total_ms, g_probe_about.max_ms,
    g_probe_menu.calls, g_probe_menu.total_ms, g_probe_menu.max_ms,
    g_probe_icon.calls, g_probe_icon.total_ms, g_probe_icon.max_ms);
}

extern "C" __declspec(dllexport) double darkmode_skp_perf_lookup(int iterations) {
  const auto start = std::chrono::steady_clock::now();
  FARPROC volatile sink = nullptr;
  for (int index = 0; index < iterations; ++index) {
    sink = resolve_qt_lookup().widget_find_proc;
  }
  static_cast<void>(sink);
  return std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - start).count();
}
