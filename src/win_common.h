#pragma once
// Windows 公共头：统一包含顺序、命令编号和自定义消息。
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objidl.h>
#include <ole2.h>
#include <algorithm>
// MSVC 的 GDI+ 头文件在 NOMINMAX 下需要 min/max。
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

constexpr wchar_t CLASS_NAME[] = L"LiliDesktopPet.Window.1";   // 与旧版相同，便于识别并接管旧版进程
constexpr wchar_t PANEL_CLASS[] = L"LiliDesktopPet.Panel.3";
constexpr wchar_t APP_TITLE[] = L"栗栗 · 小刺猬桌宠";
constexpr wchar_t APP_VERSION[] = L"3.0";
constexpr int APP_MAJOR = 30;                                    // 写入窗口属性 LiliMajorVersion；2.1 为 21
constexpr wchar_t SINGLETON_MUTEX[] = L"Local\\LiliDesktopPet.Singleton.1";
constexpr wchar_t SHOW_MESSAGE[] = L"LiliDesktopPet.Show.1";

constexpr UINT WM_APP_TRAY = WM_APP + 1;
constexpr UINT WM_APP_DOUBLE_ESC = WM_APP + 2;
constexpr UINT WM_APP_BIN_COUNT = WM_APP + 3;
constexpr UINT WM_APP_DROPPED = WM_APP + 4;
constexpr UINT_PTR TIMER_FRAME = 1;
constexpr int HOTKEY_ID = 77;

// 逻辑画布（100% 缩放时的像素）。脚底锚点位于 (身体中心, GROUND)。
constexpr float PET_W = 300.f, PET_H = 300.f, GROUND = 287.f;
constexpr float BODY_HALF = 80.f;    // 站立帧的半宽，用于停靠与边界

// 旧版命令编号保持不变，3.0 新增命令接在后面。
enum Command : int {
    PET = 1001, FEED, SLEEP, ROAM, TOPMOST, SMALL, MEDIUM, LARGE, HIDE, HELP, QUIT, SHOW,
    PANEL, DOCK_LEFT, DOCK_RIGHT, DOCK_FREE, ESC_MODE, ALT_MODE, NO_HOTKEY, WATCH_BIN,
    PICK_FILES, PICK_FOLDER, CONFIRM_FEED, CLEAR_FEED, OPEN_BIN, REMOVE_FEED,
    USE_MED, USE_VIT, FREE_CARE, EXERCISE, DAILY_GIFT, PLAY_TOY, WEAR_SCARF, TRY_ACTION,
    // 3.0
    DOUBLE_ESC_MODE = 1101, XSMALL, XLARGE, OPACITY_100, OPACITY_85, OPACITY_70, OPACITY_55,
    FPS_60, FPS_30, CLICK_THROUGH, FULLSCREEN_HIDE, AUTOSTART, OPEN_ON_START, AUTO_REST, CHATTER,
    RESET_POSITION, QUICK_MENU, SELECT_ALL_FEED, STOP_EXERCISE, CHEER_UP
};
constexpr int NAV_HOME = 2000, NAV_FEED = 2001, NAV_SHOP = 2002, NAV_CARE = 2003, NAV_SETTINGS = 2004, NAV_HELP = 2005;
constexpr int BUY_FIRST = 3000;
constexpr int FILE_ROW = 5000, FILE_DEL = 6000;   // + 行号

enum HotkeyMode { HOTKEY_ESC = 0, HOTKEY_CTRL_ALT_H = 1, HOTKEY_OFF = 2, HOTKEY_DOUBLE_ESC = 3 };

inline int todayStamp() {
    SYSTEMTIME t{}; GetLocalTime(&t);
    return t.wYear * 10000 + t.wMonth * 100 + t.wDay;
}
