#pragma once
// 轻量级软件合成：把预乘 BGRA 图像按仿射变换双线性采样后“源在上”叠加到目标上。
// 用来替代 GDI+ 带旋转/缩放的 DrawImage（桌宠每帧最贵的一步），只处理变换后的包围盒。平台无关，可单元测试。
#include "motion_flow.h"   // PixelBox
#include <algorithm>
#include <cmath>
#include <cstdint>

struct Affine {
    // X = a*u + c*v + tx；Y = b*u + d*v + ty（与 GDI+ Matrix 的 m11 m12 m21 m22 dx dy 顺序一致）
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
};

// clip 为目标像素范围（右、下不含）；srcBox 为源图有内容的范围，可用来缩小计算区域。
inline void blitAffine(std::uint8_t* dst, int dw, int dh, const std::uint8_t* src, int sw, int sh, const Affine& m,
                       float opacity, PixelBox clip, PixelBox srcBox) {
    if (!dst || !src || dw <= 0 || dh <= 0 || sw < 2 || sh < 2 || opacity <= 0) return;
    float det = m.a * m.d - m.b * m.c;
    if (std::abs(det) < 1e-8f) return;
    const float ia = m.d / det, ib = -m.b / det, ic = -m.c / det, id = m.a / det;
    if (srcBox.empty()) srcBox = PixelBox{0, 0, sw, sh};
    // 变换源内容框的四个角，得到目标包围盒
    float us[2] = {static_cast<float>(srcBox.left - 1), static_cast<float>(srcBox.right + 1)};
    float vs[2] = {static_cast<float>(srcBox.top - 1), static_cast<float>(srcBox.bottom + 1)};
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (float u : us) for (float v : vs) {
        float X = m.a * u + m.c * v + m.tx, Y = m.b * u + m.d * v + m.ty;
        minX = std::min(minX, X); maxX = std::max(maxX, X); minY = std::min(minY, Y); maxY = std::max(maxY, Y);
    }
    clip.left = std::max({clip.left, 0, static_cast<int>(std::floor(minX))});
    clip.top = std::max({clip.top, 0, static_cast<int>(std::floor(minY))});
    clip.right = std::min({clip.right, dw, static_cast<int>(std::ceil(maxX)) + 1});
    clip.bottom = std::min({clip.bottom, dh, static_cast<int>(std::ceil(maxY)) + 1});
    if (clip.empty()) return;
    const std::uint32_t op = static_cast<std::uint32_t>(std::clamp(opacity, 0.f, 1.f) * 256.f + .5f);
    auto tap = [&](int x, int y) -> const std::uint8_t* {
        static const std::uint8_t zero[4] = {0, 0, 0, 0};
        return (x < 0 || y < 0 || x >= sw || y >= sh) ? zero : src + (static_cast<size_t>(y) * sw + x) * 4;
    };
    for (int y = clip.top; y < clip.bottom; ++y) {
        float X0 = clip.left + .5f - m.tx, Y0 = y + .5f - m.ty;
        float u = ia * X0 + ic * Y0 - .5f, v = ib * X0 + id * Y0 - .5f;
        std::uint8_t* out = dst + (static_cast<size_t>(y) * dw + clip.left) * 4;
        for (int x = clip.left; x < clip.right; ++x, u += ia, v += ib, out += 4) {
            if (u <= -1.f || v <= -1.f || u >= sw || v >= sh) continue;
            int iu = static_cast<int>(u + 1.f) - 1, iv = static_cast<int>(v + 1.f) - 1;   // u、v > -1，等价于 floor
            std::uint32_t fu = static_cast<std::uint32_t>((u - iu) * 256.f), fv = static_cast<std::uint32_t>((v - iv) * 256.f);
            const std::uint8_t *p00, *p10, *p01, *p11;
            if (iu >= 0 && iv >= 0 && iu < sw - 1 && iv < sh - 1) {
                p00 = src + (static_cast<size_t>(iv) * sw + iu) * 4; p10 = p00 + 4; p01 = p00 + static_cast<size_t>(sw) * 4; p11 = p01 + 4;
            } else { p00 = tap(iu, iv); p10 = tap(iu + 1, iv); p01 = tap(iu, iv + 1); p11 = tap(iu + 1, iv + 1); }
            if (!(p00[3] | p10[3] | p01[3] | p11[3])) continue;
            std::uint32_t w00 = (256 - fu) * (256 - fv), w10 = fu * (256 - fv), w01 = (256 - fu) * fv, w11 = fu * fv;
            std::uint32_t s[4];
            for (int ch = 0; ch < 4; ++ch) {
                std::uint32_t value = (p00[ch] * w00 + p10[ch] * w10 + p01[ch] * w01 + p11[ch] * w11 + 32768) >> 16;
                s[ch] = (value * op) >> 8;
            }
            std::uint32_t inverse = 255 - std::min<std::uint32_t>(255, s[3]);
            for (int ch = 0; ch < 4; ++ch) {
                std::uint32_t value = s[ch] + (out[ch] * inverse + 127) / 255;
                out[ch] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, value));
            }
        }
    }
}

// 柔和椭圆阴影（中心最浓，边缘渐隐），直接写入预乘目标。
inline void softEllipse(std::uint8_t* dst, int dw, int dh, float cx, float cy, float rx, float ry,
                        std::uint8_t r, std::uint8_t g, std::uint8_t b, float alpha) {
    if (!dst || rx <= 0 || ry <= 0 || alpha <= 0) return;
    int left = std::max(0, static_cast<int>(cx - rx)), right = std::min(dw, static_cast<int>(cx + rx) + 1);
    int top = std::max(0, static_cast<int>(cy - ry)), bottom = std::min(dh, static_cast<int>(cy + ry) + 1);
    for (int y = top; y < bottom; ++y) {
        float dy = (y + .5f - cy) / ry;
        std::uint8_t* out = dst + (static_cast<size_t>(y) * dw + left) * 4;
        for (int x = left; x < right; ++x, out += 4) {
            float dx = (x + .5f - cx) / rx, dist = dx * dx + dy * dy;
            if (dist >= 1.f) continue;
            float k = 1.f - dist; k = k * k * alpha;
            std::uint32_t a = static_cast<std::uint32_t>(k * 255.f + .5f);
            if (!a) continue;
            std::uint32_t inverse = 255 - a;
            const std::uint32_t src[4] = {b * a / 255u, g * a / 255u, r * a / 255u, a};
            for (int ch = 0; ch < 4; ++ch) out[ch] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, src[ch] + (out[ch] * inverse + 127) / 255));
        }
    }
}
