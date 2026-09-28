// 桌宠核心：状态机、养成更新、命令分发、文件喂食流程。
#include "app.h"
#include "recycle.h"
#include "resource.h"

PetApp* g_app = nullptr;
using Animation::Motion;

namespace {
UINT systemDpi() {
    using Fn = UINT(WINAPI*)();
    FARPROC address = GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem");
    Fn getDpi = nullptr;
    static_assert(sizeof(getDpi) == sizeof(address));
    std::memcpy(&getDpi, &address, sizeof(getDpi));
    if (getDpi) return getDpi();
    HDC screen = GetDC(nullptr); UINT value = static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSX)); ReleaseDC(nullptr, screen);
    return value ? value : 96;
}
const Motion IDLE_CHOICES[] = {Motion::Stretch, Motion::Yawn, Motion::Scratch, Motion::Roll, Motion::Sniff, Motion::Look};
const Motion DEMO_CHOICES[] = {Motion::Stretch, Motion::Yawn, Motion::Scratch, Motion::Roll, Motion::Sniff, Motion::Look, Motion::Cheer, Motion::Enjoy};
} // namespace

bool PetApp::initialize(HINSTANCE module, bool testMode) {
    instance = module; testing = testMode;
    if (!sprites.loadResources(instance)) return false;
    dpi = testing ? 96 : static_cast<int>(systemDpi());
    if (!testing) { configPath = Settings::defaultPath(); cfg.load(configPath, dpi); }
    else { cfg.life.firstDay = todayStamp(); cfg.firstRun = true; }
    if (cfg.hasPosition) { petX = cfg.petX; petY = cfg.petY; }
    else {
        RECT area{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
        float s = cfg.sizePercent / 100.f * dpi / 96.f;
        petX = area.right - (BODY_HALF + 70) * s; petY = area.bottom - 4 * s;
    }
    if (!applyScale()) return false;
    direction = cfg.dockMode == 1 ? 1 : -1;
    animation.reset(direction);
    sprites.render(animation.pose()); bodyPose = animation.pose();
    int bigSize = GetSystemMetrics(SM_CXICON), smallSize = GetSystemMetrics(SM_CXSMICON);
    icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_PET), IMAGE_ICON, bigSize ? bigSize : 32, bigSize ? bigSize : 32, LR_DEFAULTCOLOR));
    smallIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_PET), IMAGE_ICON, smallSize ? smallSize : 16, smallSize ? smallSize : 16, LR_DEFAULTCOLOR));
    taskbarMessage = RegisterWindowMessageW(L"TaskbarCreated");
    showMessage = RegisterWindowMessageW(SHOW_MESSAGE);
    tickTime = statClock = lastSave = GetTickCount64();
    nextDecision = tickTime + 8500; blinkStart = tickTime + 3000; nextIdle = tickTime + 5000;
    nextChatter = tickTime + static_cast<ULONGLONG>(randInt(60, 120)) * 1000;
    return true;
}

// 窗口创建之后调用：注册系统功能并显示第一帧。
bool PetApp::start() {
    SetPropW(hwnd, L"LiliMajorVersion", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(APP_MAJOR)));
    applyWindowStyle();
    registerDropTargets();
    configureHotkey();
    startBinWatcher();
    addTray();
    if (cfg.migratedHotkey) notice = L"3.0 起默认「快速按两下 Esc」隐藏 / 显示，不再吞掉其他软件的 Esc。想用旧方式请选「Esc（独占）」。";
    else notice = hotkeyStatus + L"。右键或双击栗栗可打开生活馆。";
    const wchar_t* hint = actualHotkey == HOTKEY_DOUBLE_ESC ? L"快速按两下 Esc，我就躲起来～" : actualHotkey == HOTKEY_ESC ? L"右键打开小窝，Esc 藏猫猫～"
                        : actualHotkey == HOTKEY_CTRL_ALT_H ? L"Ctrl+Alt+H 可以隐藏或显示我～" : L"右键打开小窝，托盘可找回我～";
    say(cfg.firstRun ? L"你好呀，我是栗栗！双击或右键我打开小窝～" : hint, 8500);
    ULONGLONG now = GetTickCount64();
    draw(now);
    if (!present()) return false;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    updateTimer();
    if (!autostartLaunch && (cfg.firstRun || cfg.openPanelOnStart || cfg.migratedHotkey)) showPanel(cfg.migratedHotkey ? NAV_SETTINGS : NAV_HOME);
    requestSave(3000);   // 写入 3.0 存档版本与迁移结果
    return true;
}

