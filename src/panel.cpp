// 生活馆面板：整窗自绘（无子控件，不再每次操作销毁重建按钮），静态层缓存 + 仅刷新动画区域。
#include "app.h"

using namespace Gdiplus;

namespace Ui {
constexpr float W = 820.f, H = 720.f;
const Color background(255, 247, 243, 236), paper(255, 255, 253, 249), ink(255, 62, 54, 45);
const Color muted(255, 137, 126, 111), sage(255, 84, 122, 98), sageDark(255, 62, 95, 75), pale(255, 230, 238, 227);
const Color line(255, 231, 222, 209), warm(255, 241, 231, 213), gold(255, 201, 150, 72), danger(255, 184, 88, 72);
const Color disabledFill(255, 234, 230, 223), white(255, 255, 255, 255);

void text(Graphics& g, FontCache& fonts, const std::wstring& value, float x, float y, float w, float h, float size,
          const Color& color, bool bold = false, StringAlignment align = StringAlignmentNear, bool wrap = false,
          StringTrimming trimming = StringTrimmingEllipsisCharacter) {
    SolidBrush brush(color); StringFormat format;
    format.SetAlignment(align); format.SetLineAlignment(wrap ? StringAlignmentNear : StringAlignmentCenter);
    format.SetTrimming(trimming);
    if (!wrap) format.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(value.c_str(), -1, fonts.get(size, bold), RectF(x, y, w, h), &format, &brush);
}
void card(Graphics& g, float x, float y, float w, float h, const Color& fill = paper, float r = 18) {
    Draw::softShadow(g, x, y, w, h, r, 14);
    Draw::fillRound(g, x, y, w, h, r, fill);
}
void meter(Graphics& g, FontCache& fonts, const wchar_t* name, double value, float x, float y, float width, const Color& color) {
    text(g, fonts, name, x, y, width - 90, 22, 14, muted);
    text(g, fonts, std::to_wstring(static_cast<int>(std::round(value))) + L" / 100", x + width - 90, y, 90, 22, 13, ink, true, StringAlignmentFar);
    Draw::fillRound(g, x, y + 28, width, 10, 5, line);
    float filled = static_cast<float>(std::clamp(value, 0., 100.) / 100) * width;
    if (filled > 1) {
        LinearGradientBrush brush(PointF(x, 0), PointF(x + width, 0), Draw::withAlpha(color, .78f), color);
        GraphicsPath path; Draw::roundRect(path, x, y + 28, filled, 10, std::min(5.f, filled / 2)); g.FillPath(&brush, &path);
    }
}
void coin(Graphics& g, float cx, float cy, float r) {
    LinearGradientBrush brush(PointF(cx, cy - r), PointF(cx, cy + r), Color(255, 244, 205, 110), Color(255, 214, 158, 66));
    g.FillEllipse(&brush, cx - r, cy - r, r * 2, r * 2);
    Pen rim(Color(255, 190, 134, 52), 1.4f); g.DrawEllipse(&rim, cx - r * .62f, cy - r * .62f, r * 1.24f, r * 1.24f);
}
// 标签页小图标
void tabIcon(Graphics& g, int index, float x, float y, const Color& color) {
    Pen pen(color, 1.8f); pen.SetLineJoin(LineJoinRound); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
    switch (index) {
    case 0: { PointF roof[] = {PointF(x, y + 7), PointF(x + 8, y), PointF(x + 16, y + 7)}; g.DrawLines(&pen, roof, 3); g.DrawRectangle(&pen, x + 2.5f, y + 7, 11.f, 9.f); break; }
    case 1: { PointF doc[] = {PointF(x + 2, y), PointF(x + 10, y), PointF(x + 14, y + 4), PointF(x + 14, y + 16), PointF(x + 2, y + 16), PointF(x + 2, y)}; g.DrawLines(&pen, doc, 6); g.DrawLine(&pen, x + 5, y + 9, x + 11, y + 9); g.DrawLine(&pen, x + 5, y + 12.5f, x + 10, y + 12.5f); break; }
    case 2: g.DrawRectangle(&pen, x + 1, y + 5, 14.f, 11.f); g.DrawArc(&pen, x + 4.5f, y, 7.f, 9.f, 180, 180); break;
    case 3: Draw::heart(g, x + 8, y + 8, 8.5f, color.GetA()); break;
    default: g.DrawEllipse(&pen, x + 3, y + 3, 10.f, 10.f); for (int i = 0; i < 8; ++i) { float a = i * 3.14159f / 4; g.DrawLine(&pen, x + 8 + 5.2f * std::cos(a), y + 8 + 5.2f * std::sin(a), x + 8 + 7.6f * std::cos(a), y + 8 + 7.6f * std::sin(a)); } break;
    }
}
void shopIcon(Graphics& g, int id, float x, float y) {
    Draw::fillRound(g, x, y, 50, 50, 15, pale);
    float cx = x + 25, cy = y + 25;
    switch (id) {
    case 0: { SolidBrush red(Color(255, 214, 96, 86)); Draw::fillRound(g, cx - 14, cy - 14, 28, 28, 7, Color(255, 255, 255, 255)); g.FillRectangle(&red, cx - 4, cy - 10, 8.f, 20.f); g.FillRectangle(&red, cx - 10, cy - 4, 20.f, 8.f); break; }
    case 1: { GraphicsState s = g.Save(); g.TranslateTransform(cx, cy); g.RotateTransform(-35);
              Draw::fillRound(g, -15, -7, 30, 14, 7, Color(255, 246, 190, 92)); GraphicsPath half; Draw::roundRect(half, -15, -7, 15, 14, 7); SolidBrush o(Color(255, 232, 128, 70)); g.FillPath(&o, &half);
              g.Restore(s); break; }
    case 2: { SolidBrush ball(Color(255, 110, 164, 214)); g.FillEllipse(&ball, cx - 14, cy - 14, 28.f, 28.f); Pen stripe(Color(255, 255, 238, 150), 3.f); g.DrawArc(&stripe, cx - 14, cy - 6, 28.f, 12.f, 0, 180); SolidBrush shine(Color(170, 255, 255, 255)); g.FillEllipse(&shine, cx - 8, cy - 10, 7.f, 5.f); break; }
    case 3: { SolidBrush red(Color(255, 200, 70, 64)); GraphicsPath band; band.AddBezier(cx - 16, cy - 8, cx - 6, cy - 2, cx + 6, cy - 2, cx + 16, cy - 8); band.AddLine(cx + 16, cy - 8, cx + 15, cy); band.AddBezier(cx + 15, cy, cx + 6, cy + 6, cx - 6, cy + 6, cx - 15, cy); band.CloseFigure(); g.FillPath(&red, &band);
              PointF tail[] = {PointF(cx + 3, cy + 2), PointF(cx + 10, cy + 2), PointF(cx + 12, cy + 16), PointF(cx + 5, cy + 16)}; g.FillPolygon(&red, tail, 4); break; }
    case 4: { SolidBrush cushion(Color(255, 170, 196, 152)); g.FillEllipse(&cushion, cx - 18, cy - 4, 36.f, 16.f); SolidBrush inner(Color(255, 226, 238, 214)); g.FillEllipse(&inner, cx - 12, cy - 2, 24.f, 8.f); SolidBrush moon(Color(255, 240, 196, 96)); g.FillEllipse(&moon, cx + 4, cy - 18, 10.f, 10.f); SolidBrush cut(pale); g.FillEllipse(&cut, cx + 7, cy - 20, 9.f, 9.f); break; }
    default: { SolidBrush goldBrush(Color(255, 232, 186, 78)); GraphicsPath bell; bell.AddBezier(cx - 12, cy + 8, cx - 10, cy - 14, cx + 10, cy - 14, cx + 12, cy + 8); bell.CloseFigure(); g.FillPath(&goldBrush, &bell);
              g.FillRectangle(&goldBrush, cx - 14, cy + 7, 28.f, 4.f); SolidBrush clap(Color(255, 170, 120, 50)); g.FillEllipse(&clap, cx - 3, cy + 10, 6.f, 6.f); break; }
    }
}
} // namespace Ui

