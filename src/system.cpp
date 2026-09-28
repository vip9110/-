// 系统集成：托盘、快捷键（含不占用按键的双击 Esc）、回收站后台感应、开机自启、全屏检测、OLE 拖放。
#include "app.h"
#include <shobjidl.h>

namespace {
HHOOK g_hook = nullptr;
HWND g_hookTarget = nullptr;
DWORD g_lastEscDown = 0;
bool g_escHeld = false;

// 低级键盘钩子只“旁听”Esc，始终把按键继续传给当前软件（CallNextHookEx），所以不会吞掉任何按键。
LRESULT CALLBACK keyboardHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        if (key->vkCode == VK_ESCAPE && !(key->flags & LLKHF_INJECTED)) {
            if (down && !g_escHeld) {
                g_escHeld = true;
                bool modifier = ((GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_SHIFT) |
                                  GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
                if (!modifier && g_lastEscDown && key->time - g_lastEscDown <= 420) {
                    g_lastEscDown = 0;
                    if (g_hookTarget) PostMessageW(g_hookTarget, WM_APP_DOUBLE_ESC, 0, 0);
                } else g_lastEscDown = modifier ? 0 : key->time;
            } else if (!down) g_escHeld = false;
        } else if (down) g_lastEscDown = 0;   // 中间按了别的键，不算双击
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

// 线程自己持有窗口句柄和事件句柄的副本，即使查询回收站很慢、退出时没能及时结束，也不会访问已释放的对象。
struct BinWatcherArgs { HWND target; HANDLE stop, wake; };
DWORD WINAPI binWatcher(LPVOID parameter) {
    std::unique_ptr<BinWatcherArgs> args(static_cast<BinWatcherArgs*>(parameter));
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HANDLE events[2] = {args->stop, args->wake};
    for (;;) {
        SHQUERYRBINFO info{}; info.cbSize = sizeof(info);
        HRESULT result = SHQueryRecycleBinW(nullptr, &info);
        if (WaitForSingleObject(args->stop, 0) == WAIT_OBJECT_0) break;
        PostMessageW(args->target, WM_APP_BIN_COUNT, SUCCEEDED(result) ? 1 : 0, static_cast<LPARAM>(info.i64NumItems));
        if (WaitForMultipleObjects(2, events, FALSE, 10000) == WAIT_OBJECT_0) break;
    }
    CloseHandle(args->stop); CloseHandle(args->wake);
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
}

constexpr wchar_t RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RUN_VALUE[] = L"LiliDesktopPet";
std::wstring autostartCommand() {
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH);
    return L"\"" + std::wstring(path) + L"\" --autostart";
}
} // namespace

// ---------- 托盘 ----------
void PetApp::addTray() {
    if (testing || !hwnd) return;
    NOTIFYICONDATAW data{}; data.cbSize = sizeof(data); data.hWnd = hwnd; data.uID = 1;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; data.uCallbackMessage = WM_APP_TRAY; data.hIcon = smallIcon ? smallIcon : icon;
    lstrcpynW(data.szTip, L"栗栗桌宠 · 单击找回 / 双击生活馆 / 右键菜单", static_cast<int>(std::size(data.szTip)));
    trayReady = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (!trayReady) trayReady = Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
}

void PetApp::removeTray() {
    if (!hwnd) return;
    NOTIFYICONDATAW data{}; data.cbSize = sizeof(data); data.hWnd = hwnd; data.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &data); trayReady = false;
}