void PetApp::shutdown() {
    stopBinWatcher();
    revokeDropTargets();
    releaseHotkey();
    if (hwnd) KillTimer(hwnd, TIMER_FRAME);
    saveNow();
    removeTray();
}

PetApp::~PetApp() {
    stopBinWatcher();
    if (icon) DestroyIcon(icon);
    if (smallIcon) DestroyIcon(smallIcon);
}

void PetApp::requestSave(int delayMs) {
    ULONGLONG due = GetTickCount64() + static_cast<ULONGLONG>(delayMs);
    if (!saveDue || due < saveDue) saveDue = due;
}

void PetApp::saveNow() {
    saveDue = 0; lastSave = GetTickCount64();
    if (testing || configPath.empty()) return;
    cfg.petX = petX; cfg.petY = petY;
    cfg.save(configPath);
}

void PetApp::say(const std::wstring& text, int duration) {
    ULONGLONG now = GetTickCount64();
    if (!(now < bubbleUntil && bubble == text)) bubbleStart = now;
    bubble = text; bubbleUntil = now + static_cast<ULONGLONG>(duration);
}

void PetApp::happy(bool food) {
    ULONGLONG now = GetTickCount64();
    if (sleeping) { sleeping = false; autoSlept = false; }
    moving = false; feeding = food; idleAction = -1; idleUntil = 0;
    joyMotion = food ? Motion::Feed : Motion::Hop;
    animation.play(joyMotion, true);
    happyUntil = now + static_cast<ULONGLONG>(1000 * Animation::duration(joyMotion));
    nextIdle = happyUntil + 4000; nextDecision = happyUntil + 2200;
    if (food) say(L"咔嚓～文件已经进回收站啦！");
    else {
        const wchar_t* lines[] = {L"嘿嘿，今天也陪着你～", L"摸摸头，烦恼都溜走！", L"被你发现啦！", L"慢慢来，我在这里陪你。", L"再摸一下下～", L"最喜欢你啦！"};
        say(lines[randInt(0, 5)]);
    }
}

void PetApp::celebrate(const wchar_t* line) {
    ULONGLONG now = GetTickCount64();
    if (sleeping) { sleeping = false; autoSlept = false; }
    moving = false; feeding = false; idleAction = -1; idleUntil = 0;
    joyMotion = life.sick ? Motion::Hop : Motion::Cheer;
    animation.play(joyMotion, true);
    happyUntil = now + static_cast<ULONGLONG>(1000 * Animation::duration(joyMotion));
    nextIdle = happyUntil + 4000; nextDecision = happyUntil + 2200;
    if (line) say(line);
}

void PetApp::petReaction(bool rub) {
    life.pet(rub ? 3 : 2);
    if (!rub) { happy(false); return; }
    ULONGLONG now = GetTickCount64();
    if (sleeping) { say(L"呼……（被摸得好舒服，翻了个身）", 2600); return; }
    moving = false; feeding = false; idleAction = -1; idleUntil = 0;
    joyMotion = Motion::Enjoy; animation.play(joyMotion, true);
    happyUntil = now + static_cast<ULONGLONG>(1000 * Animation::duration(joyMotion));
    nextIdle = happyUntil + 4000; nextDecision = happyUntil + 2200;
    const wchar_t* lines[] = {L"好舒服呀～", L"再摸摸这里～", L"咕噜咕噜……", L"刺刺是软的哦！"};
    say(lines[randInt(0, 3)], 2600);
    requestSave(4000);
}

void PetApp::startIdle(Motion motion) {
    ULONGLONG now = GetTickCount64();
    idleAction = static_cast<int>(motion); lastIdleAction = idleAction; moving = false;
    animation.play(motion, animation.motion() == motion);
    idleUntil = now + static_cast<ULONGLONG>(1000 * Animation::duration(motion));
    nextIdle = idleUntil + static_cast<ULONGLONG>(randInt(4200, 8500));
    nextDecision = idleUntil + static_cast<ULONGLONG>(randInt(1500, 2500));
}

