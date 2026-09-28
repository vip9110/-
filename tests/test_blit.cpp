#include "../src/blit.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
int main() {
    const int S = 64;
    std::vector<std::uint8_t> src(S * S * 4, 0);
    for (int y = 8; y < 56; ++y) for (int x = 10; x < 50; ++x) { auto* p = &src[(y * S + x) * 4]; p[3] = static_cast<std::uint8_t>(120 + (x * 3 + y) % 136); p[0] = p[3] / 2; p[1] = p[3] / 3; p[2] = p[3]; }
    // 整数平移 = 精确复制
    std::vector<std::uint8_t> dst(100 * 100 * 4, 0);
    Affine shift; shift.tx = 20; shift.ty = 15;
    blitAffine(dst.data(), 100, 100, src.data(), S, S, shift, 1.f, PixelBox{0, 0, 100, 100}, PixelBox{});
    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) for (int c = 0; c < 4; ++c) assert(dst[((y + 15) * 100 + x + 20) * 4 + c] == src[(y * S + x) * 4 + c]);
    std::cout << "PASS integer translation copies pixels exactly\n";
    // 旋转缩放后仍满足预乘约束
    std::fill(dst.begin(), dst.end(), 0);
    float angle = .3f, k = 1.3f; Affine rot{k * std::cos(angle), k * std::sin(angle), -k * std::sin(angle), k * std::cos(angle), 50, 5};
    blitAffine(dst.data(), 100, 100, src.data(), S, S, rot, 1.f, PixelBox{0, 0, 100, 100}, PixelBox{10, 8, 50, 56});
    size_t covered = 0;
    for (size_t p = 0; p < dst.size(); p += 4) { for (int c = 0; c < 3; ++c) assert(dst[p + c] <= dst[p + 3]); if (dst[p + 3] > 100) ++covered; }
    assert(covered > 2800);
    std::cout << "PASS rotated/scaled blit keeps premultiplied alpha\n";
    // 半透明
    std::vector<std::uint8_t> half(100 * 100 * 4, 0);
    blitAffine(half.data(), 100, 100, src.data(), S, S, shift, .5f, PixelBox{0, 0, 100, 100}, PixelBox{});
    int a = src[(30 * S + 30) * 4 + 3], h = half[((30 + 15) * 100 + 50) * 4 + 3];
    assert(std::abs(h - a / 2) <= 1);
    std::cout << "PASS opacity scales the source\n";
    // 裁剪区域外不改动
    std::vector<std::uint8_t> clipped(100 * 100 * 4, 7);
    blitAffine(clipped.data(), 100, 100, src.data(), S, S, shift, 1.f, PixelBox{40, 40, 60, 60}, PixelBox{});
    for (int y = 0; y < 100; ++y) for (int x = 0; x < 100; ++x) if (x < 40 || x >= 60 || y < 40 || y >= 60) assert(clipped[(y * 100 + x) * 4] == 7);
    std::cout << "PASS clip rectangle is respected\n";
    // 源在上叠加：不透明源覆盖目标
    std::vector<std::uint8_t> under(100 * 100 * 4, 0);
    for (size_t p = 0; p < under.size(); p += 4) { under[p] = 200; under[p + 3] = 255; }
    std::vector<std::uint8_t> solid(S * S * 4, 0); for (size_t p = 0; p < solid.size(); p += 4) { solid[p + 1] = 255; solid[p + 3] = 255; }
    blitAffine(under.data(), 100, 100, solid.data(), S, S, shift, 1.f, PixelBox{0, 0, 100, 100}, PixelBox{});
    auto* mid = &under[((15 + 30) * 100 + 50) * 4]; assert(mid[0] == 0 && mid[1] == 255 && mid[3] == 255);
    std::cout << "PASS source-over composition\n";
    // 阴影
    std::vector<std::uint8_t> shadow(200 * 40 * 4, 0);
    softEllipse(shadow.data(), 200, 40, 100, 20, 60, 8, 60, 40, 25, .3f);
    auto* c = &shadow[(20 * 200 + 100) * 4]; assert(c[3] >= 70 && c[3] <= 78 && c[0] <= c[3] && c[2] <= c[3]);
    assert(shadow[(20 * 200 + 20) * 4 + 3] == 0);
    std::cout << "PASS soft ellipse shadow\n";
    // 性能：256×256 旋转叠加
    const int B = 256; std::vector<std::uint8_t> body(B * B * 4, 0), canvas(300 * 300 * 4, 0);
    for (int y = 50; y < 245; ++y) for (int x = 48; x < 208; ++x) { auto* p = &body[(y * B + x) * 4]; p[3] = 250; p[0] = 120; p[1] = 150; p[2] = 200; }
    Affine pose{std::cos(.05f), std::sin(.05f), -std::sin(.05f), std::cos(.05f), 22, 30};
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 300; ++i) blitAffine(canvas.data(), 300, 300, body.data(), B, B, pose, 1.f, PixelBox{0, 0, 300, 300}, PixelBox{48, 50, 208, 245});
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 300;
    std::cout << "INFO body blit " << ms << " ms/frame at 100% (offline, not a Windows benchmark)\n";
}