void PetApp::menu(POINT point) {
    if (menuOpen) return;
    menuOpen = true; moving = false;
    HMENU popup = CreatePopupMenu(), sizes = CreatePopupMenu(), alpha = CreatePopupMenu();
    if (!popup || !sizes || !alpha) {
        for (HMENU m : {popup, sizes, alpha}) if (m) DestroyMenu(m);
        menuOpen = false; return;
    }
    auto check = [](bool on) { return static_cast<UINT>(MF_STRING | (on ? MF_CHECKED : 0)); };
    AppendMenuW(popup, MF_STRING | MF_DISABLED, 0, L"栗栗 · 今天也陪着你");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    if (hidden) AppendMenuW(popup, MF_STRING, SHOW, L"显示桌宠");
    AppendMenuW(popup, MF_STRING, PANEL, L"打开生活馆 / 商店");
    AppendMenuW(popup, MF_STRING, PET, L"摸摸它");
    AppendMenuW(popup, MF_STRING, FEED, L"文件喂食");
    AppendMenuW(popup, check(sleeping), SLEEP, sleeping ? L"醒一醒" : L"睡一会儿");
    AppendMenuW(popup, check(cfg.roaming), ROAM, L"自动散步");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sizes, check(cfg.sizePercent == 60), XSMALL, L"迷你 · 60%");
    AppendMenuW(sizes, check(cfg.sizePercent == 75), SMALL, L"小巧 · 75%");
    AppendMenuW(sizes, check(cfg.sizePercent == 100), MEDIUM, L"适中 · 100%");
    AppendMenuW(sizes, check(cfg.sizePercent == 135), LARGE, L"大只 · 135%");
    AppendMenuW(sizes, check(cfg.sizePercent == 160), XLARGE, L"超大 · 160%");
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(sizes), L"桌宠大小");
    AppendMenuW(alpha, check(cfg.opacity == 100), OPACITY_100, L"100%");
    AppendMenuW(alpha, check(cfg.opacity == 85), OPACITY_85, L"85%");
    AppendMenuW(alpha, check(cfg.opacity == 70), OPACITY_70, L"70%");
    AppendMenuW(alpha, check(cfg.opacity == 55), OPACITY_55, L"55%");
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(alpha), L"不透明度");
    AppendMenuW(popup, check(cfg.topmost), TOPMOST, L"保持置顶");
    AppendMenuW(popup, check(cfg.clickThrough), CLICK_THROUGH, L"鼠标穿透");
    AppendMenuW(popup, MF_STRING, RESET_POSITION, L"回到屏幕右下角");
    if (!hidden) AppendMenuW(popup, MF_STRING, HIDE, L"隐藏到托盘");
    AppendMenuW(popup, MF_STRING, HELP, L"使用说明");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, QUIT, L"退出桌宠");
    // 菜单挂在一个普通的隐藏窗口上：桌宠窗口不可激活（且可能开启了鼠标穿透），直接作为菜单宿主时菜单可能不显示或点外面不消失。
    if (!menuHost) menuHost = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"LiliMenuHost", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    HWND owner = menuHost ? menuHost : hwnd;
    SetForegroundWindow(owner);
    UINT selected = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, 0, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(popup);   // 会一并销毁子菜单
    menuOpen = false; tickTime = GetTickCount64(); nextDecision = tickTime + 2000;
    if (selected) command(static_cast<int>(selected));
}

// ---------- 快捷键 ----------
void PetApp::releaseHotkey() {
    if (hwnd) UnregisterHotKey(hwnd, HOTKEY_ID);
    if (g_hook) { UnhookWindowsHookEx(g_hook); g_hook = nullptr; }
    g_hookTarget = nullptr; g_lastEscDown = 0; g_escHeld = false;
    hotkeyReady = false; actualHotkey = -1;
}