void PetApp::setSleeping(bool value, bool automatic) {
    if (sleeping == value) return;
    sleeping = value; autoSlept = value && automatic;
    moving = false; happyUntil = 0; idleAction = -1; idleUntil = 0;
    if (value) exerciseUntil = 0;
    else startIdle(Motion::Wake);
    nextDecision = GetTickCount64() + 3500;
}

void PetApp::stopExercise(const wchar_t* reason) {
    exerciseUntil = 0;
    if (reason) { notice = reason; say(reason, 4200); }
    refreshPanel();
}

Motion PetApp::requestedMotion(ULONGLONG now) const {
    if (dragging && dragMoved) return Motion::Carry;
    if (happyUntil) return joyMotion;
    if (fileHover) return Motion::Curious;
    if (sleeping) return Motion::Sleep;
    if (life.sick) return Motion::Sick;
    if (idleAction >= 0) return static_cast<Motion>(idleAction);
    if (now < exerciseUntil || ((moving || std::abs(walkSpeed) > 1.f) && !menuOpen)) return Motion::Walk;
    return now < blinkUntil ? Motion::Blink : Motion::Stand;
}

void PetApp::updateLife(ULONGLONG now) {
    if (modal) { statClock = now; return; }
    if (now - statClock >= 1000) {
        bool wasSick = life.sick;
        bool exercising = !hidden && !autoHidden && (moving || now < exerciseUntil);
        life.advance((now - statClock) / 1000., sleeping, exercising); statClock = now;
        if (life.sick) moving = false;
        if (now < exerciseUntil && life.mustStopExercise())
            stopExercise(life.sick ? L"栗栗不舒服，踏步已停止。先去照护页照顾它吧。" : L"精力不足，踏步提前结束，让栗栗休息一下。");
        if (!wasSick && life.sick) { notice = L"栗栗有点不舒服。去照护页用药，或陪伴、休息帮助恢复。"; say(L"有点不舒服，陪陪我好吗？", 5500); refreshPanel(); }
        if (wasSick && !life.sick) { notice = L"栗栗康复啦！谢谢你的细心照顾。"; celebrate(L"我好多啦，谢谢你～"); refreshPanel(); }
        // 自动作息：精力见底会自己打盹，睡饱后自己醒来（可在设置中关闭）。
        if (cfg.autoRest && !sleeping && !dragging && !modal && life.energy < 12 && !happyUntil && now >= exerciseUntil) {
            setSleeping(true, true); notice = L"栗栗太困了，自己睡着啦。精力恢复后会自动醒来。";
            say(L"好困……先睡一小会儿。", 4000); refreshPanel();
        } else if (sleeping && autoSlept && life.energy >= 85) {
            setSleeping(false); say(L"睡饱啦，精神满满！"); refreshPanel();
        }
        if (panel && IsWindowVisible(panel)) { panelDirty = true; InvalidateRect(panel, nullptr, FALSE); }
    }
    if (now - lastSave >= 30000) saveNow();
}

void PetApp::chatter(ULONGLONG now) {
    if (now < nextChatter) return;
    nextChatter = now + static_cast<ULONGLONG>(randInt(90, 200)) * 1000;
    if (!cfg.chatter || hidden || autoHidden || dragging || modal || sleeping || now < bubbleUntil) return;
    SYSTEMTIME t{}; GetLocalTime(&t);
    int period = t.wHour >= 6 && t.wHour < 10 ? 0 : t.wHour >= 11 && t.wHour < 13 ? 1 : t.wHour >= 17 && t.wHour < 19 ? 2 : t.wHour >= 23 || t.wHour < 5 ? 3 : -1;
    if (period >= 0 && period != greetedPeriod) {
        greetedPeriod = period;
        const wchar_t* greet[] = {L"早上好！今天也要元气满满～", L"午饭时间到啦，记得好好吃饭哦。", L"傍晚啦，辛苦了一天～", L"夜深了，早点休息吧，我陪你到这里。"};
        say(greet[period], 5000); return;
    }
    if (life.sick) { say(L"咳……摸摸我就会好一点。", 4200); return; }
    if (life.satiety < 25) { say(L"肚子咕咕叫了，有不要的文件可以喂我哦～", 4800); return; }
    if (life.energy < 22) { say(L"有点困了……想休息一下。", 4000); return; }
    if (life.mood < 30) { say(L"陪我玩一会儿嘛～", 4000); return; }
    const wchar_t* lines[] = {L"坐久了，记得起来走走哦。", L"喝口水，休息一下眼睛吧～", L"今天的桌面也很整洁呢！", L"我在这儿安安静静陪着你。",
        L"要不要一起伸个懒腰？", L"加油，你已经做得很好啦！", L"摸摸我的刺，是软软的哦。", L"有空来小窝看看我吧～"};
    say(lines[randInt(0, 7)], 4200);
}