void PetApp::showPanel(int tab) {
    page = std::clamp(tab, NAV_HOME, NAV_HELP);
    if (hidden) reveal();
    if (!panel) {
        WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.style = CS_DBLCLKS; cls.lpfnWndProc = PanelProc;
        cls.hInstance = instance; cls.hIcon = icon; cls.hIconSm = smallIcon; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = PANEL_CLASS; RegisterClassExW(&cls);
        RECT work = workArea();
        uiScale = std::max(.6f, std::min({dpi / 96.f, (work.right - work.left - 40) / Ui::W, (work.bottom - work.top - 60) / Ui::H}));
        RECT desired{0, 0, static_cast<LONG>(Ui::W * uiScale), static_cast<LONG>(Ui::H * uiScale)};
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectEx(&desired, style, FALSE, WS_EX_APPWINDOW);
        int width = desired.right - desired.left, height = desired.bottom - desired.top;
        panel = CreateWindowExW(WS_EX_APPWINDOW, PANEL_CLASS, L"栗栗的生活馆 · 3.0", style,
            work.left + (work.right - work.left - width) / 2, work.top + (work.bottom - work.top - height) / 2, width, height,
            nullptr, nullptr, instance, this);
        if (!panel) { say(L"面板未能打开，请重新运行桌宠。"); return; }
        registerDropTargets();
    }
    hoverId = pressedId = 0; panelDirty = true;
    if (!testing) {
        ShowWindow(panel, IsIconic(panel) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(panel);
    }
    InvalidateRect(panel, nullptr, FALSE);
}

void PetApp::refreshPanel() {
    if (!panel) return;
    panelDirty = true; InvalidateRect(panel, nullptr, FALSE);
}

void PetApp::hidePanel() {
    if (!panel) return;
    ShowWindow(panel, SW_HIDE); panelWasVisible = false; pressedId = hoverId = 0;
}

void PetApp::ensureFileVisible(int index) {
    int rows = fileRowsVisible();
    if (index < fileScroll) fileScroll = index;
    if (index >= fileScroll + rows) fileScroll = index - rows + 1;
    fileScroll = std::clamp(fileScroll, 0, std::max(0, static_cast<int>(pendingFiles.size()) - rows));
}

void PetApp::layoutPanel() {
    buttons.clear();
    auto add = [&](int id, const std::wstring& label, float x, float y, float w, float h, UiButton::Kind kind = UiButton::Secondary, bool enabled = true, bool selected = false) {
        UiButton b; b.id = id; b.rect = RectF(x, y, w, h); b.text = label; b.kind = kind; b.enabled = enabled; b.selected = selected; buttons.push_back(b);
    };
    const wchar_t* tabs[] = {L"小窝", L"文件喂食", L"小商店", L"照护", L"设置"};
    for (int i = 0; i < 5; ++i) add(NAV_HOME + i, tabs[i], 32 + i * 154.f, 106, 146, 40, UiButton::Tab, true, page == NAV_HOME + i);
    add(NAV_HELP, L"使用说明", 488, 34, 104, 34, UiButton::Ghost, true, page == NAV_HELP);
    ULONGLONG now = GetTickCount64();
    switch (page) {
    case NAV_HOME: {
        bool gift = life.giftDate < todayStamp();
        add(DAILY_GIFT, gift ? L"领取每日 +20 金币" : L"今日礼物已领取", 44, 556, 256, 38, gift ? UiButton::Primary : UiButton::Secondary);
        add(TRY_ACTION, L"看看小动作（共 8 种）", 28, 620, 288, 40);
        add(PET, L"摸摸它", 332, 620, 106, 40);
        add(SLEEP, sleeping ? L"叫醒它" : L"睡一觉", 446, 620, 106, 40);
        add(NAV_FEED, L"去喂食", 560, 620, 106, 40);
        add(NAV_CARE, L"去照护", 674, 620, 118, 40);
        break;
    }
    case NAV_FEED: {
        add(PICK_FILES, L"选择文件", 48, 266, 140, 36, UiButton::Primary);
        add(PICK_FOLDER, L"选择文件夹", 198, 266, 140, 36);
        bool any = !pendingFiles.empty();
        add(SELECT_ALL_FEED, L"全选", 500, 334, 80, 30, UiButton::Ghost, any);
        add(REMOVE_FEED, L"移除选中", 588, 334, 96, 30, UiButton::Ghost, any);
        add(CLEAR_FEED, L"清空列表", 692, 334, 88, 30, UiButton::Ghost, any);
        RectF list = fileListRect();
        int rows = fileRowsVisible();
        for (int i = 0; i < rows && fileScroll + i < static_cast<int>(pendingFiles.size()); ++i) {
            int index = fileScroll + i;
            bool selected = index < static_cast<int>(fileSelected.size()) && fileSelected[index];
            add(FILE_ROW + index, pendingFiles[index], list.X, list.Y + i * 30.f, list.Width - 44, 30, UiButton::Row, true, selected);
            add(FILE_DEL + index, L"×", list.X + list.Width - 38, list.Y + i * 30.f + 3, 30, 24, UiButton::RowDelete);
        }
        add(CONFIRM_FEED, L"确认移入回收站并喂食", 28, 620, 330, 44, UiButton::Primary, any);
        add(OPEN_BIN, L"打开系统回收站", 370, 620, 200, 44);
        add(WATCH_BIN, cfg.watchBin ? L"回收站感应：开" : L"回收站感应：关", 582, 620, 210, 44, UiButton::Toggle, true, cfg.watchBin);
        break;
    }
    case NAV_SHOP:
        for (int i = 0; i < 6; ++i) {
            float x = 28 + (i % 3) * 258.f, y = 164 + (i / 3) * 222.f;
            bool owned = life.owned(i);
            std::wstring label = owned ? L"已拥有" : L"购买 · " + std::to_wstring(PetLife::price(i)) + L" 金币";
            add(BUY_FIRST + i, label, x + 16, y + 158, 214, 36, UiButton::Primary, !owned && life.coins >= PetLife::price(i));
            if (!owned && life.coins < PetLife::price(i)) buttons.back().text = L"还差 " + std::to_wstring(PetLife::price(i) - life.coins) + L" 金币";
        }
        break;
    case NAV_CARE: {
        add(USE_MED, L"用药 · 库存 " + std::to_wstring(life.medicine), 44, 404, 164, 38, UiButton::Secondary, life.medicine > 0);
        add(USE_VIT, L"营养片 · 库存 " + std::to_wstring(life.vitamins), 218, 404, 168, 38, UiButton::Secondary, life.vitamins > 0);
        add(FREE_CARE, life.careCooldown > 0 ? L"陪伴照护（冷却中）" : L"免费陪伴照护", 44, 452, 342, 38, UiButton::Primary, life.careCooldown <= 0);
        add(SLEEP, sleeping ? L"结束休息" : L"休息恢复精力", 44, 500, 342, 38);
        bool exercising = now < exerciseUntil;
        add(EXERCISE, exercising ? L"结束踏步（还剩 " + std::to_wstring((exerciseUntil - now + 999) / 1000) + L" 秒）" : L"开始两分钟踏步",
            434, 404, 342, 38, exercising ? UiButton::Secondary : UiButton::Primary, exercising || life.canExercise());
        add(PLAY_TOY, life.toy ? L"玩逗逗球" : L"逗逗球未解锁", 434, 452, 166, 38, UiButton::Secondary, life.toy);
        add(WEAR_SCARF, life.wearingScarf ? L"收起围巾" : L"戴上围巾", 610, 452, 166, 38, UiButton::Secondary, life.scarf);
        add(NAV_SHOP, L"去商店补充道具", 434, 500, 342, 38);
        break;
    }
    case NAV_SETTINGS: {
        const float x0 = 170;
        struct Seg { int id; const wchar_t* label; bool on; };
        auto row = [&](float y, float w, std::initializer_list<Seg> items) {
            float x = x0;
            for (const auto& s : items) { add(s.id, s.label, x, y, w, 36, UiButton::Segment, true, s.on); x += w + 8; }
        };
        row(178, 142, {{DOUBLE_ESC_MODE, L"双击 Esc", cfg.hotkeyMode == HOTKEY_DOUBLE_ESC}, {ESC_MODE, L"Esc（独占）", cfg.hotkeyMode == HOTKEY_ESC},
                       {ALT_MODE, L"Ctrl+Alt+H", cfg.hotkeyMode == HOTKEY_CTRL_ALT_H}, {NO_HOTKEY, L"关闭", cfg.hotkeyMode == HOTKEY_OFF}});
        row(252, 194, {{DOCK_LEFT, L"停靠左侧", cfg.dockMode == 1}, {DOCK_RIGHT, L"停靠右侧", cfg.dockMode == 2}, {DOCK_FREE, L"自由位置", cfg.dockMode == 0}});
        row(302, 110, {{XSMALL, L"60%", cfg.sizePercent == 60}, {SMALL, L"75%", cfg.sizePercent == 75}, {MEDIUM, L"100%", cfg.sizePercent == 100},
                       {LARGE, L"135%", cfg.sizePercent == 135}, {XLARGE, L"160%", cfg.sizePercent == 160}});
        row(352, 142, {{OPACITY_100, L"100%", cfg.opacity == 100}, {OPACITY_85, L"85%", cfg.opacity == 85}, {OPACITY_70, L"70%", cfg.opacity == 70}, {OPACITY_55, L"55%", cfg.opacity == 55}});
        row(402, 194, {{FPS_60, L"流畅 · 60 帧", cfg.fps == 60}, {FPS_30, L"省电 · 30 帧", cfg.fps == 30}});
        struct Tog { int id; const wchar_t* label; bool on; };
        Tog toggles[] = {{ROAM, L"自动散步", cfg.roaming}, {TOPMOST, L"保持置顶", cfg.topmost}, {WATCH_BIN, L"回收站感应", cfg.watchBin},
                         {CLICK_THROUGH, L"鼠标穿透", cfg.clickThrough}, {FULLSCREEN_HIDE, L"全屏时隐藏", cfg.fullscreenHide}, {AUTOSTART, L"开机自启", testing ? false : autostartEnabled()},
                         {OPEN_ON_START, L"启动时打开生活馆", cfg.openPanelOnStart}, {AUTO_REST, L"自动作息", cfg.autoRest}, {CHATTER, L"闲聊气泡", cfg.chatter}};
        for (int i = 0; i < 9; ++i) add(toggles[i].id, toggles[i].label, x0 + (i % 3) * 206.f, 454 + (i / 3) * 44.f, 198, 36, UiButton::Toggle, true, toggles[i].on);
        add(RESET_POSITION, L"回到右下角", 404, 620, 124, 40);
        add(HIDE, L"隐藏桌宠", 536, 620, 120, 40);
        add(QUIT, L"退出桌宠", 664, 620, 128, 40, UiButton::Danger);
        break;
    }
    default: add(NAV_HOME, L"回到小窝", 664, 620, 128, 40, UiButton::Primary); break;
    }
}

const UiButton* PetApp::buttonAt(float x, float y) const {
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it) if (it->rect.Contains(x, y)) return &*it;
    return nullptr;
}