void PetApp::configureHotkey() {
    releaseHotkey();
    if (modal || testing || !hwnd) return;
    auto registerCtrlAltH = [&]() { return RegisterHotKey(hwnd, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'H') != FALSE; };
    switch (cfg.hotkeyMode) {
    case HOTKEY_OFF: hotkeyStatus = L"快捷键已关闭，可从托盘图标找回栗栗"; return;
    case HOTKEY_ESC:
        if (RegisterHotKey(hwnd, HOTKEY_ID, MOD_NOREPEAT, VK_ESCAPE)) { actualHotkey = HOTKEY_ESC; hotkeyStatus = L"Esc（独占）隐藏 / 显示"; }
        else if (registerCtrlAltH()) { actualHotkey = HOTKEY_CTRL_ALT_H; hotkeyStatus = L"Esc 已被占用，改用 Ctrl + Alt + H"; }
        break;
    case HOTKEY_CTRL_ALT_H:
        if (registerCtrlAltH()) { actualHotkey = HOTKEY_CTRL_ALT_H; hotkeyStatus = L"Ctrl + Alt + H 隐藏 / 显示"; }
        break;
    default:
        g_hookTarget = hwnd;
        g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHook, instance, 0);
        if (g_hook) { actualHotkey = HOTKEY_DOUBLE_ESC; hotkeyStatus = L"快速按两下 Esc 隐藏 / 显示（不占用 Esc）"; }
        else if (registerCtrlAltH()) { g_hookTarget = nullptr; actualHotkey = HOTKEY_CTRL_ALT_H; hotkeyStatus = L"双击 Esc 不可用，改用 Ctrl + Alt + H"; }
        break;
    }
    hotkeyReady = actualHotkey >= 0;
    if (!hotkeyReady) hotkeyStatus = L"快捷键被占用，请使用托盘图标";
}

// ---------- 回收站感应（后台线程，避免回收站很大时每 10 秒卡一下界面） ----------
bool PetApp::startBinWatcher() {
    if (testing || binThread || !hwnd) return binThread != nullptr;
    binStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    binWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!binStop || !binWake) return false;
    auto args = std::make_unique<BinWatcherArgs>(BinWatcherArgs{hwnd, nullptr, nullptr});
    HANDLE self = GetCurrentProcess();
    if (!DuplicateHandle(self, binStop, self, &args->stop, 0, FALSE, DUPLICATE_SAME_ACCESS)) return false;
    if (!DuplicateHandle(self, binWake, self, &args->wake, 0, FALSE, DUPLICATE_SAME_ACCESS)) { CloseHandle(args->stop); return false; }
    InterlockedExchange(&binBaselineRequest, 1);
    BinWatcherArgs* raw = args.release();
    binThread = CreateThread(nullptr, 0, binWatcher, raw, 0, nullptr);
    if (!binThread) { CloseHandle(raw->stop); CloseHandle(raw->wake); delete raw; }
    return binThread != nullptr;
}

void PetApp::stopBinWatcher() {
    if (binThread) {
        SetEvent(binStop);
        WaitForSingleObject(binThread, 5000);
        CloseHandle(binThread); binThread = nullptr;
    }
    if (binStop) { CloseHandle(binStop); binStop = nullptr; }
    if (binWake) { CloseHandle(binWake); binWake = nullptr; }
}

void PetApp::requestBinBaseline() {
    InterlockedExchange(&binBaselineRequest, 1);
    if (binWake) SetEvent(binWake);
}

void PetApp::onBinCount(bool ok, LONGLONG count) {
    if (!ok) { binBaseline = -1; return; }
    bool baselineOnly = InterlockedExchange(&binBaselineRequest, 0) != 0;
    if (!baselineOnly && !modal && cfg.watchBin && binBaseline >= 0 && count > binBaseline) {
        int added = static_cast<int>(std::min<LONGLONG>(100, count - binBaseline));
        life.feedRecycled(added); happy(true);
        notice = L"感应到回收站新增加 " + std::to_wstring(added) + L" 项，栗栗吃饱了一点。文件仍可从回收站恢复。";
        requestSave(); refreshPanel();
    }
    binBaseline = count;
}

// ---------- 开机自启（仅当前用户，默认关闭） ----------
bool PetApp::autostartEnabled() {
    wchar_t value[MAX_PATH * 2]{}; DWORD bytes = sizeof(value), type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ, &type, value, &bytes) != ERROR_SUCCESS) return false;
    return lstrcmpiW(value, autostartCommand().c_str()) == 0;
}