void PetApp::tick() {
    ULONGLONG now = GetTickCount64();
    updateLife(now);
    if (saveDue && now >= saveDue) saveNow();
    checkFullscreen(now);
    if (hidden || autoHidden || modal) { tickTime = now; return; }
    // 延迟重建位图，避免 DPI 消息在窗口移动过程中嵌套释放正在提交的画面。
    if (pendingDpi) {
        dpi = pendingDpi; pendingDpi = 0;
        if (!applyScale()) { DestroyWindow(hwnd); return; }
    }
    float elapsed = std::min(.05f, static_cast<float>(now - tickTime) / 1000.f); tickTime = now;
    updateHover(now);
    bool joyDone = happyUntil && now >= happyUntil && (animation.motion() != joyMotion || animation.finished());
    if (joyDone) { happyUntil = 0; feeding = false; }
    bool finishingIdle = idleAction >= 0 && animation.motion() == static_cast<Motion>(idleAction) && !animation.finished();
    if (idleAction >= 0 && now >= idleUntil && !finishingIdle) idleAction = -1;
    bool exercising = now < exerciseUntil;
    bool free = !dragging && !sleeping && !menuOpen && !life.sick && !happyUntil && idleAction < 0 && !exercising && !fileHover;
    if (free && !moving && std::abs(walkSpeed) < 1.f && now >= nextIdle) {
        int selected = 0;
        do {
            selected = randInt(0, 5);
            if (life.energy < 30 && randInt(0, 2) == 0) selected = randInt(0, 1);     // 困了多打哈欠、伸懒腰
            else if (life.mood > 80 && randInt(0, 3) == 0) selected = 3;              // 开心时更爱打滚
        } while (static_cast<int>(IDLE_CHOICES[selected]) == lastIdleAction);
        startIdle(IDLE_CHOICES[selected]); free = false;
    }
    if (free && !moving && now >= blinkStart) { blinkUntil = now + 380; blinkStart = now + static_cast<ULONGLONG>(randInt(2800, 6200)); }
    bool canWalk = cfg.roaming && !cfg.dockMode && free && !hovered && now >= roamPauseUntil;
    if (canWalk && now >= nextDecision) {
        moving = !moving;
        nextDecision = now + static_cast<ULONGLONG>(moving ? randInt(2300, 5200) : randInt(2600, 6500));
        if (moving && randInt(0, 3) == 0) direction = -direction;
    } else if (!canWalk) {
        moving = false;
        if (hovered) nextDecision = std::max(nextDecision, now + 1800);
    }
    float targetSpeed = canWalk && moving && !animation.turning() ? direction * 34.f * scale : 0.f;
    walkSpeed = Animation::approach(walkSpeed, targetSpeed, elapsed);
    if (std::abs(walkSpeed) < .05f || dragging || cfg.dockMode || sleeping) walkSpeed = 0;
    if (walkSpeed != 0) {
        RECT area = workArea(); double hw = halfWidth();
        petX += walkSpeed * elapsed;
        if (petX < area.left + hw) { petX = area.left + hw; direction = 1; walkSpeed = 0; }
        if (petX > area.right - hw) { petX = area.right - hw; direction = -1; walkSpeed = 0; }
    }
    // 停靠时平滑滑到屏幕边缘，而不是瞬移。
    if (cfg.dockMode && !dragging) {
        double target = dockTargetX();
        petX = Animation::approach(static_cast<float>(petX - target), 0.f, elapsed, 9.f) + target;
        if (std::abs(petX - target) < .5) petX = target;
    }
    int face = direction;
    if (lookDirection && !moving && std::abs(walkSpeed) < 1.f && !dragging && !sleeping) face = lookDirection;
    animation.advance(elapsed, requestedMotion(now), face);
    chatter(now);
    // 自适应帧率：安静站立/睡觉/养病时只需细微呼吸，隔帧绘制即可，占用减半；动作、移动、拖动时保持全速。
    Motion current = animation.motion();
    bool settled = (current == Motion::Stand) || (current == Motion::Sleep && animation.time() > 3.f) || (current == Motion::Sick && animation.time() > .7f);
    bool bubbleAnimating = now < bubbleUntil && (now - bubbleStart < 220 || bubbleUntil - now < 320);
    bool sliding = cfg.dockMode && !dragging && std::abs(petX - dockTargetX()) > .5;
    bool calm = cfg.fps == 60 && settled && !animation.blending() && !dragging && !moving && walkSpeed == 0 && !happyUntil &&
                idleAction < 0 && !fileHover && !bubbleAnimating && !sliding && !exercising;
    if (!calm || (++frameCounter & 1) == 0) render(now);
    panelTick(now);
}

