#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"wintrust.lib")
#pragma comment(lib,"crypt32.lib")

namespace frame {
inline std::optional<std::wstring> ReadProcessEnvironment(const wchar_t* name) {
    SetLastError(ERROR_SUCCESS);
    const DWORD size=GetEnvironmentVariableW(name,nullptr,0);
    if(!size) {
        const auto error=GetLastError();
        if(error==ERROR_ENVVAR_NOT_FOUND)return {};
        if(error==ERROR_SUCCESS)return std::wstring{};
        throw std::runtime_error("Cannot read process environment");
    }
    std::wstring result(size,L'\0');
    const DWORD written=GetEnvironmentVariableW(name,result.data(),size);
    if(!written || written>=size)throw std::runtime_error("Process environment changed during read");
    result.resize(written);return result;
}
class ScopedProcessEnvironment {
    std::vector<std::pair<std::wstring,std::optional<std::wstring>>> originals_;
public:
    explicit ScopedProcessEnvironment(const std::vector<std::pair<std::wstring,std::wstring>>& values) {
        try {
            for(const auto& entry:values) {
                if(entry.first.empty() || entry.first.find(L'=')!=std::wstring::npos)
                    throw std::runtime_error("Invalid environment name");
                originals_.emplace_back(entry.first,ReadProcessEnvironment(entry.first.c_str()));
                if(!SetEnvironmentVariableW(entry.first.c_str(),entry.second.c_str()))
                    throw std::runtime_error("Cannot set launch-only environment");
            }
        } catch(...) {Restore();throw;}
    }
    ScopedProcessEnvironment(const ScopedProcessEnvironment&)=delete;
    ScopedProcessEnvironment& operator=(const ScopedProcessEnvironment&)=delete;
    ~ScopedProcessEnvironment() {Restore();}
    bool Restore() noexcept {
        bool ok=true;
        for(auto entry=originals_.rbegin();entry!=originals_.rend();++entry)
            if(!SetEnvironmentVariableW(entry->first.c_str(),entry->second?entry->second->c_str():nullptr))ok=false;
        if(ok)originals_.clear();
        return ok;
    }
};
inline bool LaunchStatusReady(DWORD expectedPid,DWORD alivePid,const std::string& status) {
    return expectedPid!=0 && expectedPid==alivePid &&
        status.find("\"pid\":"+std::to_string(expectedPid)+",")!=std::string::npos &&
        status.find("\"registered\":true")!=std::string::npos &&
        status.find("\"identityMode\":\"frame-experimental\"")!=std::string::npos &&
        (status.find("\"queryStatus\":0,")!=std::string::npos || status.find("\"queryStatus\":0}")!=std::string::npos);
}
inline bool IsTrustedEacLauncher(const std::filesystem::path& path) {
    WINTRUST_FILE_INFO file{sizeof(file)};file.pcwszFilePath=path.c_str();
    WINTRUST_DATA data{sizeof(data)};
    data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;
    data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;
    data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result=WinVerifyTrust(nullptr,&policy,&data);
    bool trusted=false;
    if(result==ERROR_SUCCESS) {
        auto* provider=WTHelperProvDataFromStateData(data.hWVTStateData);
        auto* signer=provider?WTHelperGetProvSignerFromChain(provider,0,FALSE,0):nullptr;
        if(signer && signer->csCertChain) {
            wchar_t organization[256]{};
            CertGetNameStringW(signer->pasCertChain[0].pCert,CERT_NAME_ATTR_TYPE,0,
                const_cast<char*>(szOID_ORGANIZATION_NAME),organization,256);
            trusted=wcscmp(organization,L"EasyAntiCheat Oy")==0;
        }
    }
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&data);
    return trusted;
}
inline std::filesystem::path ProtectedVrchatLauncher() {
    const wchar_t* key=L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 438100";
    wchar_t directory[32768]{};
    for(DWORD view:{RRF_SUBKEY_WOW6464KEY,RRF_SUBKEY_WOW6432KEY}) {
        DWORD size=sizeof(directory);
        if(RegGetValueW(HKEY_LOCAL_MACHINE,key,L"InstallLocation",RRF_RT_REG_SZ|view,nullptr,directory,&size)==ERROR_SUCCESS) {
            const std::filesystem::path root(directory);
            const auto launcher=root/L"start_protected_game.exe";
            if(!root.is_absolute() || !std::filesystem::is_regular_file(root/L"VRChat.exe") ||
               !std::filesystem::is_regular_file(launcher) || !IsTrustedEacLauncher(launcher))
                throw std::runtime_error("The installed VRChat EAC launcher could not be verified");
            return launcher;
        }
    }
    throw std::runtime_error("Steam App 438100 installation is not registered in Windows");
}
inline bool CurrentProcessElevated() {
    HANDLE token{};
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))throw std::runtime_error("Cannot verify launch privileges");
    TOKEN_ELEVATION elevation{};DWORD size{};
    const BOOL ok=GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size);
    CloseHandle(token);
    if(!ok)throw std::runtime_error("Cannot verify launch privileges");
    return elevation.TokenIsElevated!=0;
}
struct GameLaunchResult { DWORD pid{};bool environmentRestored{}; };
inline GameLaunchResult StartFrameVrchat(HWND owner) {
    if(CurrentProcessElevated())throw std::runtime_error("Start the Tuner UI without administrator privileges before launching VRChat");
    const auto launcher=ProtectedVrchatLauncher();
    const auto directory=launcher.parent_path().wstring();
    ScopedProcessEnvironment environment({{L"TEST_VENDOR_ID",L"0xffff"},{L"TEST_PRODUCT_ID",L"0xffff"},
        {L"SteamAppId",L"438100"},{L"SteamGameId",L"438100"}});
    SHELLEXECUTEINFOW info{sizeof(info)};
    info.hwnd=owner;info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;
    info.lpVerb=L"open";info.lpFile=launcher.c_str();info.lpDirectory=directory.c_str();info.nShow=SW_SHOWNORMAL;
    const BOOL started=ShellExecuteExW(&info);
    const DWORD error=started?ERROR_SUCCESS:GetLastError();
    const DWORD pid=info.hProcess?GetProcessId(info.hProcess):0;
    if(info.hProcess)CloseHandle(info.hProcess);
    const bool restored=environment.Restore();
    if(!started)throw std::runtime_error("EAC launcher did not start; Windows error="+std::to_string(error)+
        (restored?"":"; process environment restoration failed"));
    return {pid,restored};
}
}
