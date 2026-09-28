#pragma once
#include "win_common.h"
#include "gfx.h"
#include "sprites.h"
#include "settings.h"
#include "animation.h"
#include <random>

class DropTarget;

// 面板里的一个自绘按钮（逻辑坐标，100% 缩放）。
struct UiButton {
    enum Kind { Primary, Secondary, Tab, Segment, Toggle, Danger, Ghost, Row, RowDelete };
    int id = 0;
    Gdiplus::RectF rect;
    std::wstring text;
    Kind kind = Secondary;
    bool enabled = true, selected = false;
};

struct PetApp {
    // ---------- 基础 ----------
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr, panel = nullptr, menuHost = nullptr;
    HICON icon = nullptr, smallIcon = nullptr;
    bool testing = false, autostartLaunch = false;
    std::wstring configPath;
    Settings cfg;
    PetLife& life = cfg.life;
    std::mt19937 random{static_cast<unsigned>(GetTickCount64())};
    int randInt(int low, int high) { return std::uniform_int_distribution<int>(low, high)(random); }

    // ---------- 画面 ----------
    SpriteBank sprites;
    Canvas canvas;
    FontCache petFonts;
    Animation::Player animation;
    Animation::Pose bodyPose;
    int dpi = 96, pendingDpi = 0;
    float scale = 1.f;               // 逻辑像素 → 屏幕像素
    double petX = 0, petY = 0;       // 脚底锚点（屏幕坐标）
    int canvasLeft = 0, canvasTop = 0;
    float bodyX = PET_W / 2;         // 身体中心在画布内的逻辑 x
    int direction = -1;              // 行走/面向：1 向右，-1 向左
    float walkSpeed = 0;
    std::wstring bubble; ULONGLONG bubbleStart = 0, bubbleUntil = 0;
    Canvas bubbleCanvas; std::wstring bubbleCacheText; float bubbleCacheScale = 0, bubbleW = 0, bubbleH = 0;
    unsigned frameCounter = 0;
    bool presentedOnce = false;

    // ---------- 状态 ----------
    bool hidden = false, autoHidden = false, modal = false, menuOpen = false, trayReady = false;
    bool sleeping = false, autoSlept = false, moving = false, feeding = false;
    bool dragging = false, dragMoved = false, fileHover = false, hovered = false;
    std::vector<std::wstring> droppedPaths;
    POINT pressPoint{}; double dragOffsetX = 0, dragOffsetY = 0;
    Animation::Motion joyMotion = Animation::Motion::Hop;
    int idleAction = -1, lastIdleAction = -1, previewAction = 0;
    ULONGLONG tickTime = 0, statClock = 0, saveDue = 0, lastSave = 0;
    ULONGLONG nextDecision = 0, blinkStart = 0, blinkUntil = 0, happyUntil = 0, idleUntil = 0, nextIdle = 0;
    ULONGLONG exerciseUntil = 0, roamPauseUntil = 0, nextChatter = 0, lastFullscreenCheck = 0, lookUntil = 0;
    ULONGLONG rubCooldown = 0, lastRubReversal = 0;
    int rubReversals = 0, rubDirection = 0; LONG rubLastX = 0;
    int lookDirection = 0; ULONGLONG lookCandidateSince = 0; int lookCandidate = 0;
    int greetedPeriod = -1;

    // ---------- 系统集成 ----------
    UINT taskbarMessage = 0, showMessage = 0;
    std::wstring hotkeyStatus;
    int actualHotkey = -1;           // 实际生效的 HotkeyMode，-1 表示无
    bool hotkeyReady = false;
    HANDLE binThread = nullptr, binWake = nullptr, binStop = nullptr;
    LONGLONG binBaseline = -1; volatile LONG binBaselineRequest = 1;
    DropTarget* petDrop = nullptr; DropTarget* panelDrop = nullptr;
    bool oleReady = false;

    // ---------- 生活馆面板 ----------
    int page = NAV_HOME;
    float uiScale = 1.f;
    Canvas panelStatic, panelFrame;
    FontCache uiFonts;
    bool panelDirty = true, keyboardFocus = false, panelWasVisible = false;
    std::vector<UiButton> buttons;
    int hoverId = 0, pressedId = 0, focusId = 0;
    std::vector<std::wstring> pendingFiles;
    std::vector<bool> fileSelected;
    int fileScroll = 0, fileAnchor = -1, hoverFile = -1;
    std::wstring notice = L"今天也一起把桌面整理得干干净净。";
    ULONGLONG lastPanelFrame = 0;