void PetApp::command(int id) {
    if (id >= NAV_HOME && id <= NAV_HELP) { showPanel(id); return; }
    if (id >= FILE_ROW && id < FILE_ROW + 1000) return;   // 面板内部处理
    ULONGLONG now = GetTickCount64();
    if (modal && id != QUIT && id != SHOW) return;
    if (id >= BUY_FIRST && id < BUY_FIRST + 6) {
        int item = id - BUY_FIRST;
        const wchar_t* names[] = {L"养护药", L"营养片", L"逗逗球", L"小围巾", L"软软睡窝", L"运动铃"};
        if (life.buy(item)) {
            notice = std::wstring(L"购买了") + names[item] + (item < 2 ? L"，去照护页使用。" : L"，道具已永久解锁。");
            if (item >= 2) celebrate(item == 3 ? L"新围巾！暖暖的，好看吗？" : L"谢谢你的礼物！");
        } else notice = life.owned(item) ? L"这件道具已经拥有啦。" : L"金币不足或库存已满。每日礼物和文件喂食都能获得金币。";
        life.normalize(); requestSave(); refreshPanel(); return;
    }
    switch (id) {
    case PET: petReaction(false); break;
    case FEED: showPanel(NAV_FEED); return;
    case SLEEP:
        setSleeping(!sleeping);
        say(sleeping ? L"打个小盹，醒了再陪你～" : L"睡饱啦，精神满满！");
        notice = sleeping ? L"栗栗正在休息，精力恢复更快。" : L"栗栗醒来了。";
        break;
    case ROAM:
        cfg.roaming = !cfg.roaming; moving = false; nextDecision = now + 1000; roamPauseUntil = 0;
        if (cfg.roaming) cfg.dockMode = 0;
        say(cfg.roaming ? L"出去走两步～" : L"好，就在这里陪你。");
        notice = cfg.roaming ? L"自动散步已开启（会解除停靠）。" : L"自动散步已关闭。";
        break;
    case TOPMOST: cfg.topmost = !cfg.topmost; applyWindowStyle(); notice = cfg.topmost ? L"栗栗会保持在其他窗口上方。" : L"已取消置顶。"; break;
    case XSMALL: case SMALL: case MEDIUM: case LARGE: case XLARGE: {
        int previous = cfg.sizePercent;
        cfg.sizePercent = id == XSMALL ? 60 : id == SMALL ? 75 : id == MEDIUM ? 100 : id == LARGE ? 135 : 160;
        if (!applyScale()) {
            cfg.sizePercent = previous;
            if (!applyScale()) { MessageBoxW(hwnd, L"无法创建绘图窗口，请重新打开桌宠。", APP_TITLE, MB_ICONERROR); DestroyWindow(hwnd); return; }
        }
        notice = L"大小已调整为 " + std::to_wstring(cfg.sizePercent) + L"%。在栗栗身上按住 Ctrl 滚动滚轮也能调整。";
        break;
    }
    case HIDE: if (!hidden) toggleVisible(); break;
    case SHOW: reveal(); break;
    case HELP: showPanel(NAV_HELP); return;
    case QUIT: DestroyWindow(hwnd); return;
    case PANEL: showPanel(NAV_HOME); return;
    case DOCK_LEFT: case DOCK_RIGHT:
        cfg.dockMode = id == DOCK_LEFT ? 1 : 2; cfg.roaming = false; applyDock(false);
        notice = id == DOCK_LEFT ? L"栗栗已停靠在显示器左侧。" : L"栗栗已停靠在显示器右侧。"; break;
    case DOCK_FREE: cfg.dockMode = 0; notice = L"已解除停靠，可以自由拖动。"; break;
    case ESC_MODE: case ALT_MODE: case NO_HOTKEY: case DOUBLE_ESC_MODE:
        cfg.hotkeyMode = id == ESC_MODE ? HOTKEY_ESC : id == ALT_MODE ? HOTKEY_CTRL_ALT_H : id == NO_HOTKEY ? HOTKEY_OFF : HOTKEY_DOUBLE_ESC;
        configureHotkey(); notice = hotkeyStatus; break;
    case WATCH_BIN:
        cfg.watchBin = !cfg.watchBin; requestBinBaseline();
        notice = cfg.watchBin ? L"已开启回收站感应，只感应之后新增的项目。" : L"回收站感应已关闭，仍可拖入文件喂食。"; break;
    case PICK_FILES: pickFiles(false); return;
    case PICK_FOLDER: pickFiles(true); return;
    case CLEAR_FEED: pendingFiles.clear(); fileSelected.clear(); fileScroll = 0; notice = L"已清空待喂食列表，没有移动任何文件。"; break;
    case REMOVE_FEED: removeSelectedFiles(); break;
    case SELECT_ALL_FEED: fileSelected.assign(pendingFiles.size(), true); break;
    case CONFIRM_FEED: confirmFeeding(); return;
    case OPEN_BIN: ShellExecuteW(panel ? panel : hwnd, L"open", L"explorer.exe", L"shell:RecycleBinFolder", nullptr, SW_SHOWNORMAL); return;
    case USE_MED:
        if (life.takeMedicine()) { notice = L"已使用养护药，健康 +35，精力 +8。"; say(L"苦苦的……但是好多了！"); }
        else notice = L"没有养护药了，可以在小商店购买。";
        break;
    case USE_VIT:
        if (life.takeVitamins()) { notice = L"已使用营养片，健康 +12，精力 +20。"; say(L"嘎嘣脆，元气补充！"); }
        else notice = L"没有营养片了，可以在小商店购买。";
        break;
    case FREE_CARE:
        if (life.care()) { notice = L"温柔陪伴：健康 +8，心情 +10。五分钟后可再次照护。"; if (!life.sick && !sleeping) petReaction(true); else say(L"有你陪着真好。"); }
        else notice = L"刚刚照护过，再陪它休息一会儿吧。";
        break;
    case EXERCISE:
        if (now < exerciseUntil) { stopExercise(nullptr); notice = L"踏步已结束。"; break; }
        if (!life.canExercise()) notice = L"现在不适合运动，先休息、恢复健康和精力吧。";
        else {
            if (sleeping) setSleeping(false);
            happyUntil = 0; idleUntil = 0; idleAction = -1; exerciseUntil = now + 120000;
            notice = life.bell ? L"两分钟原地踏步开始：运动铃让消耗更多。" : L"两分钟原地踏步开始：会缓慢消耗体重和精力。";
            say(L"一二一，一二一！");
        }
        break;
    case STOP_EXERCISE: stopExercise(nullptr); notice = L"踏步已结束。"; break;
    case DAILY_GIFT:
        if (life.dailyGift(todayStamp())) { notice = L"今日礼物已领取：+20 金币。"; celebrate(L"今天的礼物～ +20 金币！"); }
        else notice = L"今天的礼物已经领取，明天再来吧。";
        break;
    case PLAY_TOY:
        if (!life.toy) notice = L"去小商店解锁逗逗球，就能一起玩啦。";
        else if (life.sick) notice = L"栗栗正在养病，恢复后再一起玩吧。";
        else {
            happyUntil = 0; exerciseUntil = 0; if (sleeping) setSleeping(false);
            startIdle(Motion::Roll); life.mood = std::min(100., life.mood + 8);
            notice = L"滚来滚去，心情变好啦！"; say(L"球球！接住啦～");
        }
        break;
    case TRY_ACTION: {
        if (life.sick) { notice = L"栗栗正在养病，恢复后再一起玩吧。"; break; }
        sleeping = false; autoSlept = false; happyUntil = 0; exerciseUntil = 0;
        Motion selected = DEMO_CHOICES[previewAction++ % 8];
        if (selected == Motion::Cheer || selected == Motion::Enjoy) {
            joyMotion = selected; animation.play(selected, true); idleAction = -1;
            happyUntil = now + static_cast<ULONGLONG>(1000 * Animation::duration(selected)); nextIdle = happyUntil + 4000;
        } else startIdle(selected);
        notice = std::wstring(L"正在表演：") + Animation::name(selected) + L"。再点一次，换个小动作。";
        break;
    }
    case WEAR_SCARF:
        if (!life.scarf) notice = L"还没有小围巾，可以去商店看看。";
        else { life.wearingScarf = !life.wearingScarf; notice = life.wearingScarf ? L"围巾戴好啦。" : L"小围巾已收进衣柜。"; if (life.wearingScarf) say(L"暖暖的～"); }
        break;
    case OPACITY_100: case OPACITY_85: case OPACITY_70: case OPACITY_55:
        cfg.opacity = id == OPACITY_100 ? 100 : id == OPACITY_85 ? 85 : id == OPACITY_70 ? 70 : 55;
        notice = L"不透明度：" + std::to_wstring(cfg.opacity) + L"%。"; break;
    case FPS_60: case FPS_30:
        cfg.fps = id == FPS_60 ? 60 : 30; updateTimer();
        notice = cfg.fps == 60 ? L"动画：流畅模式（约 60 帧）。" : L"动画：省电模式（约 30 帧），占用更低。"; break;
    case CLICK_THROUGH:
        cfg.clickThrough = !cfg.clickThrough; applyWindowStyle();
        notice = cfg.clickThrough ? L"鼠标穿透已开启：点击会穿过栗栗。可从托盘菜单或这里关闭。" : L"鼠标穿透已关闭，可以摸摸、拖动栗栗了。";
        if (cfg.clickThrough) say(L"我不挡你点东西啦～托盘里可以关掉穿透。", 4500);
        break;
    case FULLSCREEN_HIDE:
        cfg.fullscreenHide = !cfg.fullscreenHide;
        if (!cfg.fullscreenHide && autoHidden) { autoHidden = false; ShowWindow(hwnd, SW_SHOWNOACTIVATE); updateTimer(); }
        notice = cfg.fullscreenHide ? L"全屏看视频、玩游戏时，栗栗会自动躲起来。" : L"全屏时不再自动隐藏。"; break;
    case AUTOSTART: {
        bool target = !autostartEnabled();
        notice = setAutostart(target) ? (target ? L"已设置开机自动启动（仅当前用户）。" : L"已取消开机自动启动。") : L"无法修改开机启动设置。";
        break;
    }
    case OPEN_ON_START: cfg.openPanelOnStart = !cfg.openPanelOnStart; notice = cfg.openPanelOnStart ? L"启动时会打开生活馆。" : L"启动时只显示栗栗，不打开生活馆。"; break;
    case AUTO_REST: cfg.autoRest = !cfg.autoRest; notice = cfg.autoRest ? L"自动作息：精力见底会自己打盹，睡饱自己醒。" : L"自动作息已关闭。"; break;
    case CHATTER: cfg.chatter = !cfg.chatter; notice = cfg.chatter ? L"栗栗会偶尔和你聊两句。" : L"已关闭闲聊气泡（重要提醒仍会显示）。"; break;
    case RESET_POSITION: resetPosition(); notice = L"栗栗回到了屏幕右下角。"; break;
    case QUICK_MENU: { POINT p{}; GetCursorPos(&p); menu(p); return; }
    case CHEER_UP: celebrate(L"耶！"); break;
    default: return;
    }
    life.normalize(); requestSave(); refreshPanel();
    if (!hidden && !autoHidden) render(GetTickCount64());
}

