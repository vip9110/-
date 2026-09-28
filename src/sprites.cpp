#include "sprites.h"
#include "resource.h"
#include "sprite_metadata.h"

using namespace Gdiplus;

namespace {
bool resourceBytes(HINSTANCE instance, int id, const BYTE*& data, DWORD& size) {
    HRSRC found = FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!found) return false;
    size = SizeofResource(instance, found);
    HGLOBAL loaded = LoadResource(instance, found);
    data = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    return data && size;
}

// 资源内存只读，GDI+ 解码需要可寻址的流；复制一份到 HGLOBAL 后交给流对象托管。
std::unique_ptr<Bitmap> decodePng(const BYTE* data, DWORD size, IStream*& stream) {
    stream = nullptr;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;
    void* target = GlobalLock(memory);
    if (!target) { GlobalFree(memory); return nullptr; }
    std::memcpy(target, data, size); GlobalUnlock(memory);
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); return nullptr; }
    std::unique_ptr<Bitmap> bitmap(Bitmap::FromStream(stream));
    if (!bitmap || bitmap->GetLastStatus() != Ok) { bitmap.reset(); stream->Release(); stream = nullptr; }
    return bitmap;
}
} // namespace

bool SpriteBank::loadResources(HINSTANCE instance) {
    const BYTE* data = nullptr; DWORD size = 0;
    if (!resourceBytes(instance, IDR_MOTION_FLOW, data, size) || !flow_.load(data, size)) return false;
    constexpr int ids[] = {IDR_ATLAS, IDR_EXTRA_ATLAS, IDR_MOTION_WALK, IDR_MOTION_RELAX, IDR_MOTION_PLAY};
    for (int i = 0; i < 5; ++i) if (!resourceBytes(instance, ids[i], atlases_[i].data, atlases_[i].size)) return false;
    return true;
}

// 去掉图像生成工具留下的离散噪点，并把几乎不透明的身体补足为完全不透明（2.1 的身体只有 98% 不透明，桌面图标会隐约透出）。
void SpriteBank::cleanFrame(std::vector<BYTE>& pixels, int side, PixelBox& box) {
    const size_t count = static_cast<size_t>(side) * side;
    for (size_t p = 0; p < count; ++p) {
        BYTE* px = &pixels[p * 4];
        if (px[3] < 4) { px[0] = px[1] = px[2] = px[3] = 0; continue; }
        for (int c = 0; c < 3; ++c) px[c] = std::min(px[c], px[3]);
    }
    // 8 邻接连通域，只保留主体及足够大的部件。
    std::vector<int> label(count, 0), stack, sizes(1, 0);
    int next = 0;
    for (size_t start = 0; start < count; ++start) {
        if (!pixels[start * 4 + 3] || label[start]) continue;
        ++next; sizes.push_back(0); stack.assign(1, static_cast<int>(start)); label[start] = next;
        while (!stack.empty()) {
            int p = stack.back(); stack.pop_back(); ++sizes[next];
            int x = p % side, y = p / side;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy;
                if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= side || ny >= side) continue;
                int q = ny * side + nx;
                if (!label[q] && pixels[static_cast<size_t>(q) * 4 + 3]) { label[q] = next; stack.push_back(q); }
            }
        }
    }
    int largest = 0; for (int i = 1; i <= next; ++i) if (sizes[i] > largest) largest = sizes[i];
    const int keep = std::max(64, largest / 40);
    box = PixelBox{side, side, 0, 0};
    for (size_t p = 0; p < count; ++p) {
        BYTE* px = &pixels[p * 4];
        if (!px[3]) continue;
        if (sizes[label[p]] < keep) { px[0] = px[1] = px[2] = px[3] = 0; continue; }
        int a = px[3];
        int boosted = std::min(255, (a * 255 + 122) / 246);
        if (boosted != a) {
            for (int c = 0; c < 3; ++c) px[c] = static_cast<BYTE>(std::min(boosted, (px[c] * boosted + a / 2) / a));
            px[3] = static_cast<BYTE>(boosted);
        }
        int x = static_cast<int>(p % side), y = static_cast<int>(p / side);
        box.left = std::min(box.left, x); box.top = std::min(box.top, y);
        box.right = std::max(box.right, x + 1); box.bottom = std::max(box.bottom, y + 1);
    }
    if (box.empty()) box = PixelBox{};
}

