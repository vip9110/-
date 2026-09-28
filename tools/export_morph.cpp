// Optional offline preview bridge: exposes the exact native interpolation kernel to Python.
#include "../src/motion_flow.h"
#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C"
#endif
EXPORT void* flow_new(const void* bytes,std::size_t count) {
    auto* p=new MotionFlow;
    if (!p->load(bytes,count)) { delete p; return nullptr; }
    return p;
}
EXPORT void flow_delete(void* handle) { delete static_cast<MotionFlow*>(handle); }
EXPORT bool flow_render(void* handle,int a,int b,float t,const std::uint8_t* srcA,const std::uint8_t* srcB,std::uint8_t* out) {
    return handle && static_cast<MotionFlow*>(handle)->render(a,b,t,srcA,srcB,out);
}