void PetApp::removeSelectedFiles() {
    std::vector<std::wstring> kept;
    for (size_t i = 0; i < pendingFiles.size(); ++i) if (i >= fileSelected.size() || !fileSelected[i]) kept.push_back(pendingFiles[i]);
    size_t removed = pendingFiles.size() - kept.size();
    pendingFiles = std::move(kept); fileSelected.assign(pendingFiles.size(), false);
    fileScroll = std::clamp(fileScroll, 0, std::max(0, static_cast<int>(pendingFiles.size()) - fileRowsVisible()));
    notice = removed ? L"已从列表移除 " + std::to_wstring(removed) + L" 项（没有移动任何文件）。" : L"请先点选要移除的项目，或点击每行右侧的 ×。";
}

void PetApp::addFiles(const std::vector<std::wstring>& paths) {
    int rejected = 0, added = 0; std::wstring lastReason;
    for (const auto& input : paths) {
        if (pendingFiles.size() >= 100) { ++rejected; lastReason = L"每批最多 100 个项目。"; continue; }
        std::wstring normalized, reason;
        if (!safeCandidate(input, normalized, reason)) { ++rejected; lastReason = reason; continue; }
        bool covered = false;
        for (const auto& selected : pendingFiles) if (sameOrInside(normalized, selected)) { covered = true; break; }
        if (covered) continue;
        pendingFiles.erase(std::remove_if(pendingFiles.begin(), pendingFiles.end(), [&](const std::wstring& selected) { return sameOrInside(selected, normalized); }), pendingFiles.end());
        pendingFiles.push_back(normalized); ++added;
    }
    fileSelected.assign(pendingFiles.size(), false);
    if (!pendingFiles.empty()) ensureFileVisible(static_cast<int>(pendingFiles.size()) - 1);
    notice = rejected ? L"有 " + std::to_wstring(rejected) + L" 项未加入。" + lastReason : L"文件只加入了待确认列表，还没有移动。确认后才会移入回收站。";
    if (added && !hidden && !autoHidden && !sleeping) say(L"闻到好吃的了！等你确认哦～", 3000);
    showPanel(NAV_FEED);
}

