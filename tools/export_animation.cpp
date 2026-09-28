// Export the same animation Player used by the exe; no Windows UI or file recycling involved.
#include "../src/animation.h"
#include <cstdio>
#include <set>
using namespace Animation;
int main(int argc,char**) {
    if (argc>1) {
        std::set<std::pair<int,int>> pairs;
        for (int m=0;m<MotionCount;++m) for (float t=0;t<7;t+=.009f) {
            auto p=sample(static_cast<Motion>(m),t); int previous=-1;
            for (int i=0;i<FrameCount;++i) if (p.weights[i]>.0001f) { if (previous>=0) pairs.insert({previous,i}); previous=i; }
        }
        for (int i=0;i<FrameCount;++i) if(i!=Neutral) pairs.insert({std::min(i,Neutral),std::max(i,Neutral)});
        for (auto pair:pairs) std::printf("%d %d\n",pair.first,pair.second);
        return 0;
    }
    Player players[3];
    std::puts("[");
    for (int frame=0;frame<600;++frame) {
        float t=frame/60.f;
        Motion requested[]={Motion::Stand,Motion::Stand,Motion::Stand};
        if ((t>.4f && t<3.5f) || (t>4.3f && t<8.9f)) requested[0]=Motion::Walk;
        if (t>.4f && t<3.91f) requested[1]=Motion::Stretch;
        else if (t>4.4f && t<7.62f) requested[1]=Motion::Yawn;
        else if (t>8.1f && t<8.5f) requested[1]=Motion::Blink;
        if (t>.4f && t<3.72f) requested[2]=Motion::Scratch;
        else if (t>4.15f && t<8.77f) requested[2]=Motion::Roll;
        std::printf("%s[",frame?",":"");
        for (int j=0;j<3;++j) {
            players[j].advance(1.f/60,requested[j],j==0 && t>4.f?-1:1);
            auto p=players[j].pose();
            std::printf("%s{\"motion\":%d,\"x\":%.6f,\"y\":%.6f,\"sx\":%.6f,\"sy\":%.6f,\"angle\":%.6f,\"facing\":%.6f,\"weights\":[",j?",":"",static_cast<int>(players[j].motion()),p.x,p.y,p.sx,p.sy,p.angle,players[j].facing());
            bool first=true;
            for (int i=0;i<FrameCount;++i) if (p.weights[i]>.000001f) { std::printf("%s[%d,%.7f]",first?"":",",i,p.weights[i]); first=false; }
            std::printf("]}");
        }
        std::puts("]");
    }
    std::puts("]");
}