bool SpriteBank::bake(float scale) {
    scale = std::clamp(scale, .75f, 1.6f);
    int side = static_cast<int>(std::lround(Animation::SpriteSize * scale));
    if (side == side_ && ready()) return true;
    const float k = side / static_cast<float>(Animation::SpriteSize);
    std::array<std::vector<BYTE>, Animation::FrameCount> frames;
    std::array<PixelBox, Animation::FrameCount> boxes{};
    ImageAttributes clampEdges; clampEdges.SetWrapMode(WrapModeTileFlipXY);
    for (int sheet = 0; sheet < 5; ++sheet) {
        IStream* stream = nullptr;
        auto atlas = decodePng(atlases_[sheet].data, atlases_[sheet].size, stream);
        if (!atlas) return false;
        bool ok = true;
        for (int f = 0; f < Animation::FrameCount && ok; ++f) {
            const auto& s = SPRITES[f];
            if (s.sheet != sheet) continue;
            PixelBuffer target;
            if (!target.create(side)) { ok = false; break; }
            {
                Graphics g(target.bitmap.get());
                g.SetCompositingMode(CompositingModeSourceCopy);
                g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                RectF dest((Animation::PivotX - s.pivotX * s.ratio) * k, (Animation::PivotY - s.pivotY * s.ratio) * k, s.w * s.ratio * k, s.h * s.ratio * k);
                ok = g.DrawImage(atlas.get(), dest, static_cast<REAL>(s.x), static_cast<REAL>(s.y), static_cast<REAL>(s.w), static_cast<REAL>(s.h), UnitPixel, &clampEdges) == Ok;
                g.Flush(FlushIntentionSync);
            }
            target.bitmap.reset();
            frames[f] = std::move(target.pixels);
            if (ok) cleanFrame(frames[f], side, boxes[f]);
        }
        atlas.reset(); if (stream) stream->Release();
        if (!ok) return false;
    }
    for (auto& f : frames) if (f.empty()) return false;
    frames_ = std::move(frames); boxes_ = boxes;
    side_ = side; bakeScale_ = k;
    if (!body_.create(side)) { side_ = 0; return false; }
    cached_.fill(-1.f);
    render(Animation::Pose());
    return true;
}

void SpriteBank::render(const Animation::Pose& pose) {
    if (!ready() || cached_ == pose.weights) return;
    cached_ = pose.weights;
    std::array<int, Animation::FrameCount> ids{}, weights{};
    int active = 0, total = 0;
    for (int i = 0; i < Animation::FrameCount; ++i) if (pose.weights[i] > .000001f) {
        ids[active] = i; weights[active] = static_cast<int>(std::lround(pose.weights[i] * 65536.f));
        total += weights[active++];
    }
    lastActive_ = active;
    if (!active) return;
    int largest = 0; for (int i = 1; i < active; ++i) if (weights[i] > weights[largest]) largest = i;
    weights[largest] += 65536 - total;
    const size_t bytes = static_cast<size_t>(side_) * side_ * 4;
    BYTE* out = body_.pixels.data();
    if (active == 1) { std::memcpy(out, frames_[ids[0]].data(), bytes); return; }
    PixelBox area{side_, side_, 0, 0};
    for (int i = 0; i < active; ++i) {
        const auto& b = boxes_[ids[i]];
        area.left = std::min(area.left, b.left); area.top = std::min(area.top, b.top);
        area.right = std::max(area.right, b.right); area.bottom = std::max(area.bottom, b.bottom);
    }
    if (active == 2 && flow_.render(ids[0], ids[1], weights[1] / 65536.f, frames_[ids[0]].data(), frames_[ids[1]].data(), out, side_, &area)) return;
    {
        // 三帧以上（动作切换时的交叉淡入）或没有直接位移向量的两帧：经由中性帧做多帧形变，避免重影。
        std::array<int, Animation::FrameCount> order{};
        for (int i = 0; i < active; ++i) order[i] = i;
        std::sort(order.begin(), order.begin() + active, [&](int a, int b) { return weights[a] > weights[b]; });
        int used = std::min(active, 4), sum = 0;
        int frames[4]; float share[4]; const BYTE* pixels[4];
        for (int i = 0; i < used; ++i) sum += weights[order[i]];
        for (int i = 0; i < used; ++i) {
            frames[i] = ids[order[i]]; share[i] = weights[order[i]] / static_cast<float>(sum); pixels[i] = frames_[frames[i]].data();
        }
        if (flow_.renderMulti(frames, share, used, pixels, out, side_, &area)) return;
    }
    // 预乘像素的线性混合保持不透明度；普通叠加淡入淡出会让身体发暗、出现重影。
    std::memset(out, 0, bytes);
    for (int y = area.top; y < area.bottom; ++y) {
        size_t row = (static_cast<size_t>(y) * side_ + area.left) * 4, end = (static_cast<size_t>(y) * side_ + area.right) * 4;
        for (size_t p = row; p < end; ++p) {
            unsigned sum = 32768;
            for (int i = 0; i < active; ++i) sum += static_cast<unsigned>(frames_[ids[i]][p]) * weights[i];
            out[p] = static_cast<BYTE>(std::min(255u, sum >> 16));
        }
    }
}
