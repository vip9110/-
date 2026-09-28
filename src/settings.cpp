#include "settings.h"
#include <cstdio>

std::wstring Settings::defaultPath() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, path))) return {};
    std::wstring folder = std::wstring(path) + L"\\LiliDesktopPet";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder + L"\\settings.ini";
}

bool Settings::load(const std::wstring& path, int dpi) {
    if (path.empty()) { firstRun = true; life.firstDay = todayStamp(); return false; }
    auto read = [&](const wchar_t* key, int fallback) { return static_cast<int>(GetPrivateProfileIntW(L"Pet", key, fallback, path.c_str())); };
    auto has = [&](const wchar_t* key) {
        wchar_t buffer[32]{}; GetPrivateProfileStringW(L"Pet", key, L"", buffer, 32, path.c_str()); return buffer[0] != 0;
    };
    firstRun = GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES;
    version = read(L"Version", 0);
    sizePercent = read(L"Size", 100); if (!validSize(sizePercent)) sizePercent = 100;
    topmost = read(L"Topmost", 1) != 0;
    roaming = read(L"Roaming", 0) != 0;
    dockMode = std::clamp(read(L"Dock", 2), 0, 2); if (dockMode) roaming = false;
    hotkeyMode = read(L"Hotkey", firstRun ? HOTKEY_DOUBLE_ESC : HOTKEY_ESC);
    if (hotkeyMode < 0 || hotkeyMode > 3) hotkeyMode = HOTKEY_DOUBLE_ESC;
    // 2.1 默认注册独占的全局 Esc，会吞掉所有软件的 Esc。升级时迁移为不占用按键的「双击 Esc」。
    if (!firstRun && version < 30 && hotkeyMode == HOTKEY_ESC) { hotkeyMode = HOTKEY_DOUBLE_ESC; migratedHotkey = true; }
    watchBin = read(L"WatchRecycleBin", 1) != 0;
    opacity = read(L"Opacity", 100); if (!validOpacity(opacity)) opacity = 100;
    fps = read(L"Fps", 60) == 30 ? 30 : 60;
    clickThrough = read(L"ClickThrough", 0) != 0;
    fullscreenHide = read(L"FullscreenHide", 1) != 0;
    openPanelOnStart = read(L"OpenPanelOnStart", 0) != 0;
    autoRest = read(L"AutoRest", 1) != 0;
    chatter = read(L"Chatter", 1) != 0;
    life.satiety = read(L"Satiety", 7000) / 100.; life.health = read(L"Health", 9500) / 100.;
    life.mood = read(L"Mood", 8000) / 100.; life.energy = read(L"Energy", 8500) / 100.; life.grams = read(L"Grams", 45000) / 100.;
    life.coins = read(L"Coins", 80); life.medicine = read(L"Medicine", 1); life.vitamins = read(L"Vitamins", 0);
    life.recycled = read(L"Recycled", 0); life.giftDate = read(L"GiftDate", 0);
    life.sick = read(L"Sick", 0) != 0; life.toy = read(L"Toy", 0) != 0;
    life.scarf = read(L"Scarf", 0) != 0; life.bed = read(L"Bed", 0) != 0; life.bell = read(L"Bell", 0) != 0;
    life.wearingScarf = read(L"WearingScarf", 0) != 0;
    life.careCooldown = std::clamp(read(L"CareCooldown", 0), 0, 300);
    life.petCount = read(L"PetCount", 0);
    life.firstDay = read(L"FirstDay", 0);
    if (life.firstDay < 19700101 || life.firstDay > todayStamp()) life.firstDay = todayStamp();
    life.normalize();
    if (has(L"PetX") && has(L"PetY")) {
        petX = read(L"PetX", 0); petY = read(L"PetY", 0); hasPosition = true;
    } else if (has(L"X") && has(L"Y")) {
        // 2.1 保存的是 280×286 画布左上角；换算成脚底锚点（窗口中心、画布底部上方 11 像素）。
        double oldScale = sizePercent / 100. * dpi / 96.;
        petX = read(L"X", 0) + 140 * oldScale; petY = read(L"Y", 0) + 275 * oldScale; hasPosition = true;
    }
    return !firstRun;
}

bool Settings::save(const std::wstring& path) const {
    if (path.empty()) return false;
    std::wstring text = L"[Pet]\r\n";
    auto put = [&](const wchar_t* key, long long value) { text += key; text += L'='; text += std::to_wstring(value); text += L"\r\n"; };
    put(L"Version", 30);
    put(L"X", std::llround(petX - 140 * sizePercent / 100.)); put(L"Y", std::llround(petY - 275 * sizePercent / 100.));
    put(L"PetX", std::llround(petX)); put(L"PetY", std::llround(petY));
    put(L"Size", sizePercent); put(L"Topmost", topmost); put(L"Roaming", roaming);
    put(L"Dock", dockMode); put(L"Hotkey", hotkeyMode); put(L"WatchRecycleBin", watchBin);
    put(L"Opacity", opacity); put(L"Fps", fps); put(L"ClickThrough", clickThrough); put(L"FullscreenHide", fullscreenHide);
    put(L"OpenPanelOnStart", openPanelOnStart); put(L"AutoRest", autoRest); put(L"Chatter", chatter);
    put(L"Satiety", static_cast<long long>(life.satiety * 100)); put(L"Health", static_cast<long long>(life.health * 100));
    put(L"Mood", static_cast<long long>(life.mood * 100)); put(L"Energy", static_cast<long long>(life.energy * 100));
    put(L"Grams", static_cast<long long>(life.grams * 100));
    put(L"Coins", life.coins); put(L"Medicine", life.medicine); put(L"Vitamins", life.vitamins); put(L"Recycled", life.recycled);
    put(L"GiftDate", life.giftDate); put(L"Sick", life.sick); put(L"Toy", life.toy); put(L"Scarf", life.scarf);
    put(L"Bed", life.bed); put(L"Bell", life.bell); put(L"WearingScarf", life.wearingScarf);
    put(L"CareCooldown", static_cast<long long>(std::ceil(life.careCooldown)));
    put(L"PetCount", life.petCount); put(L"FirstDay", life.firstDay);
    // 全部是 ASCII，按 ANSI 写入，旧版和系统 INI 接口都能读取。
    std::string bytes; bytes.reserve(text.size());
    for (wchar_t c : text) bytes.push_back(static_cast<char>(c));
    std::wstring temp = path + L".tmp";
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
    ok = FlushFileBuffers(file) && ok;
    CloseHandle(file);
    if (!ok) { DeleteFileW(temp.c_str()); return false; }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(temp.c_str()); return false; }
    return true;
}
