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
    // 多帧形变：单帧权重 1 时等于原帧；两帧（与 20 相关）时接近直接成对插值；三帧时保持预乘约束。
    for (int f : {20, 29, 15}) assert(flow.hasNeutralField(f));
    std::vector<std::uint8_t> m20(bytes,0), m29(bytes,0), m15(bytes,0), multi(bytes), pairOut(bytes);
    for(int y=40;y<250;++y) for(int x=50;x<210;++x){ int p=(y*side+x)*4; auto al=static_cast<std::uint8_t>(90+(x+2*y)%160);
        m20[p+3]=al; m20[p]=al/2; m29[p+3]=al; m29[p+1]=al/2; m15[p+3]=al; m15[p+2]=al/3; }
    {
        int frames[1]={29}; float w[1]={1.f}; const std::uint8_t* px[1]={m29.data()};
        assert(flow.renderMulti(frames,w,1,px,multi.data()));
        size_t diff=0, n=0; for(int p=3;p<bytes;p+=4) if(m29[p]||multi[p]){ diff+=std::abs(int(multi[p])-int(m29[p])); ++n; }
        std::cout<<"INFO single-frame multi morph mean alpha difference "<<double(diff)/n<<"\n";
        assert(double(diff)/n < 12);   // 定点迭代只做两次，高频测试图案上有少量采样偏差
    }
    {
        int frames[2]={20,29}; float w[2]={.6f,.4f}; const std::uint8_t* px[2]={m20.data(),m29.data()};
        assert(flow.renderMulti(frames,w,2,px,multi.data()) && flow.render(20,29,.4f,m20.data(),m29.data(),pairOut.data()));
        double diff=0; size_t n=0; for(int p=3;p<bytes;p+=4){ if(multi[p]||pairOut[p]){ diff+=std::abs(int(multi[p])-int(pairOut[p])); ++n; } }
        std::cout<<"INFO multi-frame vs pairwise mean alpha difference "<<diff/n<<"\n";
        assert(diff/n < 12);
    }
    {
        int frames[3]={15,20,29}; float w[3]={.3f,.3f,.4f}; const std::uint8_t* px[3]={m15.data(),m20.data(),m29.data()};
        PixelBox box{50,40,210,250};
        assert(flow.renderMulti(frames,w,3,px,multi.data(),side,&box));
        size_t visible=0; for(int p=0;p<bytes;p+=4){ for(int c=0;c<3;++c) assert(multi[p+c]<=multi[p+3]); if(multi[p+3]>60) ++visible; }
        assert(visible > 20000);
    }
    std::cout<<"PASS multi-frame morph through the neutral frame keeps alpha and matches pairwise interpolation\n";
}
