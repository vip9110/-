#pragma once
// 角色帧库：从嵌入的透明 PNG 图集裁出 60 帧，按当前显示比例烘焙成统一画布，并负责逐帧合成身体。
#include "win_common.h"
#include "gfx.h"
#include "animation.h"
#include "motion_flow.h"

class SpriteBank {
public:
    bool loadResources(HINSTANCE instance);
    // 以 scale（1.0 = 256 像素边长）重新烘焙全部帧；比例未变时直接返回。
    bool bake(float scale);
    int side() const { return side_; }
    float bakeScale() const { return bakeScale_; }
    // 按姿态权重合成身体图像（权重不变时复用上一帧结果）。
    void render(const Animation::Pose& pose);
    const PixelBuffer& body() const { return body_; }
    const std::vector<BYTE>& frame(int index) const { return frames_[index]; }
    const PixelBox& box(int index) const { return boxes_[index]; }
    MotionFlow& flow() { return flow_; }
    bool ready() const { return side_ > 0; }
    int lastActiveFrames() const { return lastActive_; }
private:
    struct Resource { const BYTE* data = nullptr; DWORD size = 0; };
    std::array<Resource, 5> atlases_{};
    std::array<std::vector<BYTE>, Animation::FrameCount> frames_;
    std::array<PixelBox, Animation::FrameCount> boxes_{};
    std::array<float, Animation::FrameCount> cached_{};
    PixelBuffer body_;
    MotionFlow flow_;
    int side_ = 0, lastActive_ = 0;
    float bakeScale_ = 0;
    static void cleanFrame(std::vector<BYTE>& pixels, int side, PixelBox& box);
};
