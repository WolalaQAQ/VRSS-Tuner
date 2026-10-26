#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <string>
#include <stdexcept>

namespace frame {
class Handle {
    HANDLE handle_{};
public:
    explicit Handle(HANDLE handle=nullptr):handle_(handle) {}
    ~Handle() { if(handle_)CloseHandle(handle_); }
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    Handle(Handle&& other) noexcept:handle_(other.handle_) { other.handle_=nullptr; }
    HANDLE get() const { return handle_; }
};
inline uint64_t ProcessCreated(HANDLE process) {
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return 0;
    return (uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
}
inline Handle OpenOwner(DWORD pid,uint64_t created) {
    Handle process(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    if(!process.get() || !created || ProcessCreated(process.get())!=created ||
       WaitForSingleObject(process.get(),0)!=WAIT_TIMEOUT)return Handle{};
    wchar_t ownerPath[32768]{},ownPath[32768]{};DWORD size=32768;
    if(!QueryFullProcessImageNameW(process.get(),0,ownerPath,&size) ||
       !GetModuleFileNameW(nullptr,ownPath,32768) || _wcsicmp(ownerPath,ownPath)!=0)return Handle{};
    return process;
}
inline bool ValidRunId(const std::wstring& id) {
    if(id.empty() || id.size()>64)return false;
    for(wchar_t c:id)if((c<L'0'||c>L'9') && c!=L'-')return false;
    return id.front()!=L'-' && id.back()!=L'-';
}
inline void RequirePlainDirectory(const std::filesystem::path& path) {
    for(auto current=path;!current.empty();current=current.parent_path()) {
        const DWORD attributes=GetFileAttributesW(current.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) ||
           (attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Unsafe or missing Frame run directory");
        if(current==current.root_path())break;
    }
}
inline constexpr wchar_t kWorkerMutex[]=L"Local\\VRSS-Tuner-FrameWorker-59602";
}
