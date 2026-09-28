#pragma once
// 本地设置与养成进度：%LOCALAPPDATA%\LiliDesktopPet\settings.ini
// 沿用 2.1 的全部键名，3.0 只追加新键；保存时先写临时文件再原子替换，避免断电造成存档损坏。
#include "win_common.h"
#include "pet_logic.h"

struct Settings {
    // 窗口与陪伴方式
    int sizePercent = 100, dockMode = 2, hotkeyMode = HOTKEY_DOUBLE_ESC, opacity = 100, fps = 60;
    bool topmost = true, roaming = false, watchBin = true;
    bool clickThrough = false, fullscreenHide = true, openPanelOnStart = false, autoRest = true, chatter = true;
    bool hasPosition = false;
    double petX = 0, petY = 0;
    // 升级信息
    int version = 0;                 // 读取到的存档版本（0 = 2.x 或全新）
    bool migratedHotkey = false;     // 本次从旧版全局 Esc 迁移到双击 Esc
    bool firstRun = false;
    PetLife life;

    static std::wstring defaultPath();
    bool load(const std::wstring& path, int dpi);
    bool save(const std::wstring& path) const;
    static bool validSize(int percent) { return percent == 60 || percent == 75 || percent == 100 || percent == 135 || percent == 160; }
    static bool validOpacity(int percent) { return percent == 100 || percent == 85 || percent == 70 || percent == 55; }
};
