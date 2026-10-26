#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "third_party/openvr/openvr.h"

namespace fs=std::filesystem;
constexpr const char* kAppKey="vrss.tuner.autostart";

static bool Running(const wchar_t* name) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return false;
    PROCESSENTRY32W item{};item.dwSize=sizeof(item);
    bool found=false;
    if(Process32FirstW(snapshot,&item))do {
        if(_wcsicmp(item.szExeFile,name)==0){found=true;break;}
    }while(Process32NextW(snapshot,&item));
    CloseHandle(snapshot);return found;
}

static fs::path Bundle() {
    wchar_t path[32768]{};
    DWORD size=GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));
    return size && size<std::size(path)?fs::path(path).parent_path():fs::path();
}

static std::string Utf8(const std::wstring& value) {
    int size=WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,nullptr,0,nullptr,nullptr);
    if(size<=0)return {};
    std::string bytes(size,'\0');
    WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,bytes.data(),size,nullptr,nullptr);
    bytes.pop_back();return bytes;
}

static int Configure(bool enable) {
    auto manifest=Bundle()/L"vrss-tuner.vrmanifest";
    if(!fs::is_regular_file(manifest))return 10;
    vr::EVRInitError init=vr::VRInitError_None;
    vr::VR_Init(&init,vr::VRApplication_Utility);
    if(init!=vr::VRInitError_None)return 11;
    auto* apps=vr::VRApplications();
    if(!apps){vr::VR_Shutdown();return 12;}
    const auto path=Utf8(manifest.wstring());
    const auto result=enable?apps->AddApplicationManifest(path.c_str(),false):
        apps->SetApplicationAutoLaunch(kAppKey,false);
    if(result!=vr::VRApplicationError_None){
        std::cerr<<apps->GetApplicationsErrorNameFromEnum(result)<<'\n';
        vr::VR_Shutdown();return 13;
    }
    const auto setting=enable?apps->SetApplicationAutoLaunch(kAppKey,true):
        apps->RemoveApplicationManifest(path.c_str());
    if(setting!=vr::VRApplicationError_None){
        std::cerr<<apps->GetApplicationsErrorNameFromEnum(setting)<<'\n';
        vr::VR_Shutdown();return 14;
    }
    if(enable && !apps->GetApplicationAutoLaunch(kAppKey)){
        vr::VR_Shutdown();return 15;
    }
    vr::VR_Shutdown();return 0;
}

static int LastSource() {
    wchar_t local[32768]{};
    DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",local,DWORD(std::size(local)));
    if(!size || size>=std::size(local))return -1;
    std::ifstream input(fs::path(local)/L"VRSS Tuner"/L"last-source.txt",std::ios::binary);
    std::string source;std::getline(input,source);
    return source.size()==1 && source[0]>='0' && source[0]<='7'?source[0]-'0':-1;
}

int wmain(int argc,wchar_t** argv) {
    if(argc==2 && wcscmp(argv[1],L"--install")==0)return Configure(true);
    if(argc==2 && wcscmp(argv[1],L"--remove")==0)return Configure(false);
    if(argc>1)return 2;
    const auto bundle=Bundle();
    if(bundle.empty())return 3;
    const auto tuner=bundle/L"VRSS Tuner.exe";
    if(!fs::is_regular_file(tuner))return 5;
    if(!Running(L"VRSS Tuner.exe") &&
       reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",tuner.c_str(),nullptr,bundle.c_str(),SW_SHOWNORMAL))<=32)
        return 6;
    const int source=LastSource();
    if(source<=0 || Running(L"common_vrss_provider.exe") || Running(L"vrss_gaze_provider.exe"))return 0;
    static const wchar_t* routes[]={L"",L"Baballonia",L"PSVR2Toolkit",L"SRanibro",
        L"ViveProEye",L"ViveStreaming",L"SteamVREye",L"Varjo"};
    const auto launcher=bundle/L"Start-Provider-Only.cmd";
    if(!fs::is_regular_file(launcher))return 7;
    const std::wstring arguments=std::wstring(routes[source])+L" wait";
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",launcher.c_str(),arguments.c_str(),bundle.c_str(),SW_SHOWNORMAL))>32?0:8;
}
