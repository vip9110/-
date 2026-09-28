// 桌宠窗口：分层透明窗口的定位、绘制与鼠标交互。
#include "app.h"
#include "sprite_anchors.h"
#include "blit.h"

using namespace Gdiplus;
using Animation::Motion;

bool PetApp::applyScale() {
    scale = cfg.sizePercent / 100.f * dpi / 96.f;
    int w = static_cast<int>(std::lround(PET_W * scale)), h = static_cast<int>(std::lround(PET_H * scale));
    if (!canvas.ensure(w, h)) return false;
    // 帧按显示比例烘焙（上限 1.6 倍，等于素材原生精度），绘制时接近 1:1，既清晰又省 CPU。
    float bakeTarget = std::clamp(std::max(scale, dpi / 96.f), 1.f, 1.6f);
    if (!sprites.bake(bakeTarget)) return false;
    clampPosition();
    if (cfg.dockMode && !dragging) applyDock(true);
    placeCanvas();
    return true;
}

RECT PetApp::workAreaAt(double x, double y) const {
    POINT point{static_cast<LONG>(std::lround(x)), static_cast<LONG>(std::lround(y))};
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{}; info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) return info.rcWork;
    RECT fallback{0, 0, 1280, 720}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &fallback, 0); return fallback;
}

double PetApp::dockTargetX() const {
    RECT area = workArea(); double hw = halfWidth();
    return cfg.dockMode == 1 ? area.left + hw : area.right - hw;
}

void PetApp::clampPosition() {
    RECT area = workArea();
    double hw = halfWidth() * .9;
    double minX = area.left + hw, maxX = area.right - hw;
    if (minX > maxX) minX = maxX = (area.left + area.right) / 2.;
    double minY = area.top + GROUND * scale, maxY = area.bottom - 4 * scale;
    if (minY > maxY) minY = maxY;
    if (!std::isfinite(petX)) petX = maxX;
    if (!std::isfinite(petY)) petY = maxY;
    petX = std::clamp(petX, minX, maxX);
    petY = std::clamp(petY, minY, maxY);
}

// 画布始终留在显示器工作区内；身体在画布里的位置随之变化。
// 这样停靠、解除停靠、拖动时身体都不会跳动（2.1 切换停靠会让身体瞬移约 27 像素）。
void PetApp::placeCanvas() {
    RECT area = workArea();
    double left = petX - canvas.w / 2.0;
    if (area.right - area.left >= canvas.w) left = std::clamp(left, static_cast<double>(area.left), static_cast<double>(area.right - canvas.w));
    canvasLeft = static_cast<int>(std::lround(left));
    canvasTop = static_cast<int>(std::lround(petY - GROUND * scale));
    bodyX = static_cast<float>((petX - canvasLeft) / scale);
}

void PetApp::applyDock(bool instant) {
    if (!cfg.dockMode) return;
    direction = cfg.dockMode == 1 ? 1 : -1;
    cfg.roaming = false; moving = false;
    if (instant) petX = dockTargetX();
    clampPosition();
}

void PetApp::settleAfterDrag() {
    RECT area = workArea(); double hw = halfWidth();
    double leftGap = (petX - hw) - area.left, rightGap = area.right - (petX + hw);
    int dock = std::min(leftGap, rightGap) < 40 * scale ? (leftGap < rightGap ? 1 : 2) : 0;
    cfg.dockMode = dock;
    moving = false; happyUntil = 0;
    if (sleeping) { sleeping = false; autoSlept = false; }
    applyDock(false);
    // 拖动后只暂停散步十二秒，而不是像 2.1 那样永久关闭散步。
    roamPauseUntil = GetTickCount64() + 12000;
    startIdle(Motion::Land);
    say(dock ? L"靠边坐好，陪你工作～" : cfg.roaming ? L"换个地方，歇一下再散步～" : L"换个位置，继续陪你～");
    requestSave(); refreshPanel();
}