void PetApp::pickFiles(bool folder) {
    if (modal) return;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) { notice = L"无法打开文件选择窗口。"; refreshPanel(); return; }
    DWORD options = 0; dialog->GetOptions(&options);
    HRESULT configured = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT | (folder ? static_cast<DWORD>(FOS_PICKFOLDERS) : 0u));
    if (FAILED(configured)) { dialog->Release(); notice = L"文件选择窗口初始化失败，没有执行文件操作。"; refreshPanel(); return; }
    dialog->SetTitle(folder ? L"选择不再需要的文件夹（稍后确认回收）" : L"选择不再需要的文件（稍后确认回收）");
    modal = true; releaseHotkey();
    HRESULT result = dialog->Show(panel ? panel : hwnd);
    std::vector<std::wstring> paths;
    if (SUCCEEDED(result)) {
        IShellItemArray* selected = nullptr;
        if (SUCCEEDED(dialog->GetResults(&selected))) {
            DWORD count = 0; selected->GetCount(&count);
            for (DWORD i = 0; i < std::min<DWORD>(100, count); ++i) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(selected->GetItemAt(i, &item))) { auto path = shellPath(item); if (!path.empty()) paths.push_back(path); item->Release(); }
            }
            selected->Release();
        }
    }
    dialog->Release(); modal = false; statClock = tickTime = GetTickCount64(); configureHotkey();
    if (!paths.empty()) addFiles(paths); else refreshPanel();
}

