#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdint>
#include <vector>
#include <cstring>
#include "GazeCalibrationFormat.h"
#pragma comment(lib, "advapi32.lib")

namespace live {
namespace fs = std::filesystem;
using Values = GazeCalibration::Values;
enum class Target { Pimax, Common };
inline const wchar_t* Name(Target t) { return t == Target::Pimax ? L"pimax" : L"common"; }
inline bool Valid(const Values& v) {
    return std::isfinite(v.left_deg) && std::isfinite(v.right_deg) &&
        std::isfinite(v.x_gain) && std::isfinite(v.y_gain) &&
        std::abs(v.left_deg) <= 5 && std::abs(v.right_deg) <= 5 &&
        v.x_gain >= .1f && v.x_gain <= 2 && std::abs(v.y_gain) >= .1f && std::abs(v.y_gain) <= 2;
}
inline std::string Serialize(const Values& v) {
    std::ostringstream s; s.imbue(std::locale::classic()); s << std::fixed << std::setprecision(6);
    s << "left_deg=" << v.left_deg << "\nright_deg=" << v.right_deg
      << "\nx_gain=" << v.x_gain << "\ny_gain=" << v.y_gain << '\n';
    return s.str();
}
inline bool ReadBytes(const fs::path& path, std::string& text, size_t limit = 256) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{}; DWORD read{};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && uint64_t(size.QuadPart) <= limit;
    if (ok) {
        text.resize(size_t(size.QuadPart));
        ok = ReadFile(file, text.data(), DWORD(text.size()), &read, nullptr) && read == text.size();
    }
    CloseHandle(file); return ok;
}
inline std::string Sha(const std::string& data) {
    HCRYPTPROV provider{}; HCRYPTHASH hash{}; BYTE bytes[32]{}; DWORD size = sizeof bytes;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return {};
    bool ok = CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) &&
        CryptHashData(hash, reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()), 0) &&
        CryptGetHashParam(hash, HP_HASHVAL, bytes, &size, 0);
    if (hash) CryptDestroyHash(hash); CryptReleaseContext(provider, 0);
    if (!ok) return {};
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (BYTE byte : bytes) out << std::setw(2) << unsigned(byte);
    return out.str();
}
inline std::wstring Wide(const std::string& text) { return {text.begin(), text.end()}; }
inline std::string Unique() {
    FILETIME now{}; GetSystemTimeAsFileTime(&now);
    return std::to_string((uint64_t(now.dwHighDateTime) << 32) | now.dwLowDateTime) + "-" + std::to_string(GetCurrentProcessId());
}
inline bool WriteNew(const fs::path& path, const std::string& data, DWORD& error) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_FLAG_WRITE_THROUGH, nullptr);
    if (f == INVALID_HANDLE_VALUE) { error = GetLastError(); return false; }
    DWORD written{}; bool ok = WriteFile(f, data.data(), DWORD(data.size()), &written, nullptr) && written == data.size() && FlushFileBuffers(f);
    error = ok ? ERROR_SUCCESS : GetLastError(); CloseHandle(f);
    if (!ok) DeleteFileW(path.c_str()); return ok;
}
inline bool AtomicWrite(const fs::path& path, const std::string& data, DWORD& error) {
    auto temp = path.parent_path() / (L".vrss-tuner-" + Wide(Unique()) + L".tmp");
    if (!WriteNew(temp, data, error)) return false;
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DWORD size{};
        GetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &size);
        std::vector<BYTE> security(size);
        if (!size || !GetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION,
            reinterpret_cast<PSECURITY_DESCRIPTOR>(security.data()), size, &size) ||
            !SetFileSecurityW(temp.c_str(), DACL_SECURITY_INFORMATION,
                reinterpret_cast<PSECURITY_DESCRIPTOR>(security.data()))) {
            error = GetLastError(); DeleteFileW(temp.c_str()); return false;
        }
    }
    HANDLE file = CreateFileW(temp.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = GetLastError(); DeleteFileW(temp.c_str()); return false; }
    auto name = fs::absolute(path).wstring();
    std::vector<BYTE> storage(sizeof(FILE_RENAME_INFO) + name.size() * sizeof(wchar_t));
    auto info = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
    // POSIX replacement keeps existing reader handles valid and lets new readers
    // open the complete new file without a delete/recreate gap (Windows 10+).
    info->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
    info->FileNameLength = DWORD(name.size() * sizeof(wchar_t));
    std::memcpy(info->FileName, name.data(), info->FileNameLength);
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (SetFileInformationByHandle(file, FileRenameInfoEx, info, DWORD(storage.size()))) { CloseHandle(file); return true; }
        error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED) break;
        Sleep(10);
    }
    CloseHandle(file); DeleteFileW(temp.c_str()); return false;
}
struct Snapshot { Values values{}; std::string bytes, hash; };
struct Result { int code{}; DWORD winError{}; bool changed{}; };
class Store {
public:
    fs::path pimax, common, backups;
    fs::path Path(Target t) const { return t == Target::Pimax ? pimax : common; }
    fs::path Last(Target t) const { return backups / (std::wstring(L"last-gaze-") + Name(t) + L".txt"); }
    bool Read(Target t, Snapshot& out) const {
        Snapshot s;
        if (!ReadBytes(Path(t), s.bytes) || !GazeCalibration::Parse(s.bytes, s.values) ||
            (t == Target::Common && s.bytes.find("x_gain=") == std::string::npos)) return false;
        s.hash = Sha(s.bytes); if (s.hash.empty()) return false;
        out = s; return true;
    }
    Result ApplyText(Target t, const std::string& expected, const std::string& bytes) const {
        Values validated{};
        if (!GazeCalibration::Parse(bytes, validated) || !Valid(validated) ||
            (t == Target::Common && bytes.find("x_gain=") == std::string::npos)) return {60};
        HANDLE mutex = CreateMutexW(nullptr, FALSE, (std::wstring(L"Local\\CodexVrssTunerGaze-") + Name(t)).c_str());
        if (!mutex) return {67, GetLastError()};
        DWORD wait = WaitForSingleObject(mutex, 0);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) { CloseHandle(mutex); return {67}; }
        struct Release { HANDLE h; ~Release() { ReleaseMutex(h); CloseHandle(h); } } release{mutex};
        Snapshot before;
        if (!Read(t, before)) return {65};
        if (before.hash != expected) return {61};
        if (before.bytes == bytes) return {};
        if (backups.empty()) return {62};
        std::error_code ec; fs::create_directories(backups, ec); if (ec) return {62, DWORD(ec.value())};
        DWORD error{};
        auto archive = backups / (std::wstring(L"gaze-") + Name(t) + L"-" + Wide(Unique()) + L".txt");
        if (!WriteNew(archive, before.bytes, error)) return {62, error};
        // Recheck immediately before replacement; never overwrite a stale UI read.
        Snapshot current; if (!Read(t, current) || current.hash != expected) return {61};
        if (!AtomicWrite(Path(t), bytes, error)) return {63, error};
        Snapshot after; if (!Read(t, after) || after.bytes != bytes) return {64, 0, true};
        if (!AtomicWrite(Last(t), before.bytes, error)) return {62, error, true};
        return {0, 0, true};
    }
    Result Apply(Target t, const std::string& expected, const Values& values) const {
        if (!Valid(values)) return {60};
        return ApplyText(t, expected, Serialize(values));
    }
    Result Restore(Target t, const std::string& expected) const {
        std::string old; if (!ReadBytes(Last(t), old)) return {66};
        return ApplyText(t, expected, old);
    }
};
} // namespace live
