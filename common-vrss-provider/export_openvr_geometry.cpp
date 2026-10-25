#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include "third_party/openvr/openvr.h"
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    vr::EVRInitError error{};
    auto vrSystem=vr::VR_Init(&error,vr::VRApplication_Background);
    if(!vrSystem || error!=vr::VRInitError_None) return 3;
    vr::ETrackedPropertyError a{},b{};
    auto vid=vrSystem->GetInt32TrackedDeviceProperty(0,vr::Prop_EdidVendorID_Int32,&a);
    auto pid=vrSystem->GetInt32TrackedDeviceProperty(0,vr::Prop_EdidProductID_Int32,&b);
    if(a!=vr::TrackedProp_Success || b!=vr::TrackedProp_Success || vid<=0 || pid<=0) {
        vr::VR_Shutdown(); return 4;
    }
    std::ofstream out{std::filesystem::path(argv[1])}; out<<std::setprecision(10);
    out<<"format=1\nvendor="<<vid<<"\nproduct="<<pid<<"\n";
    for(int i=0;i<2;++i) {
        float l,r,t,d;
        vrSystem->GetProjectionRaw(static_cast<vr::EVREye>(i),&l,&r,&t,&d);
        auto pose=vrSystem->GetEyeToHeadTransform(static_cast<vr::EVREye>(i));
        out<<"projection"<<i<<"="<<l<<","<<r<<","<<t<<","<<d<<"\n";
        out<<"eyeToHead"<<i<<"=";
        for(int row=0;row<3;++row) for(int col=0;col<3;++col)
            out<<(row||col?",":"")<<pose.m[row][col];
        out<<"\n";
    }
    std::cout<<"vendor="<<vid<<" product="<<pid<<" runtime="<<vrSystem->GetRuntimeVersion()<<"\n";
    vr::VR_Shutdown();
    return out.good()?0:5;
}