    // ===== app.cpp：核心行为 =====
    bool initialize(HINSTANCE module, bool testMode);
    bool start();
    void shutdown();
    void tick();
    void updateLife(ULONGLONG now);
    void command(int id);
    void handlePanelCommand(int id);
    void say(const std::wstring& text, int duration = 3300);
    void happy(bool food);
    void celebrate(const wchar_t* line);
    void petReaction(bool rub);
    void startIdle(Animation::Motion motion);
    Animation::Motion requestedMotion(ULONGLONG now) const;
    void chatter(ULONGLONG now);
    void requestSave(int delayMs = 1500);
    void saveNow();
    void setSleeping(bool value, bool automatic = false);
    void stopExercise(const wchar_t* reason);
    void confirmFeeding();
    void addFiles(const std::vector<std::wstring>& paths);
    void pickFiles(bool folder);
    void removeSelectedFiles();

    // ===== pet_window.cpp：桌宠窗口 =====
    bool applyScale();
    RECT workAreaAt(double x, double y) const;
    RECT workArea() const { return workAreaAt(petX, petY - 100 * scale); }
    float halfWidth() const { return BODY_HALF * static_cast<float>(life.bodyScale()) * scale; }
    double dockTargetX() const;
    void clampPosition();
    void placeCanvas();
    void settleAfterDrag();
    void applyDock(bool instant);
    void resetPosition();
    void draw(ULONGLONG now, int forcedFrame = -1);
    void drawScarf(Gdiplus::Graphics& g, const Animation::Pose& pose);
    void drawBubble(Gdiplus::Graphics& g, ULONGLONG now);
    void paintBody(Gdiplus::Graphics& g, float center, float ground, float zoom, bool withScarf, Canvas* target = nullptr, const PixelBox* clip = nullptr);
    bool present();
    void render(ULONGLONG now) { draw(now); present(); }
    bool cursorOverPet(POINT cursor) const;
    void updateHover(ULONGLONG now);
    void reveal();
    void toggleVisible();
    void applyWindowStyle();
    void updateTimer();

    // ===== system.cpp：托盘、热键、回收站、自启、全屏、拖放 =====
    void addTray();
    void removeTray();
    void menu(POINT point);
    void configureHotkey();
    void releaseHotkey();
    bool startBinWatcher();
    void stopBinWatcher();
    void onBinCount(bool ok, LONGLONG count);
    void requestBinBaseline();
    static bool autostartEnabled();
    static bool setAutostart(bool enable);
    bool fullscreenForeground() const;
    void checkFullscreen(ULONGLONG now);
    void registerDropTargets();
    void revokeDropTargets();
    void revokePanelDrop();
    void fileHoverChanged(bool over);

    // ===== panel.cpp：生活馆 =====
    void showPanel(int tab = NAV_HOME);
    void refreshPanel();
    void hidePanel();
    void layoutPanel();
    void paintPanelStatic(Gdiplus::Graphics& g);
    void paintPortrait(Gdiplus::Graphics& g, Canvas* target = nullptr, const PixelBox* clip = nullptr);
    void paintPortraitLabel(Gdiplus::Graphics& g);
    void panelPaint(HDC dc, const RECT& area);
    const UiButton* buttonAt(float x, float y) const;
    void focusNext(int step);
    void activate(int id);
    void panelTick(ULONGLONG now);
    Gdiplus::RectF portraitRect() const { return Gdiplus::RectF(36, 236, 272, 284); }
    Gdiplus::RectF portraitLabelRect() const { return Gdiplus::RectF(44, 522, 256, 26); }
    int portraitLabelMotion = -1;
    Gdiplus::RectF fileListRect() const { return Gdiplus::RectF(40, 372, 740, 180); }
    int fileRowsVisible() const { return static_cast<int>(fileListRect().Height / 30); }
    void ensureFileVisible(int index);

    ~PetApp();
};

extern PetApp* g_app;
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PanelProc(HWND, UINT, WPARAM, LPARAM);