static void paintButton(Graphics& g, FontCache& fonts, const UiButton& b, bool hover, bool pressed, bool focused) {
    using namespace Ui;
    const RectF& r = b.rect;
    Color fill = pale, textColor = sage, border(0, 0, 0, 0);
    float size = 13.5f; bool bold = true;
    switch (b.kind) {
    case UiButton::Primary: fill = b.enabled ? (pressed ? sageDark : hover ? Color(255, 74, 110, 88) : sage) : disabledFill; textColor = b.enabled ? white : muted; size = 14; break;
    case UiButton::Secondary: fill = pressed ? Color(255, 214, 226, 210) : hover ? Color(255, 221, 232, 217) : pale; textColor = b.enabled ? sage : muted; if (!b.enabled) fill = disabledFill; break;
    case UiButton::Tab: fill = b.selected ? sage : pressed ? Color(255, 214, 226, 210) : hover ? Color(255, 221, 232, 217) : pale; textColor = b.selected ? white : sage; size = 14; break;
    case UiButton::Segment: fill = b.selected ? sage : pressed ? Color(255, 232, 238, 228) : hover ? Color(255, 246, 244, 238) : paper;
        textColor = b.selected ? white : ink; border = b.selected ? Color(0, 0, 0, 0) : line; bold = b.selected; break;
    case UiButton::Toggle: fill = pressed ? Color(255, 232, 238, 228) : hover ? Color(255, 246, 244, 238) : paper; textColor = ink; border = b.selected ? Color(255, 170, 196, 176) : line; bold = false; break;
    case UiButton::Danger: fill = pressed ? Color(255, 236, 206, 198) : hover ? Color(255, 242, 216, 208) : Color(255, 247, 228, 222); textColor = danger; break;
    case UiButton::Ghost: fill = b.selected ? pale : pressed ? Color(255, 232, 238, 228) : hover ? Color(255, 240, 240, 232) : Color(0, 0, 0, 0);
        textColor = b.enabled ? sage : Color(255, 190, 182, 170); border = b.enabled ? Color(255, 205, 216, 200) : line; size = 12.5f; break;
    case UiButton::Row: fill = b.selected ? Color(255, 226, 236, 222) : hover ? Color(255, 246, 242, 234) : Color(0, 0, 0, 0); textColor = ink; bold = false; size = 13; break;
    case UiButton::RowDelete: fill = pressed ? Color(255, 236, 206, 198) : hover ? Color(255, 244, 222, 214) : Color(0, 0, 0, 0); textColor = hover ? danger : muted; size = 15; break;
    }
    float radius = b.kind == UiButton::Row ? 8.f : b.kind == UiButton::RowDelete ? 12.f : std::min(12.f, r.Height / 2.6f);
    if (fill.GetA()) Draw::fillRound(g, r.X, r.Y, r.Width, r.Height, radius, fill);
    if (border.GetA()) Draw::strokeRound(g, r.X + .5f, r.Y + .5f, r.Width - 1, r.Height - 1, radius, border, 1.f);
    if (b.kind == UiButton::Tab) {
        float tw = 16 + 8 + static_cast<float>(b.text.size()) * 14.f;
        float ix = r.X + (r.Width - tw) / 2;
        Ui::tabIcon(g, b.id - NAV_HOME, ix, r.Y + (r.Height - 16) / 2, textColor);
        text(g, fonts, b.text, ix + 24, r.Y, r.Width, r.Height, size, textColor, true);
    } else if (b.kind == UiButton::Toggle) {
        float sx = r.X + 12, sy = r.Y + (r.Height - 18) / 2;
        Draw::fillRound(g, sx, sy, 32, 18, 9, b.selected ? sage : Color(255, 208, 202, 192));
        SolidBrush knob(white); g.FillEllipse(&knob, b.selected ? sx + 16 : sx + 2, sy + 2, 14.f, 14.f);
        text(g, fonts, b.text, sx + 42, r.Y, r.Width - 58, r.Height, 13.5f, textColor, false);
    } else if (b.kind == UiButton::RowDelete) {
        Pen cross(textColor, 1.7f); cross.SetStartCap(LineCapRound); cross.SetEndCap(LineCapRound);
        float cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
        g.DrawLine(&cross, cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f); g.DrawLine(&cross, cx + 4.5f, cy - 4.5f, cx - 4.5f, cy + 4.5f);
    } else if (b.kind == UiButton::Row) {
        Pen dot(Color(255, 190, 176, 150), 1.f);
        SolidBrush bullet(b.selected ? sage : Color(255, 205, 192, 170)); g.FillEllipse(&bullet, r.X + 12, r.Y + 12, 6.f, 6.f);
        text(g, fonts, b.text, r.X + 28, r.Y, r.Width - 34, r.Height, size, textColor, false, StringAlignmentNear, false, StringTrimmingEllipsisPath);
    } else {
        text(g, fonts, b.text, r.X + 6, r.Y, r.Width - 12, r.Height, size, textColor, bold, StringAlignmentCenter);
    }
    if (focused) Draw::strokeRound(g, r.X - 2.5f, r.Y - 2.5f, r.Width + 5, r.Height + 5, radius + 2, Color(255, 214, 150, 70), 2.f);
}

