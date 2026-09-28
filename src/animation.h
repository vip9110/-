#pragma once
// 平台无关的动画时间轴。Windows 绘制、单元测试与离线预览共用此文件。
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace Animation {
constexpr int FrameCount = 60, Neutral = 20, SpriteSize = 256;
constexpr float PivotX = 128.f, PivotY = 244.f, Pi = 3.14159265358979323846f;
// 前 16 个动作与 2.1 相同；3.0 在末尾追加欢呼、享受抚摸、好奇嗅嗅。
enum class Motion { Stand, Blink, Walk, Stretch, Yawn, Scratch, Roll, Sniff, Look, Hop, Feed, Sleep, Wake, Sick, Carry, Land,
    Cheer, Enjoy, Curious };
constexpr int MotionCount = static_cast<int>(Motion::Curious) + 1;

inline float smooth(float x) { x = std::clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float approach(float value, float target, float dt, float rate = 8.f) { return lerp(value, target, 1.f - std::exp(-rate * dt)); }

struct Pose {
    std::array<float, FrameCount> weights{};
    float x = 0, y = 0, sx = 1, sy = 1, angle = 0;
    explicit Pose(int frame = Neutral) { weights[frame] = 1; }
};
inline Pose blend(const Pose& a, const Pose& b, float t) {
    Pose p; for (int i = 0; i < FrameCount; ++i) p.weights[i] = lerp(a.weights[i], b.weights[i], t);
    p.x = lerp(a.x,b.x,t); p.y = lerp(a.y,b.y,t); p.sx = lerp(a.sx,b.sx,t); p.sy = lerp(a.sy,b.sy,t); p.angle = lerp(a.angle,b.angle,t);
    return p;
}
struct Key { float time; int frame; };
inline Pose keys(float t, std::initializer_list<Key> list) {
    auto a = list.begin();
    if (t <= a->time) return Pose(a->frame);
    for (auto b = a + 1; b != list.end(); ++a, ++b) {
        if (t <= b->time) return blend(Pose(a->frame), Pose(b->frame), smooth((t - a->time) / (b->time - a->time)) * .35f + (t - a->time) / (b->time - a->time) * .65f);
    }
    return Pose(a->frame);
}
// 有限动作的时长；0 表示循环或持续姿态。
inline float duration(Motion m) {
    switch (m) {
    case Motion::Blink: return .38f;
    case Motion::Stretch: return 3.50f;
    case Motion::Yawn: return 3.20f;
    case Motion::Scratch: return 3.30f;
    case Motion::Roll: return 4.60f;
    case Motion::Sniff: return 2.80f;
    case Motion::Look: return 3.40f;
    case Motion::Hop: return 1.35f;
    case Motion::Feed: return 2.80f;
    case Motion::Wake: return 1.60f;
    case Motion::Land: return .65f;
    case Motion::Cheer: return 1.90f;
    case Motion::Enjoy: return 1.90f;
    default: return 0;
    }
}
inline const wchar_t* name(Motion m) {
    switch (m) {
    case Motion::Blink: return L"眨眨眼"; case Motion::Walk: return L"轻轻散步";
    case Motion::Stretch: return L"伸个懒腰"; case Motion::Yawn: return L"打个哈欠";
    case Motion::Scratch: return L"挠挠脸颊"; case Motion::Roll: return L"打滚玩耍";
    case Motion::Sniff: return L"低头嗅嗅"; case Motion::Look: return L"好奇张望";
    case Motion::Hop: return L"开心蹦蹦"; case Motion::Feed: return L"咔嚓开饭";
    case Motion::Sleep: return L"呼呼小睡"; case Motion::Wake: return L"慢慢醒来";
    case Motion::Sick: return L"休息养病"; case Motion::Carry: return L"被提起来啦";
    case Motion::Land: return L"轻轻落地"; case Motion::Cheer: return L"举手欢呼";
    case Motion::Enjoy: return L"眯眼享受"; case Motion::Curious: return L"闻到好吃的";
    default: return L"安静陪伴";
    }
}

inline Pose sample(Motion m, float t) {
    Pose p;
    switch (m) {
    case Motion::Stand: break;
    case Motion::Blink:
        p = keys(t, {{0,20},{.07f,21},{.13f,22},{.22f,22},{.30f,23},{.38f,20}}); break;
    case Motion::Walk: {
        float cycle = std::fmod(std::max(t, 0.f), .88f) / .11f;
        int a = std::min(7, static_cast<int>(cycle)); p = blend(Pose(12 + a),Pose(12 + (a + 1) % 8),cycle - a);
        p.y = -1.5f * (1 - std::cos(t * 4 * Pi / .88f)) * .5f;
        p.angle = .65f * std::sin(t * 2 * Pi / .88f); break;
    }
    case Motion::Stretch:
        p = keys(t, {{0,20},{.35f,29},{.75f,30},{1.1f,31},{1.50f,32},{2.10f,32},{2.6f,33},{3.10f,34},{3.5f,20}});
        p.sy = 1 + .025f * std::sin(Pi * std::clamp(t/3.5f,0.f,1.f)); break;
    case Motion::Yawn:
        p = keys(t, {{0,20},{.35f,37},{.70f,38},{1.1f,39},{1.45f,40},{1.95f,40},{2.35f,41},{2.85f,42},{3.2f,20}}); break;
    case Motion::Scratch:
        p = keys(t, {{0,20},{.3f,45},{.60f,46},{.9f,47},{1.15f,48},{1.4f,49},{1.65f,48},{1.9f,49},{2.15f,48},{2.4f,49},{2.85f,50},{3.3f,20}}); break;
    case Motion::Roll:
        p = keys(t, {{0,20},{.5f,53},{1.f,54},{1.5f,55},{1.95f,56},{2.35f,55},{2.75f,56},{3.25f,57},{3.85f,58},{4.6f,20}});
        if (t > 1.5f && t < 3.25f) {
            float envelope = std::sin(Pi * (t - 1.5f) / 1.75f);
            p.angle = 7.f * std::sin((t-1.5f) * 7.2f) * envelope;
            p.x = 4.f * std::sin((t-1.5f) * 7.2f) * envelope;
        }
        break;
    case Motion::Sniff: {
        p = keys(t, {{0,20},{.65f,10},{1.9f,10},{2.8f,20}});
        float envelope = std::sin(Pi * std::clamp(t/2.8f,0.f,1.f));
        p.angle = 3.f * envelope; p.x = 2.f * std::sin(t * 12) * envelope; break;
    }
    case Motion::Look: {
        float phase = std::clamp(t / 3.4f, 0.f, 1.f);
        float sway = std::sin(phase * 2 * Pi) * std::sin(phase * Pi);
        p.angle = sway * 4.f; p.x = sway * 5.f;
        if (t > 1.5f && t < 1.88f) p.weights = sample(Motion::Blink,t-1.5f).weights;
        break;
    }
    case Motion::Hop: {
        p = keys(t, {{0,20},{.18f,25},{.36f,26},{.65f,26},{.86f,25},{1.12f,20},{1.35f,20}});
        if (t > .23f && t < .82f) p.y = -15.f * std::sin((t-.23f)/.59f*Pi);
        float squash = std::exp(-std::pow((t-.17f)/.085f,2.f)) + std::exp(-std::pow((t-.86f)/.10f,2.f));
        p.sy = 1 - .065f * squash; p.sx = 1 + .035f * squash; break;
    }
    case Motion::Feed:
        p = keys(t, {{0,20},{.30f,25},{.55f,26},{.80f,25},{1.05f,26},{1.30f,25},{1.55f,26},{1.8f,25},{2.1f,21},{2.4f,20},{2.8f,20}});
        p.sx = 1 + .010f * std::sin(t*20) * std::sin(std::clamp(t/2.8f,0.f,1.f)*Pi); break;
    case Motion::Sleep:
        p = keys(t, {{0,20},{.35f,37},{.65f,38},{.95f,40},{1.4f,41},{1.8f,53},{2.2f,54},{2.9f,4}}); break;
    case Motion::Wake:
        p = keys(t, {{0,4},{.4f,54},{.85f,53},{1.3f,20},{1.6f,20}}); break;
    case Motion::Sick:
        p = keys(t, {{0,20},{.6f,11}}); p.angle = .65f * std::sin(t*2.f); break;
    case Motion::Carry:
        p = Pose(26); p.angle = 3.5f * std::sin(t*5.f); p.y = -3.f; break;
    case Motion::Land: {
        p = keys(t, {{0,25},{.3f,20},{.65f,20}});
        float envelope = std::sin(Pi*std::clamp(t/.65f,0.f,1.f));
        p.sy = 1 - .065f*envelope; p.sx = 1 + .035f*envelope; break;
    }
    case Motion::Cheer: {
        // 使用原始图集里双手高举的开心姿态，两次小跳。
        p = keys(t, {{0,20},{.28f,5},{1.5f,5},{1.9f,20}});
        if (t > .3f && t < 1.45f) {
            float hop = std::fmod(t - .3f, .575f) / .575f;
            p.y = -9.f * std::sin(hop * Pi);
        }
        float squash = std::exp(-std::pow((t-.26f)/.08f,2.f)) + std::exp(-std::pow((t-1.5f)/.09f,2.f));
        p.sy = 1 - .05f * squash; p.sx = 1 + .03f * squash; break;
    }
    case Motion::Enjoy: {
        // 被抚摸时眯眼、轻轻左右摇。
        p = keys(t, {{0,20},{.12f,21},{.24f,22},{1.5f,22},{1.7f,21},{1.9f,20}});
        float envelope = std::sin(Pi * std::clamp(t/1.9f,0.f,1.f));
        p.angle = 2.6f * std::sin(t * 6.f) * envelope;
        p.sy = 1 - .018f * envelope; p.sx = 1 + .012f * envelope; break;
    }
    case Motion::Curious: {
        // 拖着文件悬停在身上时，低头嗅嗅并持续抽动鼻子（循环）。
        p = keys(t, {{0,20},{.55f,10}});
        float envelope = std::clamp(t / .55f, 0.f, 1.f);
        p.x = 1.6f * std::sin(t * 13.f) * envelope; p.angle = 2.f * envelope + .8f * std::sin(t * 5.f) * envelope; break;
    }
    }
    return p;
}

class Player {
    Motion motion_ = Motion::Stand;
    float time_ = 0, clock_ = 0, fade_ = 1, fadeDuration_ = .22f;
    float facing_ = 1, facingFrom_ = 1, turnTime_ = 1;
    int direction_ = 1;
    Pose from_;
public:
    Motion motion() const { return motion_; }
    float time() const { return time_; }
    float clock() const { return clock_; }
    float facing() const { return facing_; }
    int direction() const { return direction_; }
    bool turning() const { return turnTime_ < .28f; }
    bool blending() const { return fade_ < 1.f || turnTime_ < .28f; }
    bool finished() const { float d = duration(motion_); return d > 0 && time_ >= d; }
    void reset(int direction = 1) {
        *this = Player{}; direction_ = direction; facing_ = facingFrom_ = static_cast<float>(direction);
    }
    Pose rawPose() const { return blend(from_, sample(motion_,time_),smooth(fade_)); }
    Pose pose() const {
        Pose p = rawPose();
        float sleepMix = std::clamp(p.weights[4],0.f,1.f);
        float breath = std::sin(clock_ * lerp(2.25f,1.5f,sleepMix));
        p.sy *= 1.f + breath * lerp(.006f,.015f,sleepMix);
        p.sx *= 1.f - breath * .002f;
        p.y -= std::abs(std::sin(p.angle*Pi/180.f))*60.f;
        return p;
    }
    void play(Motion requested, bool restart = false) {
        if (requested != motion_ || restart) {
            from_ = rawPose(); motion_ = requested; time_ = 0; fade_ = 0;
            fadeDuration_ = requested == Motion::Blink ? .07f : requested == Motion::Sleep || requested == Motion::Wake ? .36f : .22f;
        }
    }
    void advance(float dt, Motion requested, int direction) {
        dt = std::clamp(dt,0.f,.05f); // 不补播卡顿、隐藏或对话框期间的画面。
        play(requested);
        if (direction != direction_) {
            facingFrom_ = facing_; direction_ = direction; turnTime_ = 0;
        }
        turnTime_ += dt;
        facing_ = lerp(facingFrom_,static_cast<float>(direction_),smooth(turnTime_/.28f));
        time_ += dt; clock_ += dt; fade_ = std::min(1.f,fade_ + dt/fadeDuration_);
    }
};
} // namespace Animation
