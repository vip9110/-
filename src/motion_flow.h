#pragma once
// 运动补偿插值：使用预先计算、嵌入 exe 的位移向量，把两张关键帧按部位位移后再做预乘透明度混合，
// 减少直接叠帧造成的五官重影。运行时只用标准 C++17；OpenCV 仅用于可选的开发期向量生成脚本。
// 3.0：支持任意边长（向量按比例缩放）、只处理有内容的包围盒、逐行增量插值位移网格（无需缓存稠密位移场）、定点采样。
#include "animation.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

struct PixelBox {
    int left = 0, top = 0, right = 0, bottom = 0; // right/bottom 不含
    bool empty() const { return right <= left || bottom <= top; }
};

class MotionFlow {
public:
    static constexpr int Grid = 33, BaseSide = Animation::SpriteSize;
private:
    struct Pair { std::uint16_t a = 0, b = 0; float reach = 0; std::array<std::int16_t,Grid*Grid*4> values{}; };
    std::vector<Pair> pairs_;
    std::array<int,Animation::FrameCount*Animation::FrameCount> lookup_{};

    // 定点双线性采样：坐标为 24.8 定点数，越界视为透明；结果放大 65536 倍。
    static inline void sample(const std::uint8_t* src, int side, int X, int Y, std::uint32_t* out) {
        int ix = X >> 8, iy = Y >> 8;
        if (X < 0 || Y < 0 || ix >= side - 1 || iy >= side - 1) { out[0] = out[1] = out[2] = out[3] = 0; return; }
        std::uint32_t fx = static_cast<std::uint32_t>(X & 255), fy = static_cast<std::uint32_t>(Y & 255);
        const auto* a = src + (static_cast<size_t>(iy) * side + ix) * 4; const auto* b = a + static_cast<size_t>(side) * 4;
        if (!(a[3] | a[7] | b[3] | b[7])) { out[0] = out[1] = out[2] = out[3] = 0; return; }   // 预乘格式：全透明像素颜色也为 0
        std::uint32_t w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy), w01 = (256 - fx) * fy, w11 = fx * fy;
        out[0] = a[0] * w00 + a[4] * w10 + b[0] * w01 + b[4] * w11;
        out[1] = a[1] * w00 + a[5] * w10 + b[1] * w01 + b[5] * w11;
        out[2] = a[2] * w00 + a[6] * w10 + b[2] * w01 + b[6] * w11;
        out[3] = a[3] * w00 + a[7] * w10 + b[3] * w01 + b[7] * w11;
    }