void PetApp::resetPosition() {
    RECT area{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    petX = area.right - halfWidth() - 60 * scale; petY = area.bottom - 4 * scale;
    cfg.dockMode = 2; cfg.roaming = false; applyDock(true);
    if (hidden) reveal();
    if (cfg.clickThrough) { cfg.clickThrough = false; applyWindowStyle(); }
    placeCanvas(); render(GetTickCount64());
}

bool PetApp::cursorOverPet(POINT cursor) const {
    if (!canvas.pixels) return false;
    int x = cursor.x - canvasLeft, y = cursor.y - canvasTop;
    if (x < 0 || y < 0 || x >= canvas.w || y >= canvas.h) return false;
    return canvas.pixels[(static_cast<size_t>(y) * canvas.w + x) * 4 + 3] > 20;
}

void PetApp::updateHover(ULONGLONG now) {
    POINT c{}; GetCursorPos(&c);
    hovered = !cfg.clickThrough && cursorOverPet(c);
    // 鼠标靠近时转头看向它（稳定 0.22 秒再转，避免来回抖动）。
    bool nearby = !cfg.clickThrough && std::abs(c.x - petX) < 230 * scale && c.y > canvasTop && c.y < petY + 40 * scale;
    int want = 0;
    if ((nearby || hovered) && std::abs(c.x - petX) > 28 * scale) want = c.x > petX ? 1 : -1;
    if (want != lookCandidate) { lookCandidate = want; lookCandidateSince = now; }
    if (want && now - lookCandidateSince > 220) { lookDirection = want; lookUntil = now + 2500; }
    if (lookDirection && now > lookUntil) lookDirection = 0;
    // 在栗栗身上来回轻抚鼠标（不用点击）也算摸摸。
    if (hovered && !dragging) {
        int dx = c.x - rubLastX, step = std::max(3, static_cast<int>(4 * scale));
        if (std::abs(dx) >= step) {
            int dir = dx > 0 ? 1 : -1;
            if (rubDirection && dir != rubDirection) {
                if (now - lastRubReversal > 1200) rubReversals = 0;
                ++rubReversals; lastRubReversal = now;
            }
            rubDirection = dir; rubLastX = c.x;
        }
        if (rubReversals >= 4 && now >= rubCooldown && !modal) { rubReversals = 0; rubCooldown = now + 6000; petReaction(true); }
    } else { rubReversals = 0; rubDirection = 0; rubLastX = c.x; }
}

// 身体用自带的仿射合成器绘制（target 为直接可写的 DIB）；没有 target 时退回 GDI+。
void PetApp::paintBody(Graphics& g, float center, float ground, float zoom, bool withScarf, Canvas* target, const PixelBox* clip) {
    const auto& body = sprites.body();
    if (!body.bitmap) return;
    const auto& p = bodyPose;
    float facing = animation.facing(); if (std::abs(facing) < .005f) facing = facing < 0 ? -.005f : .005f;
    float bodyScale = static_cast<float>(life.bodyScale());
    GraphicsState state = g.Save();
    g.TranslateTransform(center, ground); g.ScaleTransform(zoom, zoom);
    g.TranslateTransform(p.x * animation.facing(), p.y);
    g.ScaleTransform(facing * p.sx * bodyScale, p.sy);
    g.RotateTransform(p.angle);
    if (target && target->pixels) {
        Matrix world; g.GetTransform(&world);
        world.Translate(-Animation::PivotX, -Animation::PivotY);
        float k = static_cast<float>(Animation::SpriteSize) / body.side; world.Scale(k, k);
        REAL e[6]; world.GetElements(e);
        g.Flush(FlushIntentionSync);
        PixelBox area = clip ? *clip : PixelBox{0, 0, target->w, target->h};
        PixelBox content{body.side, body.side, 0, 0};
        for (int i = 0; i < Animation::FrameCount; ++i) if (p.weights[i] > .000001f) {
            const auto& b = sprites.box(i);
            content.left = std::min(content.left, b.left); content.top = std::min(content.top, b.top);
            content.right = std::max(content.right, b.right); content.bottom = std::max(content.bottom, b.bottom);
        }
        int grow = sprites.lastActiveFrames() == 2 ? static_cast<int>(12 * sprites.bakeScale()) : 0;   // 光流插值可能让轮廓略微外扩
        content = PixelBox{std::max(0, content.left - grow), std::max(0, content.top - grow), std::min(body.side, content.right + grow), std::min(body.side, content.bottom + grow)};
        blitAffine(target->pixels, target->w, target->h, body.pixels.data(), body.side, body.side, Affine{e[0], e[1], e[2], e[3], e[4], e[5]}, 1.f, area, content);
    } else {
        g.SetInterpolationMode(InterpolationModeBilinear);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        g.DrawImage(body.bitmap.get(), RectF(-Animation::PivotX, -Animation::PivotY, static_cast<REAL>(Animation::SpriteSize), static_cast<REAL>(Animation::SpriteSize)));
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    }
    if (withScarf && life.wearingScarf) drawScarf(g, p);
    g.Restore(state);
}

// 围巾跟随每一帧的头部位置（由光流推算的头部相似变换，见 sprite_anchors.h），所有动作都能看到。
void PetApp::drawScarf(Graphics& g, const Animation::Pose& pose) {
    float a = 0, b = 0, tx = 0, ty = 0, visible = 0;
    for (int i = 0; i < Animation::FrameCount; ++i) {
        float w = pose.weights[i];
        if (w <= 0) continue;
        const auto& h = HEAD_ANCHORS[i];
        visible += w * h.visible;
        float wv = w * h.visible;
        a += wv * h.a; b += wv * h.b; tx += wv * h.tx; ty += wv * h.ty;
    }
    if (visible < .02f) return;
    a /= visible; b /= visible; tx /= visible; ty /= visible;
    float alpha = std::clamp(visible, 0.f, 1.f);
    GraphicsState state = g.Save();
    g.TranslateTransform(-Animation::PivotX, -Animation::PivotY);
    Matrix head(a, b, -b, a, tx, ty); g.MultiplyTransform(&head);
    auto col = [&](BYTE r, BYTE gg, BYTE bb, float k = 1.f) { return Color(static_cast<BYTE>(255 * alpha * k), r, gg, bb); };
    // 尾巴先画（在围巾带下方）
    float sway = 6.f * std::sin(animation.clock() * 2.6f) - std::clamp(walkSpeed / std::max(.01f, scale), -40.f, 40.f) * .25f * animation.facing();
    {
        GraphicsState tail = g.Save();
        g.TranslateTransform(174, 174); g.RotateTransform(sway); g.TranslateTransform(-174, -174);
        LinearGradientBrush tailBrush(PointF(170, 172), PointF(170, 204), col(206, 70, 64), col(168, 44, 46));
        PointF t1[] = {PointF(167, 174), PointF(176, 174), PointF(174, 202), PointF(163, 200)};
        PointF t2[] = {PointF(176, 174), PointF(184, 174), PointF(193, 196), PointF(184, 199)};
        g.FillPolygon(&tailBrush, t2, 4); g.FillPolygon(&tailBrush, t1, 4);
        Pen fringe(col(150, 38, 40, .9f), 1.3f);
        for (int i = 0; i < 4; ++i) {
            float u = i / 3.f;
            g.DrawLine(&fringe, 163 + 11 * u, 200 + 2 * (1 - u), 162.5f + 11 * u, 205 + 2 * (1 - u));
            g.DrawLine(&fringe, 184 + 9 * u, 199 - 3 * u, 184.5f + 9 * u, 204 - 3 * u);
        }
        g.Restore(tail);
    }
    GraphicsPath band;
    band.AddBezier(105.f, 155.f, 124.f, 165.f, 178.f, 167.f, 199.f, 156.f);
    band.AddLine(199.f, 156.f, 197.f, 169.f);
    band.AddBezier(197.f, 169.f, 178.f, 182.f, 124.f, 180.f, 107.f, 168.f);
    band.CloseFigure();
    LinearGradientBrush fill(PointF(0, 154), PointF(0, 182), col(224, 86, 76), col(166, 44, 46));
    g.FillPath(&fill, &band);
    Pen rib(col(250, 150, 136, .45f), 1.1f);
    for (float x : {122.f, 136.f, 150.f, 164.f, 186.f}) {
        float t = (x - 105) / 94.f, top = 155 + 11 * std::sin(t * 3.14159f) * .95f, bottom = 168 + 12 * std::sin(t * 3.14159f);
        g.DrawLine(&rib, x, top + 2.f, x - 1.f, bottom - 1.5f);
    }
    Pen highlight(col(255, 176, 160, .7f), 1.6f); highlight.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g.DrawBezier(&highlight, 110.f, 158.f, 128.f, 166.f, 176.f, 168.f, 194.f, 159.f);
    SolidBrush knot(col(188, 54, 52));
    g.FillEllipse(&knot, 167.f, 167.f, 14.f, 12.f);
    SolidBrush shine(col(255, 170, 150, .6f)); g.FillEllipse(&shine, 170.f, 169.f, 5.f, 3.f);
    g.Restore(state);
}

// 气泡正文（圆角框 + 文字）只在文字或缩放改变时重绘一次，之后每帧直接合成，避免每帧排版文字。
void PetApp::drawBubble(Graphics& g, ULONGLONG now) {
    if (bubble.empty() || now >= bubbleUntil) return;
    float fade = std::min(1.f, (now - bubbleStart) / 160.f) * std::min(1.f, (bubbleUntil - now) / 260.f);
    if (fade <= .01f) return;
    if (bubbleCacheText != bubble || bubbleCacheScale != scale || !bubbleCanvas.pixels) {
        Font* font = petFonts.get(13.f);
        StringFormat format; format.SetAlignment(StringAlignmentCenter); format.SetLineAlignment(StringAlignmentCenter);
        RectF measured;
        g.MeasureString(bubble.c_str(), -1, font, RectF(0, 0, 238, 80), &format, &measured);
        bubbleW = std::clamp(measured.Width + 26.f, 70.f, 266.f); bubbleH = std::max(36.f, measured.Height + 16.f);
        int w = static_cast<int>(std::ceil((bubbleW + 4) * scale)), h = static_cast<int>(std::ceil((bubbleH + 6) * scale));
        if (bubbleCanvas.create(w, h)) {
            bubbleCanvas.clear();
            Graphics b(bubbleCanvas.bitmap.get());
            b.SetSmoothingMode(SmoothingModeAntiAlias); b.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            b.SetTextRenderingHint(TextRenderingHintAntiAlias); b.ScaleTransform(scale, scale);
            Draw::fillRound(b, 2, 4, bubbleW, bubbleH, 16, Color(34, 90, 60, 30));
            GraphicsPath shape; Draw::roundRect(shape, 1, 1, bubbleW, bubbleH, 16);
            SolidBrush fill(Color(250, 255, 252, 245)); Pen line(Color(235, 224, 204, 176), 1.f);
            b.FillPath(&fill, &shape); b.DrawPath(&line, &shape);
            SolidBrush ink(Color(255, 92, 64, 44));
            b.DrawString(bubble.c_str(), -1, font, RectF(11, 3, bubbleW - 20, bubbleH - 4), &format, &ink);
            b.Flush(FlushIntentionSync);
        }
        bubbleCacheText = bubble; bubbleCacheScale = scale;
    }
    float bottom = 72.f + (1 - fade) * 6.f;   // 出现/消失时轻轻上浮
    float x = std::clamp(bodyX - bubbleW / 2, 5.f, PET_W - 5.f - bubbleW), y = std::max(3.f, bottom - bubbleH);
    float tailX = std::clamp(bodyX + 6.f, x + 18.f, x + bubbleW - 18.f);
    PointF tri[] = {PointF(tailX - 8, y + bubbleH - 1), PointF(tailX + 1, y + bubbleH + 9), PointF(tailX + 8, y + bubbleH - 1)};
    SolidBrush tailFill(Color(static_cast<BYTE>(250 * fade), 255, 252, 245)); g.FillPolygon(&tailFill, tri, 3);
    Pen tailLine(Color(static_cast<BYTE>(235 * fade), 224, 204, 176), 1.f);
    g.DrawLine(&tailLine, tri[0], tri[1]); g.DrawLine(&tailLine, tri[1], tri[2]);
    g.Flush(FlushIntentionSync);
    Affine at; at.tx = std::round((x - 1) * scale); at.ty = std::round((y - 1) * scale);
    blitAffine(canvas.pixels, canvas.w, canvas.h, bubbleCanvas.pixels, bubbleCanvas.w, bubbleCanvas.h, at, fade, PixelBox{0, 0, canvas.w, canvas.h}, PixelBox{});
}

void PetApp::draw(ULONGLONG now, int forcedFrame) {
    if (!canvas.bitmap || !sprites.ready()) return;
    placeCanvas();
    canvas.clear();
    Graphics g(canvas.bitmap.get());
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingMode(CompositingModeSourceOver);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    g.ScaleTransform(scale, scale);
    Animation::Pose pose = forcedFrame >= 0 ? Animation::Pose(forcedFrame) : animation.pose();
    sprites.render(pose); bodyPose = pose;
    const float center = bodyX, ground = GROUND, time = animation.clock();
    const float bodyScale = static_cast<float>(life.bodyScale());
    const float sleepMix = pose.weights[4];
    // 柔和的地面阴影，跳起时变小变淡。
    {
        float lift = std::clamp(-pose.y, 0.f, 30.f) / 30.f;
        float sw = 118.f * bodyScale * (1 - .35f * lift), sh = 15.f * (1 - .3f * lift);
        softEllipse(canvas.pixels, canvas.w, canvas.h, center * scale, (ground + 1) * scale, sw / 2 * scale, sh / 2 * scale, 60, 40, 25, .3f * (1 - .5f * lift));
    }
    // 软软睡窝：睡觉时出现在身体下方。
    if (life.bed && sleepMix > .01f) {
        BYTE a = static_cast<BYTE>(245 * std::min(1.f, sleepMix));
        LinearGradientBrush cushion(PointF(0, ground - 24), PointF(0, ground + 10), Color(a, 196, 214, 180), Color(a, 150, 176, 138));
        g.FillEllipse(&cushion, center - 98.f, ground - 20.f, 196.f, 30.f);
        Pen rim(Color(static_cast<BYTE>(a * .8f), 128, 156, 118), 2.f); g.DrawEllipse(&rim, center - 98.f, ground - 20.f, 196.f, 30.f);
        SolidBrush inner(Color(static_cast<BYTE>(a * .55f), 232, 240, 222)); g.FillEllipse(&inner, center - 78.f, ground - 16.f, 156.f, 16.f);
    }
    paintBody(g, center, ground, 1.f, true, &canvas);
    Motion motion = animation.motion();
    bool joy = happyUntil && !dragging;
    if (joy && (motion == Motion::Hop || motion == Motion::Feed || motion == Motion::Enjoy || motion == Motion::Cheer)) {
        float fade = std::min(1.f, static_cast<float>(happyUntil > now ? happyUntil - now : 0) / 350.f);
        for (int i = 0; i < 3; ++i) {
            float phase = std::fmod(animation.time() * .65f + i / 3.f, 1.f);
            Draw::heart(g, center + (i - 1) * 36.f + 7.f * std::sin(time * 2 + i), 118.f - phase * 48.f, 7.f, static_cast<BYTE>((1 - phase) * 210 * fade));
        }
        if (motion == Motion::Cheer) {
            for (int i = 0; i < 4; ++i) {
                float phase = std::fmod(animation.time() * .9f + i / 4.f, 1.f);
                float angle = i * 1.7f + 0.4f;
                Draw::sparkle(g, center + std::cos(angle) * (60 + 30 * phase), 150.f - std::sin(angle) * 40 - phase * 30, 6.f * (1 - phase * .5f), static_cast<BYTE>((1 - phase) * 230 * fade));
            }
        }
        if (motion == Motion::Feed) {
            float progress = std::clamp((animation.time() - .3f) / 1.8f, 0.f, 1.f);
            float size = 1.f - Animation::smooth(progress);
            if (size > .02f) {
                float fx = center + animation.facing() * (65.f - 32.f * progress), fy = ground - 95.f - 26.f * progress;
                SolidBrush paper(Color(static_cast<BYTE>(255 * size), 249, 245, 226)); Pen ink(Color(static_cast<BYTE>(230 * size), 154, 174, 132), 1.3f);
                g.FillRectangle(&paper, fx - 8 * size, fy - 10 * size, 16 * size, 20 * size);
                g.DrawLine(&ink, fx - 5 * size, fy - 3 * size, fx + 5 * size, fy - 3 * size);
                g.DrawLine(&ink, fx - 5 * size, fy + 2 * size, fx + 3 * size, fy + 2 * size);
            }
        }
    }
    if (sleepMix > .05f) {
        SolidBrush ink(Color(static_cast<BYTE>(190 * sleepMix), 141, 120, 100));
        float zx = center + animation.facing() * 52.f;
        zx = std::clamp(zx, 12.f, PET_W - 40.f);
        for (int i = 0; i < 2; ++i) {
            float phase = std::fmod(time * .35f + i * .5f, 1.f);
            Font* font = petFonts.get(12.f + 6.f * phase, true);
            SolidBrush fadeInk(Color(static_cast<BYTE>(190 * sleepMix * (1 - phase)), 141, 120, 100));
            g.DrawString(L"z", 1, font, PointF(zx + phase * 16.f, 132.f - phase * 34.f), &fadeInk);
        }
    }
    if (life.sick && !sleeping && pose.weights[11] > .3f) {
        // 小汗珠
        float dx = center + animation.facing() * 44.f, dy = 132.f + 3.f * std::sin(time * 1.6f);
        GraphicsPath drop; drop.AddBezier(dx, dy - 7, dx + 5, dy, dx + 4, dy + 5, dx, dy + 5); drop.AddBezier(dx, dy + 5, dx - 4, dy + 5, dx - 5, dy, dx, dy - 7);
        SolidBrush blue(Color(static_cast<BYTE>(200 * pose.weights[11]), 150, 200, 235)); g.FillPath(&blue, &drop);
    }
    if (now < exerciseUntil && motion == Motion::Walk && life.bell) {
        float swing = std::sin(time * 9.f) * 12.f;
        GraphicsState st = g.Save(); g.TranslateTransform(center + animation.facing() * 70.f, 170.f); g.RotateTransform(swing);
        SolidBrush gold(Color(235, 232, 190, 80)); g.FillEllipse(&gold, -6.f, 0.f, 12.f, 11.f);
        Pen string(Color(200, 150, 110, 70), 1.2f); g.DrawLine(&string, 0.f, -8.f, 0.f, 1.f);
        g.Restore(st);
    }
    drawBubble(g, now);
    g.Flush(FlushIntentionSync);
}

bool PetApp::present() {
    if (hidden || autoHidden || !hwnd || !canvas.dc) return true;
    POINT destination{canvasLeft, canvasTop}, source{0, 0};
    SIZE size{canvas.w, canvas.h};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(std::clamp(cfg.opacity, 30, 100) * 255 / 100), AC_SRC_ALPHA};
    bool ok = UpdateLayeredWindow(hwnd, nullptr, &destination, &size, canvas.dc, &source, 0, &blend, ULW_ALPHA) != FALSE;
    presentedOnce = presentedOnce || ok;
    return ok;
}

