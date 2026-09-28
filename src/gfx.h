#pragma once
// 绘图工具：32 位预乘 DIB 画布、字体缓存和常用形状。
#include "win_common.h"
#include <map>

// 带内存 DC 的 32 位自顶向下 DIB，可同时供 GDI（BitBlt/UpdateLayeredWindow）和 GDI+ 使用。
struct Canvas {
    int w = 0, h = 0;
    HDC dc = nullptr;
    HBITMAP handle = nullptr;
    HGDIOBJ previous = nullptr;
    BYTE* pixels = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> bitmap;
    Canvas() = default;
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    ~Canvas() { reset(); }
    void reset() {
        bitmap.reset();
        if (dc && previous) SelectObject(dc, previous);
        if (handle) DeleteObject(handle);
        if (dc) DeleteDC(dc);
        dc = nullptr; handle = nullptr; previous = nullptr; pixels = nullptr; w = h = 0;
    }
    bool create(int width, int height) {
        reset();
        if (width <= 0 || height <= 0) return false;
        w = width; h = height;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        dc = CreateCompatibleDC(nullptr);
        if (!dc) { reset(); return false; }
        handle = CreateDIBSection(dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!handle || !pixels) { reset(); return false; }
        previous = SelectObject(dc, handle);
        bitmap = std::make_unique<Gdiplus::Bitmap>(w, h, w * 4, PixelFormat32bppPARGB, pixels);
        if (bitmap->GetLastStatus() != Gdiplus::Ok) { reset(); return false; }
        return true;
    }
    bool ensure(int width, int height) { return (width == w && height == h && pixels) || create(width, height); }
    void clear() { if (pixels) std::memset(pixels, 0, static_cast<size_t>(w) * h * 4); }
};

// 纯内存的预乘像素缓冲，外加一个包装它的 GDI+ Bitmap（不占用 GDI 句柄）。
struct PixelBuffer {
    int side = 0;
    std::vector<BYTE> pixels;
    std::unique_ptr<Gdiplus::Bitmap> bitmap;
    bool create(int s) {
        bitmap.reset(); side = s; pixels.assign(static_cast<size_t>(s) * s * 4, 0);
        bitmap = std::make_unique<Gdiplus::Bitmap>(s, s, s * 4, PixelFormat32bppPARGB, pixels.data());
        return bitmap->GetLastStatus() == Gdiplus::Ok;
    }
};

// GDI+ 字体缓存：避免每帧/每段文字重新创建字体（2.1 的主要 CPU 开销之一）。
class FontCache {
    std::unique_ptr<Gdiplus::FontFamily> family_;
    std::map<int, std::unique_ptr<Gdiplus::Font>> fonts_;
public:
    const Gdiplus::FontFamily* family() {
        if (!family_) {
            for (const wchar_t* name : {L"Microsoft YaHei UI", L"Microsoft YaHei", L"微软雅黑", L"DengXian", L"SimHei", L"Noto Sans CJK SC", L"Segoe UI"}) {
                auto candidate = std::make_unique<Gdiplus::FontFamily>(name);
                if (candidate->GetLastStatus() == Gdiplus::Ok && candidate->IsAvailable()) { family_ = std::move(candidate); break; }
            }
            if (!family_) family_.reset(Gdiplus::FontFamily::GenericSansSerif()->Clone());
        }
        return family_.get();
    }
    Gdiplus::Font* get(float size, bool bold = false) {
        int key = static_cast<int>(std::lround(size * 10)) * 2 + (bold ? 1 : 0);
        auto found = fonts_.find(key);
        if (found != fonts_.end()) return found->second.get();
        auto font = std::make_unique<Gdiplus::Font>(family(), size, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        if (font->GetLastStatus() != Gdiplus::Ok)
            font = std::make_unique<Gdiplus::Font>(Gdiplus::FontFamily::GenericSansSerif(), size, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        auto* raw = font.get(); fonts_[key] = std::move(font); return raw;
    }
    void clear() { fonts_.clear(); family_.reset(); }
};

namespace Draw {
inline void roundRect(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float radius) {
    radius = std::max(.5f, std::min(radius, std::min(w, h) / 2));
    float d = radius * 2;
    path.AddArc(x, y, d, d, 180, 90); path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90); path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}
inline void fillRound(Gdiplus::Graphics& g, float x, float y, float w, float h, float r, const Gdiplus::Color& color) {
    Gdiplus::GraphicsPath path; roundRect(path, x, y, w, h, r); Gdiplus::SolidBrush brush(color); g.FillPath(&brush, &path);
}
inline void strokeRound(Gdiplus::Graphics& g, float x, float y, float w, float h, float r, const Gdiplus::Color& color, float width = 1.f) {
    Gdiplus::GraphicsPath path; roundRect(path, x, y, w, h, r); Gdiplus::Pen pen(color, width); g.DrawPath(&pen, &path);
}
// 柔和投影：几层半透明圆角矩形叠加，适合静态面板层（不在每帧调用）。
inline void softShadow(Gdiplus::Graphics& g, float x, float y, float w, float h, float r, BYTE strength = 18) {
    for (int i = 4; i >= 1; --i) {
        float grow = i * 1.6f;
        fillRound(g, x - grow + 1, y - grow + 3, w + grow * 2 - 2, h + grow * 2, r + grow, Gdiplus::Color(static_cast<BYTE>(strength / i), 94, 72, 45));
    }
}
inline void heart(Gdiplus::Graphics& g, float cx, float cy, float size, BYTE alpha) {
    Gdiplus::GraphicsPath path;
    path.AddBezier(cx, cy + size * .35f, cx - size, cy - size * .3f, cx - size * .35f, cy - size, cx, cy - size * .5f);
    path.AddBezier(cx, cy - size * .5f, cx + size * .35f, cy - size, cx + size, cy - size * .3f, cx, cy + size * .35f);
    Gdiplus::SolidBrush fill(Gdiplus::Color(alpha, 232, 112, 128)); g.FillPath(&fill, &path);
}
inline void sparkle(Gdiplus::Graphics& g, float cx, float cy, float size, BYTE alpha) {
    Gdiplus::PointF pts[8];
    for (int i = 0; i < 8; ++i) {
        float angle = i * 3.14159265f / 4, radius = i % 2 ? size * .32f : size;
        pts[i] = Gdiplus::PointF(cx + radius * std::sin(angle), cy - radius * std::cos(angle));
    }
    Gdiplus::SolidBrush fill(Gdiplus::Color(alpha, 255, 206, 92)); g.FillPolygon(&fill, pts, 8);
}
inline Gdiplus::Color withAlpha(const Gdiplus::Color& c, float alpha) {
    return Gdiplus::Color(static_cast<BYTE>(std::clamp(c.GetA() * alpha, 0.f, 255.f)), c.GetR(), c.GetG(), c.GetB());
}
} // namespace Draw