void PetApp::paintPanelStatic(Graphics& g) {
    using namespace Ui;
    g.Clear(background);
    // 顶部柔和色块
    { LinearGradientBrush wash(PointF(0, 0), PointF(0, 160), Color(255, 238, 232, 220), background); g.FillRectangle(&wash, 0.f, 0.f, W, 160.f); }
    text(g, uiFonts, L"栗栗的生活馆", 32, 18, 400, 44, 28, ink, true);
    text(g, uiFonts, L"陪伴你的每一段桌面时光", 34, 62, 400, 24, 13.5f, muted);
    card(g, 608, 24, 184, 58, paper, 20);
    coin(g, 636, 53, 12);
    text(g, uiFonts, std::to_wstring(life.coins) + L" 金币", 654, 28, 132, 32, 20, Color(255, 160, 112, 44), true);
    text(g, uiFonts, L"游戏金币 · 无真实支付", 654, 58, 136, 18, 10.5f, muted);
    ULONGLONG now = GetTickCount64();
    switch (page) {
    case NAV_HOME: {
        card(g, 28, 164, 288, 440, warm);
        text(g, uiFonts, L"栗栗", 44, 178, 256, 38, 26, ink, true, StringAlignmentCenter);
        std::wstring status = std::wstring(life.status()) + (sleeping ? L" · 睡觉中" : now < exerciseUntil ? L" · 踏步中" : L"");
        Draw::fillRound(g, 108, 220, 128, 26, 13, Color(255, 250, 244, 232));
        text(g, uiFonts, status, 108, 220, 128, 26, 12.5f, life.sick ? danger : sage, true, StringAlignmentCenter);
        SolidBrush groundBrush(Color(255, 232, 219, 196)); g.FillEllipse(&groundBrush, 82.f, 498.f, 180.f, 22.f);
        paintPortraitLabel(g);
        card(g, 332, 164, 460, 290);
        text(g, uiFonts, L"今日状态", 356, 180, 300, 30, 18, ink, true);
        meter(g, uiFonts, L"饱食度", life.satiety, 356, 218, 412, Color(255, 212, 160, 84));
        meter(g, uiFonts, L"健康", life.health, 356, 274, 412, life.sick ? Color(255, 200, 104, 90) : sage);
        meter(g, uiFonts, L"心情", life.mood, 356, 330, 412, Color(255, 206, 132, 134));
        meter(g, uiFonts, L"精力", life.energy, 356, 386, 412, Color(255, 122, 150, 186));
        card(g, 332, 468, 460, 136);
        text(g, uiFonts, L"体重", 356, 482, 80, 28, 14, muted);
        text(g, uiFonts, std::to_wstring(static_cast<int>(life.grams)) + L" g · " + life.shape(), 420, 478, 340, 34, 22, ink, true);
        text(g, uiFonts, L"吃得太多会变胖，活动和照护帮助保持匀称。", 356, 514, 420, 22, 12, muted);
        Draw::fillRound(g, 356, 546, 412, 44, 12, background);
        std::wstring stats[] = {L"累计喂食 " + std::to_wstring(life.recycled) + L" 项", L"相伴 " + std::to_wstring(daysTogether(life.firstDay, todayStamp())) + L" 天", L"摸摸 " + std::to_wstring(life.petCount) + L" 次"};
        for (int i = 0; i < 3; ++i) text(g, uiFonts, stats[i], 356 + i * 137.f, 546, 137, 44, 13, sage, true, StringAlignmentCenter);
        break;
    }
    case NAV_FEED: {
        text(g, uiFonts, L"把不再需要的文件，变成栗栗的一餐", 32, 164, 700, 34, 20, ink, true);
        card(g, 28, 206, 764, 110, pale);
        { Pen dash(Color(255, 170, 196, 176), 1.5f); dash.SetDashStyle(DashStyleDash); GraphicsPath p; Draw::roundRect(p, 36, 214, 748, 94, 14); g.DrawPath(&dash, &p); }
        text(g, uiFonts, L"拖文件 / 文件夹到栗栗身上或这个窗口", 48, 222, 700, 30, 18, sage, true);
        text(g, uiFonts, L"先加入清单，再由你确认回收。也会感应新进入系统回收站的项目。", 350, 268, 424, 32, 12, muted, false, StringAlignmentNear, true);
        card(g, 28, 328, 764, 280);
        text(g, uiFonts, L"待确认 " + std::to_wstring(pendingFiles.size()) + L" 项", 44, 334, 300, 30, 15, ink, true);
        RectF list = fileListRect();
        Draw::fillRound(g, list.X - 4, list.Y - 4, list.Width + 8, list.Height + 8, 10, Color(255, 250, 248, 243));
        if (pendingFiles.empty()) text(g, uiFonts, L"清单为空 · 可以点「选择文件」，也可以直接把文件拖进来", list.X, list.Y, list.Width, list.Height, 13, muted, false, StringAlignmentCenter);
        else if (static_cast<int>(pendingFiles.size()) > fileRowsVisible()) {
            float track = list.Height, thumb = std::max(24.f, track * fileRowsVisible() / pendingFiles.size());
            float pos = (track - thumb) * fileScroll / std::max(1, static_cast<int>(pendingFiles.size()) - fileRowsVisible());
            Draw::fillRound(g, list.X + list.Width + 1, list.Y + pos, 4, thumb, 2, Color(255, 206, 196, 180));
        }
        text(g, uiFonts, L"文件夹连同内容一起回收，目录按 1 项计入。双击某行可在资源管理器中查看；Delete 键移除选中项。", 44, 560, 740, 40, 12, muted, false, StringAlignmentNear, true);
        break;
    }
    case NAV_SHOP: {
        const wchar_t* titles[] = {L"养护药", L"营养片", L"逗逗球", L"小围巾", L"软软睡窝", L"运动铃"};
        const wchar_t* descriptions[] = {L"健康 +35，精力 +8\n去照护页使用", L"健康 +12，精力 +20\n去照护页使用", L"解锁玩耍动作\n互动增加心情",
                                         L"一条暖红小围巾\n所有动作都戴得住", L"睡觉时恢复得更快\n睡着会出现小窝", L"提高运动的消耗\n踏步时叮当响"};
        for (int i = 0; i < 6; ++i) {
            float x = 28 + (i % 3) * 258.f, y = 164 + (i / 3) * 222.f;
            card(g, x, y, 246, 208);
            shopIcon(g, i, x + 16, y + 16);
            text(g, uiFonts, titles[i], x + 78, y + 16, 150, 28, 18, ink, true);
            text(g, uiFonts, std::to_wstring(PetLife::price(i)) + L" 金币", x + 78, y + 44, 150, 20, 12.5f, Color(255, 170, 120, 50), true);
            text(g, uiFonts, descriptions[i], x + 18, y + 78, 212, 46, 13, muted, false, StringAlignmentNear, true);
            std::wstring stock = i == 0 ? L"库存 " + std::to_wstring(life.medicine) : i == 1 ? L"库存 " + std::to_wstring(life.vitamins) : life.owned(i) ? L"✓ 永久道具 · 已解锁" : L"永久道具";
            text(g, uiFonts, stock, x + 18, y + 128, 212, 22, 12, sage, life.owned(i));
        }
        text(g, uiFonts, L"每日礼物 +20 金币。每个新回收项目 +3 金币，单次最多 +40。", 32, 614, 760, 30, 13, muted);
        break;
    }
    case NAV_CARE: {
        card(g, 28, 164, 374, 380); card(g, 418, 164, 374, 380);
        text(g, uiFonts, L"健康与照护", 44, 178, 330, 34, 20, ink, true);
        text(g, uiFonts, life.sick ? L"栗栗生病了，需要好好休息。" : L"规律休息，让栗栗保持精神。", 44, 214, 340, 24, 13.5f, life.sick ? danger : muted);
        meter(g, uiFonts, L"健康", life.health, 44, 256, 342, life.sick ? Color(255, 200, 104, 90) : sage);
        meter(g, uiFonts, L"精力", life.energy, 44, 314, 342, Color(255, 122, 150, 186));
        text(g, uiFonts, L"体重与运动", 434, 178, 330, 34, 20, ink, true);
        text(g, uiFonts, std::to_wstring(static_cast<int>(life.grams)) + L" g  /  " + life.shape(), 434, 222, 340, 42, 28, sage, true);
        text(g, uiFonts, L"380–580 g 为匀称区间。\n食物只来自你明确丢弃的文件。", 434, 280, 340, 50, 13.5f, muted, false, StringAlignmentNear, true);
        if (life.bell) text(g, uiFonts, L"✓ 已装备运动铃：运动消耗更多", 434, 342, 340, 24, 12.5f, sage, true);
        card(g, 28, 560, 764, 90, pale);
        std::wstring hint = life.sick ? L"健康低于 45 会生病，恢复到 70 后康复。用药、休息或陪伴都能帮助恢复。" : L"饥饿、过饱、低精力和体重异常会逐渐影响健康；关闭程序不会补扣数值。";
        text(g, uiFonts, hint, 46, 570, 730, 30, 13, sage);
        text(g, uiFonts, life.careCooldown > 0 ? L"免费照护还需等待 " + std::to_wstring(static_cast<int>(std::ceil(life.careCooldown))) + L" 秒。" : L"免费照护已经准备好，不需要金币。", 46, 604, 730, 30, 12.5f, muted);
        break;
    }
    case NAV_SETTINGS: {
        card(g, 28, 164, 764, 440);
        const wchar_t* labels[] = {L"快捷键", L"停靠位置", L"桌宠大小", L"不透明度", L"动画帧率", L"陪伴方式"};
        const float ys[] = {178, 252, 302, 352, 402, 454};
        for (int i = 0; i < 6; ++i) text(g, uiFonts, labels[i], 48, ys[i], 116, 36, 14.5f, ink, true);
        text(g, uiFonts, hotkeyStatus.empty() ? L"快捷键未启用" : hotkeyStatus, 170, 216, 610, 26, 12, muted);
        Pen divider(line, 1.f);
        for (float y : {246.f, 446.f}) g.DrawLine(&divider, 48.f, y, 772.f, y);
        text(g, uiFonts, L"拖到屏幕边缘自动停靠；开启自动散步会解除停靠。按住 Ctrl 在栗栗身上滚动滚轮可调整大小。", 32, 610, 364, 52, 12, muted, false, StringAlignmentNear, true);
        break;
    }
    default: {
        card(g, 28, 164, 374, 440); card(g, 418, 164, 374, 440);
        text(g, uiFonts, L"和栗栗相处", 48, 178, 330, 30, 18, ink, true);
        text(g, uiFonts,
             L"• 单击：摸摸它，心情 +2\n• 鼠标在身上来回轻抚：也算摸摸\n• 左键拖动：换位置，靠近左右边缘自动停靠\n"
             L"• 双击或右键：打开生活馆\n• 中键：快捷菜单\n• Ctrl + 滚轮：调整大小\n• 拖着文件停在栗栗身上：它会凑过来嗅嗅\n• 托盘单击找回，双击打开生活馆，右键菜单\n"
             L"• 生活馆右上角 X 只收起面板；完整退出请用设置页或托盘菜单",
             48, 214, 340, 250, 13, ink, false, StringAlignmentNear, true);
        text(g, uiFonts, L"快捷键", 48, 468, 330, 28, 16, ink, true);
        text(g, uiFonts, L"默认「快速按两下 Esc」隐藏 / 显示，Esc 仍会正常传给其他软件。也可以改为独占 Esc、Ctrl+Alt+H 或关闭。生活馆里：Tab 切换按钮，Ctrl+Tab 切换页面，Esc 收起面板。",
             48, 498, 340, 100, 12.5f, muted, false, StringAlignmentNear, true);
        text(g, uiFonts, L"文件喂食与养成", 438, 178, 330, 30, 18, ink, true);
        text(g, uiFonts,
             L"• 拖入或选择文件只会进入清单，确认后才移入 Windows 回收站，可随时还原\n• 也会感应你平时删进回收站的项目\n"
             L"• 饱食度随时间下降，喂食增加饱食度、金币和体重\n• 健康低于 45 会生病，恢复到 70 康复\n• 精力见底会自己打盹（可关闭）\n"
             L"• 金币只是游戏数值，没有任何真实支付",
             438, 214, 340, 250, 13, ink, false, StringAlignmentNear, true);
        text(g, uiFonts, L"设置与进度", 438, 468, 330, 28, 16, ink, true);
        text(g, uiFonts, L"保存在 %LOCALAPPDATA%\\LiliDesktopPet\\settings.ini，沿用旧版进度。需要重置时先退出，再删除该文件。程序离线运行，不联网。",
             438, 498, 340, 100, 12.5f, muted, false, StringAlignmentNear, true);
        break;
    }
    }
    for (const auto& b : buttons)
        paintButton(g, uiFonts, b, b.id == hoverId && b.enabled, b.id == pressedId && b.id == hoverId, keyboardFocus && b.id == focusId && GetFocus() == panel);
    std::wstring bottom = notice;
    if (page == NAV_FEED && hoverFile >= 0 && hoverFile < static_cast<int>(pendingFiles.size())) bottom = pendingFiles[hoverFile];
    Draw::fillRound(g, 28, 676, 764, 30, 10, Color(255, 240, 235, 226));
    text(g, uiFonts, bottom, 42, 676, 740, 30, 12, muted, false, StringAlignmentNear, false, page == NAV_FEED && hoverFile >= 0 ? StringTrimmingEllipsisPath : StringTrimmingEllipsisCharacter);
}