bool PetApp::setAutostart(bool enable) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS status;
    if (enable) {
        auto command = autostartCommand();
        status = RegSetValueExW(key, RUN_VALUE, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, RUN_VALUE);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

// ---------- 全屏自动隐藏 ----------
bool PetApp::fullscreenForeground() const {
    using QueryState = HRESULT(WINAPI*)(int*);
    static QueryState query = []() {
        QueryState fn = nullptr; FARPROC address = GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHQueryUserNotificationState");
        std::memcpy(&fn, &address, sizeof(fn)); return fn;
    }();
    int state = 0;
    if (query && SUCCEEDED(query(&state)) && (state == 3 /*QUNS_RUNNING_D3D_FULL_SCREEN*/ || state == 4 /*QUNS_PRESENTATION_MODE*/)) return true;
    HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == hwnd || foreground == panel || !IsWindowVisible(foreground) || IsIconic(foreground)) return false;
    wchar_t className[64]{}; GetClassNameW(foreground, className, 64);
    for (const wchar_t* shell : {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"NotifyIconOverflowWindow"})
        if (!lstrcmpW(className, shell)) return false;
    LONG_PTR style = GetWindowLongPtrW(foreground, GWL_STYLE);
    if ((style & WS_CAPTION) == WS_CAPTION && IsZoomed(foreground)) return false;   // 普通最大化窗口不算全屏
    RECT rect{}; if (!GetWindowRect(foreground, &rect)) return false;
    HMONITOR monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL);
    MONITORINFO info{}; info.cbSize = sizeof(info);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) return false;
    bool covers = rect.left <= info.rcMonitor.left && rect.top <= info.rcMonitor.top && rect.right >= info.rcMonitor.right && rect.bottom >= info.rcMonitor.bottom;
    if (!covers) return false;
    POINT anchor{static_cast<LONG>(petX), static_cast<LONG>(petY - 50 * scale)};
    return MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST) == monitor;
}

void PetApp::checkFullscreen(ULONGLONG now) {
    if (testing || !hwnd || now - lastFullscreenCheck < 1000) return;
    lastFullscreenCheck = now;
    bool want = cfg.fullscreenHide && !hidden && !dragging && !menuOpen && fullscreenForeground();
    if (want == autoHidden) return;
    autoHidden = want;
    if (autoHidden) ShowWindow(hwnd, SW_HIDE);
    else if (!hidden) {
        tickTime = now; ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, cfg.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        render(now);
    }
    updateTimer();
}

// ---------- OLE 拖放：文件悬停在栗栗身上时会低头嗅嗅 ----------
class DropTarget final : public IDropTarget {
    volatile LONG refs_ = 1;
    PetApp* app_;
    HWND window_;
    bool pet_, accept_ = false;
    IDropTargetHelper* helper_ = nullptr;
    static CLIPFORMAT descriptionFormat() { static CLIPFORMAT format = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_DROPDESCRIPTION)); return format; }
    static void describe(IDataObject* data, DROPIMAGETYPE type, const wchar_t* message, const wchar_t* insert) {
        if (!data) return;
        FORMATETC format{descriptionFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{}; medium.tymed = TYMED_HGLOBAL;
        medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPDESCRIPTION));
        if (!medium.hGlobal) return;
        if (auto* d = static_cast<DROPDESCRIPTION*>(GlobalLock(medium.hGlobal))) {
            d->type = type; lstrcpynW(d->szMessage, message, MAX_PATH); lstrcpynW(d->szInsert, insert, MAX_PATH);
            GlobalUnlock(medium.hGlobal);
        }
        if (FAILED(data->SetData(&format, &medium, TRUE))) GlobalFree(medium.hGlobal);
    }
    DWORD effect(DWORD allowed) const {
        if (!accept_) return DROPEFFECT_NONE;
        return allowed & DROPEFFECT_COPY ? DROPEFFECT_COPY : allowed & DROPEFFECT_LINK ? DROPEFFECT_LINK : DROPEFFECT_NONE;
    }
    IDataObject* current_ = nullptr;
