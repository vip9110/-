// 栗栗桌宠 3.0：Win32 分层窗口 + GDI+，所有素材嵌入 exe，单文件运行，无需安装运行库。
#include "app.h"
#include <cstdio>

using namespace Gdiplus;

namespace {
CLSID pngEncoder() {
    CLSID clsid{};
    UINT count = 0, bytes = 0; GetImageEncodersSize(&count, &bytes);
    if (!bytes) return clsid;
    std::vector<BYTE> codecs(bytes);
    auto infos = reinterpret_cast<ImageCodecInfo*>(codecs.data());
    GetImageEncoders(count, bytes, infos);
    for (UINT i = 0; i < count; ++i) if (std::wcscmp(infos[i].MimeType, L"image/png") == 0) clsid = infos[i].Clsid;
    return clsid;
}

// 若旧版（2.x）正在运行：询问后请它自行退出（旧版收到 WM_CLOSE 会先保存进度）。返回 true 表示可以继续启动。
bool takeOverRunningInstance() {
    HWND existing = FindWindowW(CLASS_NAME, nullptr);
    INT_PTR version = existing ? reinterpret_cast<INT_PTR>(GetPropW(existing, L"LiliMajorVersion")) : 0;
    if (existing && version >= APP_MAJOR) { PostMessageW(existing, RegisterWindowMessageW(SHOW_MESSAGE), 0, 0); return false; }
    if (!existing) {
        MessageBoxW(nullptr, L"栗栗桌宠已经在运行（可能还在启动中）。请稍等片刻，或从系统托盘找到它。", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return false;
    }
    int answer = MessageBoxW(nullptr, L"检测到旧版栗栗桌宠正在运行。\n\n是否自动关闭旧版并启动 3.0？\n旧版会先保存进度，3.0 会直接沿用。", APP_TITLE, MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES) return false;
    DWORD pid = 0; GetWindowThreadProcessId(existing, &pid);
    HANDLE process = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    PostMessageW(existing, WM_CLOSE, 0, 0);
    bool closed = process && WaitForSingleObject(process, 10000) == WAIT_OBJECT_0;
    if (process) CloseHandle(process);
    for (int i = 0; !closed && i < 100 && IsWindow(existing); ++i) Sleep(100);
    if (IsWindow(existing)) {
        MessageBoxW(nullptr, L"旧版没有及时退出。请从旧版托盘菜单选择「退出桌宠」后再启动 3.0。", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return false;
    }
    return true;
}

bool savePanelPage(PetApp& app, int page, const wchar_t* file, const CLSID& encoder) {
    app.page = page; app.panelDirty = true;
    RECT client{}; GetClientRect(app.panel, &client);
    if (client.right <= 0 || !app.panelStatic.ensure(client.right, client.bottom)) return false;
    app.layoutPanel();
    Graphics g(app.panelStatic.bitmap.get());
    g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.ScaleTransform(app.uiScale, app.uiScale);
    app.paintPanelStatic(g); g.Flush(FlushIntentionSync);
    PixelBox clip{0, 0, client.right, client.bottom}; app.paintPortrait(g, &app.panelStatic, &clip); g.Flush(FlushIntentionSync);
    return app.panelStatic.bitmap->Save(file, &encoder, nullptr) == Ok;
}

// 使用真实 exe 的 --self-test 检查透明画面、嵌入素材、窗口更新、停靠、面板绘制与存档读写（不处理任何真实文件）。
int selfTest(PetApp& app) {
    FILE* log = _wfopen(L"self-test.txt", L"wb");
    if (!log) return 90;
    bool ok = true;
    auto check = [&](bool result, const char* label) { std::fprintf(log, "%s %s\n", result ? "PASS" : "FAIL", label); std::fflush(log); ok &= result; };
    CLSID encoder = pngEncoder();
    check(app.sprites.flow().loaded() && app.sprites.flow().pairCount() == 94, "embedded motion vectors (94 pairs)");
    check(app.sprites.side() == 256, "frames baked at 100% (256 px)");
    auto stats = [&](size_t& visible, size_t& transparent, bool& premultiplied) {
        visible = transparent = 0; premultiplied = true;
        for (int p = 0; p < app.canvas.w * app.canvas.h; ++p) {
            BYTE* pixel = app.canvas.pixels + p * 4;
            if (pixel[3] > 20) ++visible;
            if (!pixel[3]) ++transparent;
            if (pixel[0] > pixel[3] || pixel[1] > pixel[3] || pixel[2] > pixel[3]) premultiplied = false;
        }
    };
    for (int frame = 0; frame < Animation::FrameCount; ++frame) {
        app.bubble.clear(); app.draw(123456, frame);
        size_t visible, transparent; bool premultiplied; stats(visible, transparent, premultiplied);
        char label[128]; std::snprintf(label, sizeof(label), "frame_%d alpha / opaque body / premultiplied BGRA", frame);
        check(visible > 10000 && transparent > 20000 && premultiplied, label);
        // 清理后身体完全不透明
        const auto& pixels = app.sprites.frame(frame); size_t opaque = 0;
        for (size_t p = 3; p < pixels.size(); p += 4) if (pixels[p] == 255) ++opaque;
        std::snprintf(label, sizeof(label), "frame_%d body fully opaque after cleanup", frame);
        check(opaque > 8000, label);
        wchar_t name[64]; std::swprintf(name, 64, L"frame-%d.png", frame);
        check(app.canvas.bitmap->Save(name, &encoder, nullptr) == Ok, "PNG frame export");
    }
    for (int m = 0; m < Animation::MotionCount; ++m) {
        auto motion = static_cast<Animation::Motion>(m);
        app.animation.reset(1); app.animation.play(motion, true);
        for (int i = 0; i < 40; ++i) app.animation.advance(.02f, motion, 1);
        app.draw(123456);
        size_t visible, transparent; bool premultiplied; stats(visible, transparent, premultiplied);
        char label[128]; std::snprintf(label, sizeof(label), "motion_%d renders mid-animation", m);
        check(visible > 8000 && premultiplied, label);
        wchar_t name[64]; std::swprintf(name, 64, L"motion-%d.png", m);
        app.canvas.bitmap->Save(name, &encoder, nullptr);
    }
    app.animation.reset(1);
    // 围巾：每一帧都能画出红色围巾像素（被遮挡的嗅闻帧除外）
    app.life.scarf = app.life.wearingScarf = true;
    int scarfFrames = 0;
    for (int frame = 0; frame < Animation::FrameCount; ++frame) {
        app.draw(123456, frame); size_t red = 0;
        for (int p = 0; p < app.canvas.w * app.canvas.h; ++p) { BYTE* px = app.canvas.pixels + p * 4; if (px[3] > 200 && px[2] > 150 && px[1] < 90 && px[0] < 90) ++red; }
        if (red > 150) ++scarfFrames;
    }
    check(scarfFrames >= 58, "scarf visible on (almost) every frame");
    app.draw(123456, 55); app.canvas.bitmap->Save(L"scarf-roll.png", &encoder, nullptr);
    app.life.scarf = app.life.wearingScarf = false;
    // 气泡
    app.say(L"这是一段比较长的气泡文字，用来检查自动换行和宽度是否合适～", 5000); app.bubbleStart = GetTickCount64() - 1000;
    app.draw(GetTickCount64()); app.canvas.bitmap->Save(L"bubble.png", &encoder, nullptr);
    size_t top = 0; for (int y = 0; y < 60; ++y) for (int x = 0; x < app.canvas.w; ++x) if (app.canvas.pixels[(y * app.canvas.w + x) * 4 + 3] > 200) ++top;
    check(top > 3000, "speech bubble drawn with wrapping");
    app.bubble.clear();
    for (int size : {60, 75, 100, 135, 160}) {
        app.cfg.sizePercent = size; check(app.applyScale(), "size canvas creation"); app.draw(123456, 20); check(app.present(), "UpdateLayeredWindow");
    }
    app.dpi = 192; app.cfg.sizePercent = 160; check(app.applyScale() && app.sprites.side() == 410, "200 percent DPI at largest size (1.6x bake)");
    app.draw(123456, 20); check(app.present(), "high DPI window");
    app.dpi = 96; app.cfg.sizePercent = 100; app.applyScale();
    app.petX = -99999; app.petY = 99999; app.clampPosition(); app.placeCanvas();
    RECT area = app.workArea();
    check(app.canvasLeft >= area.left && app.canvasLeft + app.canvas.w <= area.right && app.petY <= area.bottom && app.petX > area.left, "offscreen position recovery");
    // 停靠：解除停靠时身体不跳动；停靠后身体贴近边缘
    app.command(DOCK_RIGHT); app.applyDock(true); app.placeCanvas();
    double beforeX = app.petX; float beforeBody = app.bodyX; int beforeLeft = app.canvasLeft;
    app.command(DOCK_FREE); app.placeCanvas();
    check(app.petX == beforeX && app.canvasLeft + app.bodyX * app.scale == beforeLeft + beforeBody * app.scale, "undocking keeps the body in place");
    check(app.petX + app.halfWidth() >= area.right - 1 && app.petX + app.halfWidth() * .8 <= area.right, "right dock touches the screen edge");
    app.command(DOCK_LEFT); app.applyDock(true); app.placeCanvas();
    check(app.petX - app.halfWidth() <= area.left + 1 && app.canvasLeft == area.left, "left dock keeps canvas on screen");
    // 拖动到中间后松开：不停靠、散步只暂停
    app.cfg.dockMode = 0; app.cfg.roaming = true; app.petX = (area.left + area.right) / 2.; app.settleAfterDrag();
    check(app.cfg.dockMode == 0 && app.cfg.roaming && app.roamPauseUntil > GetTickCount64(), "drag pauses roaming instead of disabling it");
    app.life.feedRecycled(1); app.happy(true);
    check(app.feeding && !app.sleeping && app.happyUntil > GetTickCount64(), "feed interaction (no file operations)");
    app.draw(GetTickCount64()); app.canvas.bitmap->Save(L"interaction.png", &encoder, nullptr);
    app.happyUntil = 0;
    app.command(SLEEP); check(app.sleeping && !app.moving, "sleep state");
    app.command(SLEEP); check(!app.sleeping, "wake state");
    bool prior = app.cfg.roaming; app.command(ROAM); check(app.cfg.roaming != prior, "roaming toggle");
    app.command(TOPMOST); check(!app.cfg.topmost, "topmost toggle");
    app.command(OPACITY_70); check(app.cfg.opacity == 70 && app.present(), "opacity setting");
    app.command(OPACITY_100);
    app.command(CLICK_THROUGH); check(app.cfg.clickThrough && (GetWindowLongPtrW(app.hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT), "click-through style");
    app.command(CLICK_THROUGH); check(!(GetWindowLongPtrW(app.hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT), "click-through off");
    app.trayReady = false; app.command(HIDE); check(!app.hidden, "hide guard when tray unavailable");
    app.idleAction = -1; app.life.health = 40; app.life.normalize(); app.command(PLAY_TOY); app.life.toy = true; app.command(PLAY_TOY);
    check(app.life.sick && app.idleAction < 0, "no toy play while sick");
    app.life.health = 95; app.life.normalize();
    app.life.energy = 50; app.command(EXERCISE); check(app.exerciseUntil > GetTickCount64(), "exercise starts");
    app.life.energy = 5; app.statClock = GetTickCount64() - 1200; app.updateLife(GetTickCount64()); check(app.exerciseUntil == 0, "exercise stops when energy runs out");
    app.life.energy = 5; app.cfg.autoRest = true; app.statClock = GetTickCount64() - 1200; app.updateLife(GetTickCount64()); check(app.sleeping && app.autoSlept, "auto rest when exhausted");
    app.life.energy = 90; app.statClock = GetTickCount64() - 1200; app.updateLife(GetTickCount64()); check(!app.sleeping, "auto wake after rest");
    // 面板：每一页都能绘制
    app.showPanel(NAV_HOME);
    check(app.panel != nullptr, "panel window");
    app.pendingFiles = {L"C:\\Users\\Lili\\Desktop\\旧的截图\\screenshot-2024-01-01.png", L"C:\\Users\\Lili\\Downloads\\installer-old.exe", L"D:\\临时文件夹"};
    app.fileSelected = {false, true, false};
    const std::pair<int, const wchar_t*> pages[] = {{NAV_HOME, L"panel-home.png"}, {NAV_FEED, L"panel-feed.png"}, {NAV_SHOP, L"panel-shop.png"},
                                                    {NAV_CARE, L"panel-care.png"}, {NAV_SETTINGS, L"panel-settings.png"}, {NAV_HELP, L"panel-help.png"}};
    for (const auto& page : pages) check(savePanelPage(app, page.first, page.second, encoder), "panel page render");
    app.pendingFiles.clear(); app.fileSelected.clear();
    // 存档读写往返
    wchar_t temp[MAX_PATH]{}; GetTempPathW(MAX_PATH, temp);
    std::wstring path = std::wstring(temp) + L"lili-selftest-settings.ini";
    Settings a; a.sizePercent = 135; a.dockMode = 1; a.hotkeyMode = HOTKEY_CTRL_ALT_H; a.opacity = 85; a.fps = 30; a.clickThrough = true;
    a.life.coins = 321; a.life.petCount = 12; a.life.firstDay = 20260101; a.life.scarf = a.life.wearingScarf = true; a.petX = 900; a.petY = 700;
    bool saved = a.save(path);
    Settings b; b.load(path, 96); DeleteFileW(path.c_str());
    check(saved && b.version == 30 && b.sizePercent == 135 && b.dockMode == 1 && b.hotkeyMode == HOTKEY_CTRL_ALT_H && b.opacity == 85 && b.fps == 30 &&
          b.clickThrough && b.life.coins == 321 && b.life.petCount == 12 && b.life.firstDay == 20260101 && b.life.wearingScarf && b.hasPosition && b.petX == 900, "settings round trip");
    // 2.1 存档迁移：独占 Esc → 双击 Esc
    {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        const char old[] = "[Pet]\r\nX=1500\r\nY=700\r\nSize=100\r\nDock=2\r\nHotkey=0\r\nCoins=150\r\nScarf=1\r\nWearingScarf=1\r\n";
        DWORD w = 0; WriteFile(f, old, sizeof(old) - 1, &w, nullptr); CloseHandle(f);
        Settings c; c.load(path, 96); DeleteFileW(path.c_str());
        check(c.hotkeyMode == HOTKEY_DOUBLE_ESC && c.migratedHotkey && c.life.coins == 150 && c.life.wearingScarf && c.hasPosition && c.petX == 1640, "2.1 settings migrate");
    }
    std::fprintf(log, "RESULT=%s\n", ok ? "PASS" : "FAIL"); std::fclose(log);
    return ok ? 0 : 1;
}
// --export-anim：用真实渲染器按脚本时间轴导出 30 帧/秒的画面，供 README 预览动图使用（开发用）。
int exportAnimation(PetApp& app) {
    CLSID encoder = pngEncoder();
    using M = Animation::Motion;
    struct Step { float until; M motion; bool joy; bool scarf; const wchar_t* line; };
    const Step steps[] = {{1.6f, M::Stand, false, false, nullptr}, {2.0f, M::Blink, false, false, nullptr}, {4.6f, M::Walk, false, false, nullptr},
                          {8.2f, M::Stretch, false, false, nullptr}, {9.6f, M::Hop, true, false, L"嘿嘿，今天也陪着你～"}, {11.6f, M::Cheer, true, true, L"新围巾！暖暖的，好看吗？"},
                          {13.6f, M::Enjoy, true, true, L"好舒服呀～"}, {18.3f, M::Roll, false, true, nullptr}, {19.2f, M::Stand, false, true, nullptr}};
    app.cfg.dockMode = 0; app.petX = 800; app.petY = 700; app.animation.reset(1); app.bubble.clear();
    int index = 0; float t = 0; const Step* previous = nullptr;
    for (const auto& step : steps) {
        while (t < step.until) {
            if (&step != previous) {
                previous = &step; app.animation.play(step.motion, true);
                app.life.scarf = app.life.wearingScarf = step.scarf;
                app.happyUntil = step.joy ? GetTickCount64() + 60000 : 0; app.joyMotion = step.motion;
                if (step.line) { app.say(step.line, 600000); app.bubbleStart = GetTickCount64() - 1000; } else app.bubble.clear();
            }
            app.animation.advance(1 / 30.f, step.motion, 1);
            app.draw(GetTickCount64());
            wchar_t name[64]; std::swprintf(name, 64, L"anim-%03d.png", index++);
            app.canvas.bitmap->Save(name, &encoder, nullptr);
            t += 1 / 30.f;
        }
    }
    return 0;
}

// --bench：分别统计合成身体、整帧绘制与提交分层窗口的耗时（开发用）。
int bench(PetApp& app) {
    FILE* log = _wfopen(L"bench.txt", L"wb");
    if (!log) return 90;
    LARGE_INTEGER freq, a, b; QueryPerformanceFrequency(&freq);
    auto ms = [&](LARGE_INTEGER x, LARGE_INTEGER y) { return (y.QuadPart - x.QuadPart) * 1000.0 / freq.QuadPart; };
    const std::pair<Animation::Motion, const char*> cases[] = {{Animation::Motion::Stand, "stand"}, {Animation::Motion::Walk, "walk"}, {Animation::Motion::Stretch, "stretch"}};
    for (const auto& c : cases) {
        app.animation.reset(1);
        double tRender = 0, tDraw = 0, tPresent = 0; const int n = 120;
        for (int i = 0; i < n; ++i) {
            app.animation.advance(1 / 60.f, c.first, 1);
            QueryPerformanceCounter(&a); app.sprites.render(app.animation.pose()); QueryPerformanceCounter(&b); tRender += ms(a, b);
            QueryPerformanceCounter(&a); app.draw(123456); QueryPerformanceCounter(&b); tDraw += ms(a, b);
            QueryPerformanceCounter(&a); app.present(); QueryPerformanceCounter(&b); tPresent += ms(a, b);
        }
        std::fprintf(log, "%s: body %.3f ms, draw %.3f ms (incl. body), present %.3f ms\n", c.second, tRender / n, tDraw / n, tPresent / n);
    }
    std::fclose(log);
    return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int) {
    std::wstring args = arguments ? arguments : L"";
    bool testing = args.find(L"--self-test") != std::wstring::npos || args.find(L"--bench") != std::wstring::npos || args.find(L"--export-anim") != std::wstring::npos;
    HANDLE mutex = nullptr;
    if (!testing) {
        mutex = CreateMutexW(nullptr, FALSE, SINGLETON_MUTEX);
        if (mutex && GetLastError() == ERROR_ALREADY_EXISTS && !takeOverRunningInstance()) { CloseHandle(mutex); return 0; }
    }
    HRESULT ole = OleInitialize(nullptr);
    GdiplusStartupInput input; ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &input, nullptr) != Ok) {
        MessageBoxW(nullptr, L"无法启动 Windows 绘图组件。", APP_TITLE, MB_ICONERROR);
        if (SUCCEEDED(ole)) OleUninitialize();
        if (mutex) CloseHandle(mutex);
        return 2;
    }
    int result = 0;
    {
        auto app = std::make_unique<PetApp>();
        g_app = app.get();
        app->oleReady = SUCCEEDED(ole);
        app->autostartLaunch = args.find(L"--autostart") != std::wstring::npos;
        if (!app->initialize(instance, testing)) {
            MessageBoxW(nullptr, L"桌宠素材加载失败，请重新下载完整的 exe 文件。", APP_TITLE, MB_ICONERROR); result = 3;
        } else {
            WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.style = CS_DBLCLKS;
            cls.lpfnWndProc = WindowProc; cls.hInstance = instance; cls.hIcon = app->icon; cls.hIconSm = app->smallIcon;
            cls.hCursor = LoadCursorW(nullptr, IDC_HAND); cls.lpszClassName = CLASS_NAME;
            RegisterClassExW(&cls);
            DWORD style = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | (app->cfg.topmost ? WS_EX_TOPMOST : 0);
            HWND window = CreateWindowExW(style, CLASS_NAME, APP_TITLE, WS_POPUP,
                app->canvasLeft, app->canvasTop, app->canvas.w, app->canvas.h, nullptr, nullptr, instance, app.get());
            if (!window) { MessageBoxW(nullptr, L"桌宠窗口创建失败。", APP_TITLE, MB_ICONERROR); result = 4; }
            else if (testing) {
                result = args.find(L"--bench") != std::wstring::npos ? bench(*app) : args.find(L"--export-anim") != std::wstring::npos ? exportAnimation(*app) : selfTest(*app);
                DestroyWindow(window);
            }
            else {
                // 窗口创建后按其所在显示器的 DPI 校正（2.1 只用了系统 DPI，多显示器不同缩放时尺寸不对）。
                using DpiFn = UINT(WINAPI*)(HWND);
                FARPROC address = GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
                DpiFn dpiForWindow = nullptr; std::memcpy(&dpiForWindow, &address, sizeof(dpiForWindow));
                if (dpiForWindow) { int real = static_cast<int>(dpiForWindow(window)); if (real && real != app->dpi) { app->dpi = real; app->applyScale(); } }
                if (!app->start()) { MessageBoxW(window, L"透明窗口绘制失败。", APP_TITLE, MB_ICONERROR); DestroyWindow(window); result = 5; }
                else {
                    MSG message{};
                    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
                }
            }
        }
        g_app = nullptr;
    }
    GdiplusShutdown(token);
    if (SUCCEEDED(ole)) OleUninitialize();
    if (mutex) CloseHandle(mutex);
    return result;
}