public:
    bool load(const void* memory, std::size_t bytes) {
        pairs_.clear();
        if (!memory || bytes < 16 || std::memcmp(memory, "LILIFLOW", 8) != 0) return false;
        auto* data = static_cast<const std::uint8_t*>(memory);
        std::uint32_t count = 0, grid = 0; std::memcpy(&count, data + 8, 4); std::memcpy(&grid, data + 12, 4);
        constexpr std::size_t entry = 4 + Grid * Grid * 4 * sizeof(std::int16_t);
        if (!count || count > 1770 || grid != Grid || bytes != 16 + count * entry) return false;
        std::vector<Pair> pairs(count); lookup_.fill(-1);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto* raw = data + 16 + i * entry; auto& p = pairs[i];
            std::memcpy(&p.a, raw, 2); std::memcpy(&p.b, raw + 2, 2);
            if (p.a >= p.b || p.b >= Animation::FrameCount) { lookup_.fill(-1); return false; }
            std::memcpy(p.values.data(), raw + 4, Grid * Grid * 4 * sizeof(std::int16_t));
            int reach = 0;
            for (auto value : p.values) {
                int magnitude = std::abs(static_cast<int>(value));
                if (magnitude > 6400) { lookup_.fill(-1); return false; }
                reach = std::max(reach, magnitude);
            }
            p.reach = reach / 64.f;
            lookup_[p.a * Animation::FrameCount + p.b] = static_cast<int>(i);
        }
        pairs_ = std::move(pairs);
        return true;
    }
    bool loaded() const { return !pairs_.empty(); }
    std::size_t pairCount() const { return pairs_.size(); }
    bool has(int a, int b) const {
        return a >= 0 && a < b && b < Animation::FrameCount && !pairs_.empty() && lookup_[a * Animation::FrameCount + b] >= 0;
    }
    // 在 side×side 的预乘 BGRA/RGBA 图像上渲染 a→b 的中间帧，t∈[0,1]。
    // box 给出两帧内容的并集（可省略）；框外输出透明。
    bool render(int a, int b, float t, const std::uint8_t* pixelsA, const std::uint8_t* pixelsB, std::uint8_t* output,
                int side = BaseSide, const PixelBox* box = nullptr) {
        if (!has(a, b) || side < 8) return false;
        const size_t bytes = static_cast<size_t>(side) * side * 4;
        if (t <= 0) { std::memcpy(output, pixelsA, bytes); return true; }
        if (t >= 1) { std::memcpy(output, pixelsB, bytes); return true; }
        const int index = lookup_[a * Animation::FrameCount + b];
        PixelBox area{0, 0, side, side};
        if (box && !box->empty()) {
            int grow = static_cast<int>(std::ceil(pairs_[index].reach * side / BaseSide)) + 2;
            area = PixelBox{std::max(0, box->left - grow), std::max(0, box->top - grow),
                            std::min(side, box->right + grow), std::min(side, box->bottom + grow)};
            std::memset(output, 0, bytes);
        }
        // 逐行把 33×33 位移网格在纵向插值，横向用增量累加，无需缓存稠密位移场（2.1 每切换一对帧就重算整张位移场）。
        const auto& v = pairs_[index].values;
        const float unitA = side / static_cast<float>(BaseSide) / 64.f * t * 256.f;
        const float unitB = side / static_cast<float>(BaseSide) / 64.f * (1 - t) * 256.f;
        const float step = (Grid - 1.f) / (side - 1.f);
        const std::uint32_t t8 = static_cast<std::uint32_t>(std::clamp(t, 0.f, 1.f) * 256.f + .5f);
        float column[Grid][4];
        for (int y = area.top; y < area.bottom; ++y) {
            float gy = y * step; int iy = std::min(Grid - 2, static_cast<int>(gy)); float fy = gy - iy;
            for (int gx = 0; gx < Grid; ++gx) for (int c = 0; c < 4; ++c)
                column[gx][c] = Animation::lerp(v[(iy * Grid + gx) * 4 + c], v[((iy + 1) * Grid + gx) * 4 + c], fy) * (c < 2 ? unitA : unitB);
            const float Y256 = static_cast<float>(y * 256);
            std::uint8_t* out = output + (static_cast<size_t>(y) * side + area.left) * 4;
            for (int x = area.left; x < area.right; ++x, out += 4) {
                float gx = x * step; int ix = std::min(Grid - 2, static_cast<int>(gx)); float fx = gx - ix;
                // 位移已乘以 t·256 或 (1-t)·256，直接得到 24.8 定点偏移（+0.5 四舍五入，负数用 floor 语义由越界判断兜底）。
                const float X256 = static_cast<float>(x * 256);
                auto fixed = [](float value) { return value < 0 ? -1 : static_cast<int>(value); };   // 正数截断即 floor；负数必然越界
                int ax = fixed(X256 - (column[ix][0] + (column[ix + 1][0] - column[ix][0]) * fx));
                int ay = fixed(Y256 - (column[ix][1] + (column[ix + 1][1] - column[ix][1]) * fx));
                int bx = fixed(X256 - (column[ix][2] + (column[ix + 1][2] - column[ix][2]) * fx));
                int by = fixed(Y256 - (column[ix][3] + (column[ix + 1][3] - column[ix][3]) * fx));
                std::uint32_t aa[4], bb[4];
                sample(pixelsA, side, ax, ay, aa);
                sample(pixelsB, side, bx, by, bb);
                std::uint32_t alpha = ((aa[3] >> 8) * (256 - t8) + (bb[3] >> 8) * t8 + 32768) >> 16;
                out[3] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, alpha));
                for (int c = 0; c < 3; ++c) {
                    std::uint32_t value = ((aa[c] >> 8) * (256 - t8) + (bb[c] >> 8) * t8 + 32768) >> 16;
                    out[c] = static_cast<std::uint8_t>(std::min<std::uint32_t>(out[3], value));
                }
            }
        }
        return true;
    }
};