void PetApp::confirmFeeding() {
    if (modal || pendingFiles.empty()) return;
    std::wstring question = L"将列表中的 " + std::to_wstring(pendingFiles.size()) + L" 个项目移入 Windows 回收站？\n\n";
    for (size_t i = 0; i < std::min<size_t>(5, pendingFiles.size()); ++i) question += pendingFiles[i] + L"\n";
    if (pendingFiles.size() > 5) question += L"其余项目已完整列在喂食面板中。\n";
    question += L"\n文件夹包含其中的内容。确认后，原位置将不再显示这些项目，可从回收站恢复。\n仅成功回收的项目计入喂食。";
    modal = true; releaseHotkey();
    HWND owner = panel && IsWindowVisible(panel) ? panel : hwnd;
    int answer = MessageBoxW(owner, question.c_str(), L"确认文件喂食", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (answer == IDYES) {
        auto successes = recycleConfirmed(owner, pendingFiles);
        int count = static_cast<int>(successes.size());
        pendingFiles.erase(std::remove_if(pendingFiles.begin(), pendingFiles.end(), [&](const std::wstring& path) { return successes.count(pathKey(path)) != 0; }), pendingFiles.end());
        fileSelected.assign(pendingFiles.size(), false); fileScroll = 0;
        modal = false;
        life.feedRecycled(count); if (count) happy(true);
        notice = L"已确认回收 " + std::to_wstring(count) + L" 项" + (pendingFiles.empty() ? L"。可在系统回收站恢复。" : L"；未确认回收的项目仍留在列表，请检查原位置和回收站。");
        requestBinBaseline(); requestSave(200);
    } else notice = L"已取消喂食，没有执行文件操作。";
    modal = false; statClock = tickTime = GetTickCount64(); configureHotkey(); refreshPanel();
}
