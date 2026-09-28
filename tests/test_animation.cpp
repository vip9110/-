#include "../src/animation.h"
#include "../src/sprite_metadata.h"
#include "../src/sprite_anchors.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace Animation;
void valid(const Pose& p) {
    float sum=0;
    for (float w:p.weights) { assert(std::isfinite(w) && w>=-.00001f && w<=1.00001f); sum+=w; }
    assert(std::abs(sum-1)<.0001f);
    assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.angle));
    assert(p.sx>.8f && p.sy>.8f && p.sx<1.2f && p.sy<1.2f);
}
void same(const Pose& a,const Pose& b) {
    for (int i=0;i<FrameCount;++i) assert(std::abs(a.weights[i]-b.weights[i])<.00001f);
    assert(std::abs(a.x-b.x)<.0001f && std::abs(a.y-b.y)<.0001f && std::abs(a.angle-b.angle)<.0001f);
}
int main() {
    for (int m=0;m<MotionCount;++m) {
        for (float t=0;t<30;t+=.007f) valid(sample(static_cast<Motion>(m),t));
    }
    std::cout<<"PASS all "<<MotionCount<<" motions: valid normalized frame weights and transforms\n";
    for (Motion m:{Motion::Stretch,Motion::Yawn,Motion::Scratch,Motion::Roll,Motion::Sniff,Motion::Look,Motion::Hop,Motion::Feed,Motion::Wake,Motion::Land,Motion::Cheer,Motion::Enjoy}) {
        assert(duration(m)>0);
        assert(sample(m,duration(m)).weights[Neutral]>.999f);
    }
    same(sample(Motion::Walk,0),sample(Motion::Walk,.88f));
    for (float t=0;t<5;t+=.0001f) { auto p=sample(Motion::Walk,t); for (int i=0;i<12;++i) assert(p.weights[i]==0); for (int i=20;i<FrameCount;++i) assert(p.weights[i]==0); }
    std::cout<<"PASS finite actions return to neutral; walk cycle has identical endpoints and never leaves its eight frames\n";
    Player p;
    for (int i=0;i<600;++i) {
        Motion m=static_cast<Motion>(i%MotionCount);
        auto before=p.pose(); p.advance(0,m,i%2?1:-1); same(before,p.pose());
        p.advance(.016f,m,i%2?1:-1); valid(p.pose());
    }
    std::cout<<"PASS rapid interruptions preserve the exact pose at the transition boundary\n";
    p.advance(.04f,Motion::Hop,1); auto before=p.pose(); p.play(Motion::Hop,true); same(before,p.pose()); assert(p.time()==0);
    std::cout<<"PASS repeated pet interactions restart the hop without a visual cut\n";
    p.reset(); p.advance(.04f,Motion::Walk,1);
    float start=p.time(); p.advance(9.f,Motion::Walk,1); assert(std::abs(p.time()-start-.05f)<.00001f);
    std::cout<<"PASS long frame stalls cannot skip an entire action\n";
    p.reset(); float previous=p.facing();
    for (int i=0;i<25;++i) { p.advance(.016f,Motion::Walk,-1); assert(std::abs(p.facing()-previous)<.18f); previous=p.facing(); }
    assert(p.facing()==-1 && !p.turning() && p.direction()==-1);
    std::cout<<"PASS turn interpolates continuously and reaches the requested direction\n";
    float speed=0;
    for (int i=0;i<120;++i) speed=approach(speed,34.f,1.f/60);
    assert(speed>33.99f);
    for (int i=0;i<120;++i) speed=approach(speed,0.f,1.f/60);
    assert(speed<.001f);
    std::cout<<"PASS locomotion accelerates and settles without a speed step\n";
    for (const auto& s:SPRITES) {
        float l=PivotX-s.pivotX*s.ratio,t=PivotY-s.pivotY*s.ratio;
        assert(l>=0 && t>=0 && l+s.w*s.ratio<SpriteSize && t+s.h*s.ratio<SpriteSize);
    }
    std::cout<<"PASS sixty registered sprites fit the shared canvas without clipping\n";
    for (const auto& h:HEAD_ANCHORS) {
        float scale=std::hypot(h.a,h.b); assert(scale>.8f && scale<1.35f);
        assert(std::abs(h.tx)<120 && std::abs(h.ty)<120 && (h.visible==0.f || h.visible==1.f));
        // 围巾中心映射后仍在画布内
        float x=h.a*152-h.b*171+h.tx, y=h.b*152+h.a*171+h.ty; assert(x>20 && x<236 && y>40 && y<252);
    }
    assert(HEAD_ANCHORS[Neutral].a==1.f && HEAD_ANCHORS[Neutral].b==0.f && HEAD_ANCHORS[Neutral].tx==0.f);
    std::cout<<"PASS head anchors keep the scarf on the character in all sixty frames\n";
    p.reset(); p.advance(.016f,Motion::Cheer,1); assert(!p.finished()); for(int i=0;i<200;++i) p.advance(.016f,Motion::Cheer,1); assert(p.finished());
    std::cout<<"PASS finished() reports completion of finite motions\n";
}