void PetApp::paintPortrait(Graphics& g, Canvas* target, const PixelBox* clip) {
    if (page != NAV_HOME || !sprites.ready()) return;
    paintBody(g, 172, 508, 1.02f, true, target, clip);
}

// 动作名称写在静态层里；动作变化时只重画这一小块，不触发整页重绘。
void PetApp::paintPortraitLabel(Graphics& g) {
    RectF r = portraitLabelRect();
    SolidBrush card(Ui::warm); g.FillRectangle(&card, r);
    Ui::text(g, uiFonts, Animation::name(animation.motion()), r.X, r.Y, r.Width, r.Height, 13, Ui::sage, false, StringAlignmentCenter);
    portraitLabelMotion = static_cast<int>(animation.motion());
}

void PetApp::panelPaint(HDC dc, const RECT& area) {
    RECT client{}; GetClientRect(panel, &client);
    if (client.right <= 0 || client.bottom <= 0) return;
    if (panelStatic.w != client.right || panelStatic.h != client.bottom) {
        if (!panelStatic.create(client.right, client.bottom) || !panelFrame.create(client.right, client.bottom)) return;
        panelDirty = true;
    }
    if (panelDirty) {
        layoutPanel();
        Graphics g(panelStatic.bitmap.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.ScaleTransform(uiScale, uiScale);
        paintPanelStatic(g); g.Flush(FlushIntentionSync);
        panelDirty = false;
    }
    int x = std::max(0L, area.left), y = std::max(0L, area.top);
    int w = std::min(client.right, area.right) - x, h = std::min(client.bottom, area.bottom) - y;
    if (w <= 0 || h <= 0) return;
    BitBlt(panelFrame.dc, x, y, w, h, panelStatic.dc, x, y, SRCCOPY);
    if (page == NAV_HOME) {
        Graphics g(panelFrame.bitmap.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.SetClip(Rect(x, y, w, h));
        g.ScaleTransform(uiScale, uiScale);
        PixelBox clip{x, y, x + w, y + h};
        paintPortrait(g, &panelFrame, &clip); g.Flush(FlushIntentionSync);
    }
    BitBlt(dc, x, y, w, h, panelFrame.dc, x, y, SRCCOPY);
}

void PetApp::panelTick(ULONGLONG now) {
    if (!panel || page != NAV_HOME || !IsWindowVisible(panel) || IsIconic(panel) || now - lastPanelFrame < 33) return;
    lastPanelFrame = now;
    if (!panelDirty && panelStatic.bitmap && portraitLabelMotion != static_cast<int>(animation.motion())) {
        Graphics g(panelStatic.bitmap.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.ScaleTransform(uiScale, uiScale);
        paintPortraitLabel(g); g.Flush(FlushIntentionSync);
        RectF l = portraitLabelRect();
        RECT labelArea{static_cast<LONG>(l.X * uiScale), static_cast<LONG>(l.Y * uiScale), static_cast<LONG>(std::ceil((l.X + l.Width) * uiScale)), static_cast<LONG>(std::ceil((l.Y + l.Height) * uiScale))};
        InvalidateRect(panel, &labelArea, FALSE);
    }
    RectF r = portraitRect();
    RECT area{static_cast<LONG>(r.X * uiScale), static_cast<LONG>(r.Y * uiScale), static_cast<LONG>(std::ceil((r.X + r.Width) * uiScale)), static_cast<LONG>(std::ceil((r.Y + r.Height) * uiScale))};
    InvalidateRect(panel, &area, FALSE);
}

void PetApp::focusNext(int step) {
    std::vector<int> order;
    for (const auto& b : buttons) if (b.enabled && b.kind != UiButton::Row && b.kind != UiButton::RowDelete) order.push_back(b.id);
    if (order.empty()) return;
    auto it = std::find(order.begin(), order.end(), focusId);
    int index = it == order.end() ? (step > 0 ? -1 : 0) : static_cast<int>(it - order.begin());
    index = (index + step + static_cast<int>(order.size())) % static_cast<int>(order.size());
    focusId = order[index]; keyboardFocus = true; refreshPanel();
}

void PetApp::activate(int id) {
    if (id >= FILE_DEL && id < FILE_DEL + 1000) {
        int index = id - FILE_DEL;
        if (index < static_cast<int>(pendingFiles.size())) {
            pendingFiles.erase(pendingFiles.begin() + index);
            if (index < static_cast<int>(fileSelected.size())) fileSelected.erase(fileSelected.begin() + index);
            fileScroll = std::clamp(fileScroll, 0, std::max(0, static_cast<int>(pendingFiles.size()) - fileRowsVisible()));
            hoverFile = -1;
            notice = L"已从清单移除 1 项（没有移动任何文件）。";
        }
        refreshPanel(); return;
    }
    if (id >= FILE_ROW && id < FILE_ROW + 1000) return;
    command(id);
}

LRESULT CALLBACK PanelProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    PetApp* app = reinterpret_cast<PetApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<PetApp*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app)); app->panel = window;
    }
    if (!app) return DefWindowProcW(window, message, wp, lp);
    auto logical = [&](LPARAM value) { return PointF(GET_X_LPARAM(value) / app->uiScale, GET_Y_LPARAM(value) / app->uiScale); };
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); app->panelPaint(dc, paint.rcPaint); EndPaint(window, &paint); return 0; }
    case WM_SIZE: app->panelDirty = true; InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT p{}; GetCursorPos(&p); ScreenToClient(window, &p);
            const UiButton* b = app->buttonAt(p.x / app->uiScale, p.y / app->uiScale);
            SetCursor(LoadCursorW(nullptr, b && b->enabled && b->kind != UiButton::Row ? IDC_HAND : IDC_ARROW)); return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        PointF p = logical(lp);
        const UiButton* b = app->buttonAt(p.X, p.Y);
        int id = b && b->enabled ? b->id : 0;
        int file = b && (b->kind == UiButton::Row || b->kind == UiButton::RowDelete) ? (b->id >= FILE_DEL ? b->id - FILE_DEL : b->id - FILE_ROW) : -1;
        if (id != app->hoverId || file != app->hoverFile) { app->hoverId = id; app->hoverFile = file; app->refreshPanel(); }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0}; TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE: if (app->hoverId || app->hoverFile >= 0) { app->hoverId = 0; app->hoverFile = -1; app->refreshPanel(); } return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
        SetFocus(window); app->keyboardFocus = false;
        PointF p = logical(lp);
        const UiButton* b = app->buttonAt(p.X, p.Y);
        if (!b || !b->enabled) return 0;
        if (b->kind == UiButton::Row) {
            int index = b->id - FILE_ROW;
            auto& sel = app->fileSelected; sel.resize(app->pendingFiles.size(), false);
            if (message == WM_LBUTTONDBLCLK && index < static_cast<int>(app->pendingFiles.size())) {
                std::wstring args = L"/select,\"" + app->pendingFiles[index] + L"\"";
                ShellExecuteW(window, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
            } else if (wp & MK_SHIFT && app->fileAnchor >= 0) {
                int a = std::min(app->fileAnchor, index), z = std::max(app->fileAnchor, index);
                if (!(wp & MK_CONTROL)) std::fill(sel.begin(), sel.end(), false);
                for (int i = a; i <= z && i < static_cast<int>(sel.size()); ++i) sel[i] = true;
            } else if (wp & MK_CONTROL) { sel[index] = !sel[index]; app->fileAnchor = index; }
            else { bool only = sel[index] && std::count(sel.begin(), sel.end(), true) == 1; std::fill(sel.begin(), sel.end(), false); sel[index] = !only; app->fileAnchor = index; }
            app->refreshPanel(); return 0;
        }
        app->pressedId = b->id; app->focusId = b->id; SetCapture(window); app->refreshPanel(); return 0;
    }
    case WM_LBUTTONUP: {
        int pressed = app->pressedId; app->pressedId = 0;
        if (GetCapture() == window) ReleaseCapture();
        PointF p = logical(lp);
        const UiButton* b = app->buttonAt(p.X, p.Y);
        app->refreshPanel();
        if (pressed && b && b->id == pressed && b->enabled) app->activate(pressed);
        return 0;
    }
    case WM_CAPTURECHANGED: if (app->pressedId) { app->pressedId = 0; app->refreshPanel(); } return 0;
    case WM_MOUSEWHEEL:
        if (app->page == NAV_FEED && !app->pendingFiles.empty()) {
            int maxScroll = std::max(0, static_cast<int>(app->pendingFiles.size()) - app->fileRowsVisible());
            app->fileScroll = std::clamp(app->fileScroll + (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -2 : 2), 0, maxScroll);
            app->hoverId = 0; app->hoverFile = -1; app->refreshPanel();
        }
        return 0;
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
    case WM_KEYDOWN: {
        bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
        auto moveSelection = [&](int delta) {
            auto& sel = app->fileSelected; sel.resize(app->pendingFiles.size(), false);
            int next = std::clamp(app->fileAnchor + delta, 0, static_cast<int>(sel.size()) - 1);
            std::fill(sel.begin(), sel.end(), false); sel[next] = true; app->fileAnchor = next;
            app->ensureFileVisible(next); app->refreshPanel();
        };
        bool listKeys = app->page == NAV_FEED && !app->pendingFiles.empty() && !app->keyboardFocus;
        if (wp == VK_TAB && ctrl) {
            int index = app->page >= NAV_HOME && app->page <= NAV_SETTINGS ? app->page - NAV_HOME : 0;
            app->showPanel(NAV_HOME + (index + (shift ? 4 : 1)) % 5); return 0;
        }
        if (wp == VK_TAB) { app->focusNext(shift ? -1 : 1); return 0; }
        if (wp == VK_DOWN && listKeys) { moveSelection(1); return 0; }
        if (wp == VK_UP && listKeys) { moveSelection(-1); return 0; }
        if (wp == VK_RIGHT || wp == VK_DOWN) { app->focusNext(1); return 0; }
        if (wp == VK_LEFT || wp == VK_UP) { app->focusNext(-1); return 0; }
        if ((wp == VK_RETURN || wp == VK_SPACE) && app->focusId && app->keyboardFocus) { app->activate(app->focusId); return 0; }
        if (wp == VK_ESCAPE) { app->hidePanel(); return 0; }
        if (app->page == NAV_FEED && wp == VK_DELETE) { app->command(REMOVE_FEED); return 0; }
        if (app->page == NAV_FEED && ctrl && wp == 'A') { app->command(SELECT_ALL_FEED); return 0; }
        return 0;
    }
    case WM_SETFOCUS: case WM_KILLFOCUS: app->refreshPanel(); return 0;
    case WM_DPICHANGED: {
        auto target = reinterpret_cast<RECT*>(lp);
        SetWindowPos(window, nullptr, target->left, target->top, target->right - target->left, target->bottom - target->top, SWP_NOZORDER | SWP_NOACTIVATE);
        RECT client{}; GetClientRect(window, &client); app->uiScale = client.right / Ui::W;
        app->refreshPanel(); return 0;
    }
    case WM_CLOSE: app->hidePanel(); return 0;
    case WM_DESTROY:
        app->revokePanelDrop();
        app->panel = nullptr; app->buttons.clear(); app->panelStatic.reset(); app->panelFrame.reset();
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
