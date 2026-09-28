#include "../src/motion_flow.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
int main(int argc,char** argv) {
    std::string path=argc>1?argv[1]:"assets/motion-vectors.bin";
    std::ifstream file(path,std::ios::binary);
    std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(file),{}};
    assert(!data.empty());
    MotionFlow flow; assert(!flow.load(nullptr,0)); assert(!flow.load(data.data(),data.size()-1)); assert(!flow.loaded());
    assert(flow.load(data.data(),data.size()) && flow.pairCount()==94 && flow.has(12,13) && !flow.has(13,12) && !flow.has(1,2));
    constexpr int side=Animation::SpriteSize, bytes=side*side*4;
    std::vector<std::uint8_t> a(bytes),b(bytes),out(bytes),boxed(bytes);
    for(int p=0;p<bytes;p+=4) { auto alpha=static_cast<std::uint8_t>((p/4)%256); a[p]=alpha;a[p+1]=alpha/2;a[p+3]=alpha; b[p+2]=alpha;b[p+3]=alpha; }
    assert(flow.render(12,13,0,a.data(),b.data(),out.data()) && out==a);
    assert(flow.render(12,13,1,a.data(),b.data(),out.data()) && out==b);
    assert(!flow.render(-1,13,.5f,a.data(),b.data(),out.data()));
    assert(!flow.render(60,61,.5f,a.data(),b.data(),out.data()));
    std::cout<<"PASS vector resource validation and exact endpoint frames\n";
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<90;++i) {
        assert(flow.render(12,13,(i+.5f)/90,a.data(),b.data(),out.data()));
        for(int p=0;p<bytes;p+=4) for(int c=0;c<3;++c) assert(out[p+c]<=out[p+3]);
    }
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/90;
    std::cout<<"PASS interpolated frames preserve premultiplied alpha\n";
    std::cout<<"INFO offline kernel mean "<<elapsed<<" ms/frame at 256px including validation (not a Windows benchmark)\n";
    // 包围盒：框外全部为空时，只算框内的结果与全图计算一致。
    std::vector<std::uint8_t> sa(bytes,0),sb(bytes,0);
    PixelBox box{60,50,200,240};
    for(int y=box.top;y<box.bottom;++y) for(int x=box.left;x<box.right;++x) { int p=(y*side+x)*4; std::uint8_t al=static_cast<std::uint8_t>(100+(x*7+y*3)%150); sa[p+3]=al; sa[p]=al/2; sb[p+3]=al; sb[p+1]=al/3; }
    assert(flow.render(20,21,.4f,sa.data(),sb.data(),out.data()));
    assert(flow.render(20,21,.4f,sa.data(),sb.data(),boxed.data(),side,&box));
    assert(out==boxed);
    std::cout<<"PASS bounding-box rendering matches full-frame rendering\n";
    // 放大到 384 边长：端点帧不变，中间帧保持预乘约束。
    constexpr int big=384, bigBytes=big*big*4;
    std::vector<std::uint8_t> ba(bigBytes),bb(bigBytes),bo(bigBytes);
    for(int p=0;p<bigBytes;p+=4){ auto al=static_cast<std::uint8_t>((p/4)%200); ba[p]=al;ba[p+3]=al; bb[p+1]=al/2;bb[p+3]=al; }
    assert(flow.render(38,39,0,ba.data(),bb.data(),bo.data(),big) && bo==ba);
    start=std::chrono::steady_clock::now();
    for(int i=0;i<30;++i){ assert(flow.render(38,39,(i+.5f)/30,ba.data(),bb.data(),bo.data(),big)); for(int p=0;p<bigBytes;p+=4) for(int c=0;c<3;++c) assert(bo[p+c]<=bo[p+3]); }
    elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30;
    std::cout<<"PASS scaled 384px interpolation keeps premultiplied alpha ("<<elapsed<<" ms/frame incl. validation)\n";
}