public:
    DropTarget(PetApp* app, HWND window, bool pet) : app_(app), window_(window), pet_(pet) {
        CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&helper_));
    }
    ~DropTarget() { if (helper_) helper_->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IDropTarget)) { *object = static_cast<IDropTarget*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    ULONG STDMETHODCALLTYPE Release() override { ULONG left = static_cast<ULONG>(InterlockedDecrement(&refs_)); if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL point, DWORD* result) override {
        FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        accept_ = data && !app_->modal && SUCCEEDED(data->QueryGetData(&format));
        *result = effect(*result);
        current_ = data;
        if (accept_) describe(data, DROPIMAGE_COPY, L"喂给 %1（先加入清单）", L"栗栗");
        if (helper_) { POINT p{point.x, point.y}; helper_->DragEnter(window_, data, &p, *result); }
        if (accept_ && pet_) app_->fileHoverChanged(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL point, DWORD* result) override {
        *result = effect(*result);
        if (helper_) { POINT p{point.x, point.y}; helper_->DragOver(&p, *result); }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (accept_) describe(current_, DROPIMAGE_INVALID, L"", L"");
        current_ = nullptr;
        if (helper_) helper_->DragLeave();
        if (pet_) app_->fileHoverChanged(false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL point, DWORD* result) override {
        *result = effect(*result);
        if (helper_) { POINT p{point.x, point.y}; helper_->Drop(data, &p, *result); }
        if (pet_) app_->fileHoverChanged(false);
        current_ = nullptr;
        if (!accept_ || !data) { *result = DROPEFFECT_NONE; return S_OK; }
        FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (SUCCEEDED(data->GetData(&format, &medium))) {
            if (auto drop = static_cast<HDROP>(GlobalLock(medium.hGlobal))) {
                UINT count = std::min(100u, DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0));
                for (UINT i = 0; i < count; ++i) {
                    UINT length = DragQueryFileW(drop, i, nullptr, 0); std::wstring path(length + 1, L'\0');
                    DragQueryFileW(drop, i, path.data(), length + 1); path.resize(length); app_->droppedPaths.push_back(path);
                }
                GlobalUnlock(medium.hGlobal);
            }
            ReleaseStgMedium(&medium);
        }
        // 放下后再处理：不在拖放源（资源管理器）的模态循环中弹出面板。
        if (!app_->droppedPaths.empty()) PostMessageW(app_->hwnd, WM_APP_DROPPED, 0, 0);
        return S_OK;
    }
};

void PetApp::registerDropTargets() {
    if (testing) return;
    if (hwnd && !petDrop) {
        if (oleReady) {
            petDrop = new DropTarget(this, hwnd, true);
            if (FAILED(RegisterDragDrop(hwnd, petDrop))) { petDrop->Release(); petDrop = nullptr; }
        }
        if (!petDrop) DragAcceptFiles(hwnd, TRUE);
    }
    if (panel && !panelDrop) {
        if (oleReady) {
            panelDrop = new DropTarget(this, panel, false);
            if (FAILED(RegisterDragDrop(panel, panelDrop))) { panelDrop->Release(); panelDrop = nullptr; }
        }
        if (!panelDrop) DragAcceptFiles(panel, TRUE);
    }
}

void PetApp::revokeDropTargets() {
    if (petDrop) { if (hwnd) RevokeDragDrop(hwnd); petDrop->Release(); petDrop = nullptr; }
    else if (hwnd) DragAcceptFiles(hwnd, FALSE);
    revokePanelDrop();
}

void PetApp::revokePanelDrop() {
    if (panelDrop) { if (panel) RevokeDragDrop(panel); panelDrop->Release(); panelDrop = nullptr; }
}

void PetApp::fileHoverChanged(bool over) {
    if (fileHover == over) return;
    fileHover = over;
    if (over && !sleeping && !hidden) { idleAction = -1; say(L"嗅嗅……是要喂我吃的吗？", 2600); }
}