void PetApp::reveal() {
    ULONGLONG now = GetTickCount64();
    bool wasHidden = hidden;
    hidden = false; clampPosition();
    tickTime = now; nextDecision = now + 3000;
    updateTimer();
    if (!autoHidden) ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd, cfg.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    say(wasHidden ? L"我回来陪你啦～" : L"我在这儿呢～"); render(now);
    if (wasHidden && panelWasVisible && panel) ShowWindow(panel, SW_SHOWNOACTIVATE);
}

void PetApp::toggleVisible() {
    if (modal) return;
    if (hidden) { reveal(); return; }
    if (!trayReady && !hotkeyReady) { say(L"暂时无法隐藏，请使用右键面板。", 4500); return; }
    panelWasVisible = panel && IsWindowVisible(panel);
    if (panel) ShowWindow(panel, SW_HIDE);
    hidden = true; dragging = false; ShowWindow(hwnd, SW_HIDE); updateTimer();
}

void PetApp::applyWindowStyle() {
    if (!hwnd) return;
    LONG_PTR style = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | (cfg.clickThrough ? WS_EX_TRANSPARENT : 0);
    LONG_PTR current = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    style |= current & WS_EX_TOPMOST;
    if (style != current) SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
    SetWindowPos(hwnd, cfg.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void PetApp::updateTimer() {
    if (!hwnd) return;
    UINT interval = hidden || autoHidden ? 1000 : cfg.fps == 30 ? 33 : 15;
    SetTimer(hwnd, TIMER_FRAME, interval, nullptr);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    PetApp* app = reinterpret_cast<PetApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<PetApp*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); app->hwnd = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wp, lp);
    if (app->taskbarMessage && message == app->taskbarMessage) {
        app->addTray(); if (app->hidden && !app->trayReady && !app->hotkeyReady) app->reveal(); return 0;
    }
    if (app->showMessage && message == app->showMessage) { app->reveal(); return 0; }
    switch (message) {
    case WM_TIMER: if (wp == TIMER_FRAME) app->tick(); return 0;
    case WM_HOTKEY: if (wp == HOTKEY_ID) app->toggleVisible(); return 0;
    case WM_APP_DOUBLE_ESC: app->toggleVisible(); return 0;
    case WM_APP_BIN_COUNT: app->onBinCount(wp != 0, static_cast<LONGLONG>(lp)); return 0;
    case WM_APP_DROPPED: {
        std::vector<std::wstring> paths; paths.swap(app->droppedPaths);
        if (!app->modal && !paths.empty()) app->addFiles(paths);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wp); std::vector<std::wstring> paths;
        UINT count = std::min(100u, DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0));
        for (UINT i = 0; i < count; ++i) {
            UINT length = DragQueryFileW(drop, i, nullptr, 0); std::wstring path(length + 1, L'\0');
            DragQueryFileW(drop, i, path.data(), length + 1); path.resize(length); paths.push_back(path);
        }
        DragFinish(drop); if (!app->modal) app->addFiles(paths); return 0;
    }
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT paint{}; BeginPaint(window, &paint); EndPaint(window, &paint); return 0; }
    case WM_SETCURSOR: SetCursor(LoadCursorW(nullptr, app->dragging && app->dragMoved ? IDC_SIZEALL : IDC_HAND)); return TRUE;
    case WM_LBUTTONDOWN: {
        POINT cursor{}; GetCursorPos(&cursor); app->pressPoint = cursor;
        app->dragOffsetX = cursor.x - app->petX; app->dragOffsetY = cursor.y - app->petY;
        app->dragging = true; app->dragMoved = false; SetCapture(window); return 0;
    }
    case WM_MOUSEMOVE:
        if (app->dragging && GetCapture() == window) {
            POINT cursor{}; GetCursorPos(&cursor);
            if (!app->dragMoved && (std::abs(cursor.x - app->pressPoint.x) > GetSystemMetrics(SM_CXDRAG) ||
                std::abs(cursor.y - app->pressPoint.y) > GetSystemMetrics(SM_CYDRAG))) {
                app->dragMoved = true; app->moving = false; app->idleAction = -1; app->walkSpeed = 0;
            }
            if (app->dragMoved) {
                app->petX = cursor.x - app->dragOffsetX; app->petY = cursor.y - app->dragOffsetY;
                app->clampPosition(); app->render(GetTickCount64());
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (app->dragging) {
            bool moved = app->dragMoved;
            app->dragging = false; app->dragMoved = false; ReleaseCapture();
            if (moved) app->settleAfterDrag(); else app->command(PET);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (app->dragging) { bool moved = app->dragMoved; app->dragging = false; app->dragMoved = false; if (moved) app->settleAfterDrag(); }
        return 0;
    case WM_LBUTTONDBLCLK: app->showPanel(NAV_HOME); return 0;
    case WM_CONTEXTMENU: app->showPanel(NAV_HOME); return 0;
    case WM_MBUTTONUP: { POINT p{}; GetCursorPos(&p); app->menu(p); return 0; }
    case WM_MOUSEWHEEL:
        if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) {
            const int sizes[] = {60, 75, 100, 135, 160}; const int ids[] = {XSMALL, SMALL, MEDIUM, LARGE, XLARGE};
            int index = 2; for (int i = 0; i < 5; ++i) if (sizes[i] == app->cfg.sizePercent) index = i;
            index = std::clamp(index + (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1), 0, 4);
            if (sizes[index] != app->cfg.sizePercent) app->command(ids[index]);
        }
        return 0;
    case WM_APP_TRAY:
        if (lp == WM_LBUTTONUP) { if (app->hidden) app->reveal(); else app->say(L"我在这儿呢～"); }
        else if (lp == WM_LBUTTONDBLCLK) app->showPanel(NAV_HOME);
        else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) { POINT point{}; GetCursorPos(&point); app->menu(point); }
        return 0;
    case WM_COMMAND: app->command(LOWORD(wp)); return 0;
    case WM_DISPLAYCHANGE: app->clampPosition(); app->applyDock(true); app->render(GetTickCount64()); return 0;
    case WM_SETTINGCHANGE:
        if (wp == SPI_SETWORKAREA) { app->clampPosition(); app->applyDock(true); app->render(GetTickCount64()); }
        return 0;
    case WM_DPICHANGED: app->pendingDpi = HIWORD(wp); return 0;
    case WM_POWERBROADCAST:
        if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) { app->statClock = app->tickTime = GetTickCount64(); app->requestBinBaseline(); }
        return TRUE;
    case WM_QUERYENDSESSION: app->saveNow(); return TRUE;
    case WM_ENDSESSION: if (wp) app->saveNow(); return 0;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        if (app->panel) DestroyWindow(app->panel);
        if (app->menuHost) { DestroyWindow(app->menuHost); app->menuHost = nullptr; }
        app->shutdown(); PostQuitMessage(0); return 0;
    default: return DefWindowProcW(window, message, wp, lp);
    }
}
