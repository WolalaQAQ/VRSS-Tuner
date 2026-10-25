#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include "nvapi.h"
#include "GazeSettings.h"
#include "RegionEstimate.h"
#include "BaballoniaRelay.h"
#include "Localization.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "nvapi64.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {
constexpr NvU32 kHmdGate = 0x00524252u;
constexpr NvU32 kVrLayout = 0x00524253u;
constexpr NvU32 kArea = 0x00524254u;
constexpr NvU32 kAspect = 0x00524255u;
constexpr NvU32 kAllow = 0x00D5E9C6u;
constexpr NvU32 kMode = 0x00D5E9C7u;
constexpr std::array<NvU32, 4> kEditable{kMode, kArea, kAspect, kVrLayout};

using QueryInterface = void* (__cdecl*)(NvU32);
using DrsSetEx = NvAPI_Status (__cdecl*)(NvDRSSessionHandle, NvDRSProfileHandle, NVDRS_SETTING*, NvU32, NvU32);
using DrsGetEx = NvAPI_Status (__cdecl*)(NvDRSSessionHandle, NvDRSProfileHandle, NvU32, NVDRS_SETTING*, NvU32*);
using DrsDeleteEx = NvAPI_Status (__cdecl*)(NvDRSSessionHandle, NvDRSProfileHandle, NvU32);

struct Hidden {
    DrsSetEx set{};
    DrsGetEx get{};
    DrsDeleteEx remove{};
};

struct Value {
    bool found{};
    bool custom{};
    NvU32 raw{};
};

struct ProfileValues {
    Value mode, area, aspect, layout, gate, allow;
};

struct Session {
    NvDRSSessionHandle session{};
    NvDRSProfileHandle profile{};
    Hidden hidden{};
    bool initialized{};

    ~Session() {
        if (session) NvAPI_DRS_DestroySession(session);
        if (initialized) NvAPI_Unload();
    }
    NvAPI_Status open() {
        NvAPI_Status status = NvAPI_Initialize();
        if (status != NVAPI_OK) return status;
        initialized = true;
        auto* module = GetModuleHandleW(L"nvapi64.dll");
        auto query = module ? reinterpret_cast<QueryInterface>(GetProcAddress(module, "nvapi_QueryInterface")) : nullptr;
        if (!query) return NVAPI_NO_IMPLEMENTATION;
        hidden.set = reinterpret_cast<DrsSetEx>(query(0x8A2CF5F5u));
        if (!hidden.set) hidden.set = reinterpret_cast<DrsSetEx>(query(0x577DD202u));
        hidden.get = reinterpret_cast<DrsGetEx>(query(0xEA99498Du));
        if (!hidden.get) hidden.get = reinterpret_cast<DrsGetEx>(query(0x73BF8338u));
        hidden.remove = reinterpret_cast<DrsDeleteEx>(query(0xD20D29DFu));
        if (!hidden.remove) hidden.remove = reinterpret_cast<DrsDeleteEx>(query(0xE4A26362u));
        if (!hidden.get) return NVAPI_NO_IMPLEMENTATION;
        status = NvAPI_DRS_CreateSession(&session);
        if (status != NVAPI_OK) return status;
        status = NvAPI_DRS_LoadSettings(session);
        if (status != NVAPI_OK) return status;
        NvAPI_UnicodeString name{};
        constexpr wchar_t profileName[] = L"VRChat";
        for (size_t i = 0; i < std::size(profileName); ++i) name[i] = static_cast<NvU16>(profileName[i]);
        status = NvAPI_DRS_FindProfileByName(session, name, &profile);
        if (status == NVAPI_OK) return status;
        // A user-created VRChat.exe profile can shadow the NVIDIA VRChat profile.
        // Use an application lookup only when it has the VRSS allow gate enabled.
        NvAPI_UnicodeString applicationName{};
        constexpr wchar_t executable[] = L"VRChat.exe";
        for (size_t i = 0; i < std::size(executable); ++i)
            applicationName[i] = static_cast<NvU16>(executable[i]);
        NVDRS_APPLICATION application{};
        application.version = NVDRS_APPLICATION_VER;
        NvDRSProfileHandle candidate{};
        if (NvAPI_DRS_FindApplicationByName(session, applicationName, &candidate, &application) != NVAPI_OK)
            return status;
        NVDRS_SETTING allow{};
        allow.version = NVDRS_SETTING_VER;
        NvU32 extra{};
        if (hidden.get(session, candidate, kAllow, &allow, &extra) != NVAPI_OK ||
            allow.u32CurrentValue != 1) return status;
        profile = candidate;
        return NVAPI_OK;
    }
    NvAPI_Status read(NvU32 id, Value& value) const {
        NVDRS_SETTING setting{};
        setting.version = NVDRS_SETTING_VER;
        NvU32 extra{};
        NvAPI_Status status = hidden.get(session, profile, id, &setting, &extra);
        if (status == NVAPI_SETTING_NOT_FOUND) {
            value = {};
            return NVAPI_OK;
        }
        if (status == NVAPI_OK) {
            value = {true, setting.isCurrentPredefined == 0, setting.u32CurrentValue};
        }
        return status;
    }
    NvAPI_Status readAll(ProfileValues& values) const {
        for (auto [id, destination] : std::array<std::pair<NvU32, Value*>, 6>{
                 std::pair{kMode, &values.mode}, {kArea, &values.area},
                 {kAspect, &values.aspect}, {kVrLayout, &values.layout},
                 {kHmdGate, &values.gate}, {kAllow, &values.allow}}) {
            NvAPI_Status status = read(id, *destination);
            if (status != NVAPI_OK) return status;
        }
        return NVAPI_OK;
    }
    NvAPI_Status write(NvU32 id, NvU32 raw) const {
        if (!hidden.set) return NVAPI_NO_IMPLEMENTATION;
        NVDRS_SETTING setting{};
        setting.version = NVDRS_SETTING_VER;
        setting.settingId = id;
        setting.settingType = NVDRS_DWORD_TYPE;
        setting.u32CurrentValue = raw;
        return hidden.set(session, profile, &setting, 0, 0);
    }
    NvAPI_Status erase(NvU32 id) const {
        if (!hidden.remove) return NVAPI_NO_IMPLEMENTATION;
        NvAPI_Status status = hidden.remove(session, profile, id);
        return status == NVAPI_SETTING_NOT_FOUND ? NVAPI_OK : status;
    }
};

NvU32 floatBits(float value) {
    NvU32 bits{};
    static_assert(sizeof bits == sizeof value);
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}
float bitsFloat(NvU32 bits) {
    float value{};
    std::memcpy(&value, &bits, sizeof value);
    return value;
}
bool parseNumber(const std::wstring& input, float& output) {
    std::wstring normalized = input;
    for (auto& ch : normalized) if (ch == L',') ch = L'.';
    std::wistringstream stream(normalized);
    stream.imbue(std::locale::classic());
    stream >> output;
    if (stream.fail()) return false;
    stream >> std::ws;
    return stream.eof() && std::isfinite(output);
}
std::wstring numberText(float value) {
    std::wostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(8) << value;
    return out.str();
}
std::wstring gazeNumberText(float value) {
    std::wostringstream out; out.imbue(std::locale::classic()); out << std::fixed << std::setprecision(6) << value;
    auto text = out.str();
    while (!text.empty() && text.back() == L'0') text.pop_back();
    if (!text.empty() && text.back() == L'.') text.pop_back();
    return text == L"-0" ? L"0" : text;
}
std::filesystem::path backupFolder() {
    wchar_t buffer[32768]{};
    DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(std::size(buffer)));
    if (!size || size >= std::size(buffer)) return {};
    return std::filesystem::path(buffer) / L"VRSS Tuner";
}
std::filesystem::path languageFile() { return backupFolder() / L"language.txt"; }
void loadLanguage() {
    std::ifstream in(languageFile(), std::ios::binary);
    std::string value;
    if (in >> value && (value == "ja" || value == "en")) language::english = value == "en";
    else language::english = PRIMARYLANGID(GetUserDefaultUILanguage()) != LANG_JAPANESE;
}
void saveLanguage() {
    std::error_code error;
    std::filesystem::create_directories(backupFolder(), error);
    if (!error) {
        std::ofstream out(languageFile(), std::ios::binary | std::ios::trunc);
        out << (language::english ? "en\n" : "ja\n");
    }
}
std::filesystem::path lastBackup() {
    auto folder = backupFolder();
    return folder.empty() ? std::filesystem::path{} : folder / L"last-backup.txt";
}

bool saveBackup(const ProfileValues& values) {
    auto folder = backupFolder();
    if (folder.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error) return false;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::wostringstream name;
    name << L"backup-" << std::setfill(L'0') << std::setw(4) << now.wYear
         << std::setw(2) << now.wMonth << std::setw(2) << now.wDay << L'-'
         << std::setw(2) << now.wHour << std::setw(2) << now.wMinute
         << std::setw(2) << now.wSecond << L'-' << std::setw(3) << now.wMilliseconds
         << L'-' << GetCurrentProcessId() << L".txt";
    std::ostringstream content;
    content << "VRSS-TUNER-BACKUP-1\n";
    for (auto [id, value] : std::array<std::pair<NvU32, const Value*>, 4>{
             std::pair{kMode, &values.mode}, {kArea, &values.area},
             {kAspect, &values.aspect}, {kVrLayout, &values.layout}}) {
        content << id << ' ' << (value->custom ? 1 : 0) << ' ' << value->raw << '\n';
    }
    auto writeFile = [&](const std::filesystem::path& path) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << content.str();
        out.flush();
        return out.good();
    };
    if (!writeFile(folder / name.str())) return false;
    auto pending = folder / L"last-backup.pending";
    if (!writeFile(pending)) return false;
    return MoveFileExW(pending.c_str(), lastBackup().c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
bool loadBackup(std::array<Value, 4>& values) {
    auto path = lastBackup();
    if (path.empty()) return false;
    std::ifstream in(path, std::ios::binary);
    std::string magic;
    if (!in || !std::getline(in, magic) || magic != "VRSS-TUNER-BACKUP-1") return false;
    for (size_t i = 0; i < kEditable.size(); ++i) {
        unsigned long long id{}, custom{}, raw{};
        if (!(in >> id >> custom >> raw) || id != kEditable[i] || custom > 1 || raw > 0xffffffffULL) return false;
        values[i] = {true, custom == 1, static_cast<NvU32>(raw)};
    }
    std::string extra;
    if (in >> extra) return false;
    if (values[0].custom && values[0].raw > 3) return false;
    if (values[1].custom && (!std::isfinite(bitsFloat(values[1].raw)) || bitsFloat(values[1].raw) < 1 || bitsFloat(values[1].raw) > 100)) return false;
    if (values[2].custom && (!std::isfinite(bitsFloat(values[2].raw)) || bitsFloat(values[2].raw) < 0.5f || bitsFloat(values[2].raw) > 2.0f)) return false;
    if (values[3].custom && values[3].raw > 7) return false;
    return true;
}

int applyCommand(int argc, wchar_t** argv) {
    if (argc != 6) return 10;
    unsigned mode{}, flags{};
    float area{}, aspect{};
    try {
        size_t end{};
        mode = static_cast<unsigned>(std::stoul(argv[2], &end));
        if (end != wcslen(argv[2])) return 11;
        flags = static_cast<unsigned>(std::stoul(argv[5], &end));
        if (end != wcslen(argv[5])) return 11;
    } catch (...) { return 11; }
    if (mode > 3 || flags > 5 || (flags & ~5u) || !parseNumber(argv[3], area) || area < 1 || area > 100) return 11;
    bool defaultAspect = wcscmp(argv[4], L"default") == 0;
    if (!defaultAspect && (!parseNumber(argv[4], aspect) || aspect < 0.5f || aspect > 2.0f)) return 11;
    Session session;
    NvAPI_Status status = session.open();
    if (status != NVAPI_OK) return 12;
    ProfileValues original{};
    status = session.readAll(original);
    if (status != NVAPI_OK) return 13;
    if (!saveBackup(original)) return 14;
    NvU32 originalFlags = original.layout.found ? original.layout.raw : 4u;
    NvU32 newFlags = (originalFlags & ~5u) | flags;
    status = session.write(kMode, mode);
    if (status != NVAPI_OK) return 15;
    status = session.write(kArea, floatBits(area));
    if (status != NVAPI_OK) return 16;
    status = defaultAspect ? session.erase(kAspect) : session.write(kAspect, floatBits(aspect));
    if (status != NVAPI_OK) return 17;
    status = session.write(kVrLayout, newFlags);
    if (status != NVAPI_OK) return 18;
    return NvAPI_DRS_SaveSettings(session.session) == NVAPI_OK ? 0 : 19;
}
int restoreCommand() {
    std::array<Value, 4> saved{};
    if (!loadBackup(saved)) return 20;
    Session session;
    if (session.open() != NVAPI_OK) return 21;
    for (size_t i = 0; i < saved.size(); ++i) {
        NvAPI_Status status = saved[i].custom
            ? session.write(kEditable[i], saved[i].raw)
            : session.erase(kEditable[i]);
        if (status != NVAPI_OK) return 22 + static_cast<int>(i);
    }
    return NvAPI_DRS_SaveSettings(session.session) == NVAPI_OK ? 0 : 26;
}

std::filesystem::path gResearch;
live::Store gGazeStore;
void initializeGazePaths() {
    wchar_t module[32768]{}; GetModuleFileNameW(nullptr, module, DWORD(std::size(module)));
    auto executableFolder = std::filesystem::path(module).parent_path();
    std::error_code localError;
    if (std::filesystem::is_directory(executableFolder / L"common-vrss-provider", localError))
        gResearch = executableFolder;
    auto path = executableFolder;
    for (int i = 0; gResearch.empty() && i < 5 && !path.empty(); ++i, path = path.parent_path()) {
        if (_wcsicmp(path.filename().c_str(), L"vrss-tuner") == 0) { gResearch = path.parent_path(); break; }
    }
    if (gResearch.empty()) {
        for (const auto& candidate : {executableFolder, executableFolder.parent_path()}) {
            std::error_code error;
            if (std::filesystem::is_directory(candidate / L"common-vrss-provider", error)) {
                gResearch = candidate;
                break;
            }
        }
    }
    gGazeStore.pimax = L"C:\\ProgramData\\CodexVRSS\\gaze-calibration.txt";
    gGazeStore.common = gResearch / L"common-vrss-provider" / L"calibration.txt";
    auto backup = backupFolder();
    if (!backup.empty()) gGazeStore.backups = backup / L"gaze-backups";
}
bool compatiblePimaxDriver() {
    std::string manifest, dll;
    if (!live::ReadBytes(gResearch / L"pvr-sranibro-eye-driver/gain-build-manifest.json", manifest, 65536) ||
        !live::ReadBytes(L"C:\\Program Files\\Pimax\\Runtime\\drivers\\sranibro_eye\\bin\\win64\\driver_sranibro_eye.dll", dll, 16 * 1024 * 1024)) return false;
    auto hash = live::Sha(dll); std::string normalized = manifest;
    for (auto& ch : normalized) if (ch >= 'A' && ch <= 'F') ch += 'a' - 'A';
    return hash.size() == 64 && normalized.find(hash) != std::string::npos;
}
bool saveStartupState() {
    Session session; ProfileValues values{};
    if (session.open() != NVAPI_OK || session.readAll(values) != NVAPI_OK) return false;
    live::Snapshot gaze;
    const bool legacyLayout = !gResearch.empty() &&
        std::filesystem::is_directory(gResearch / L"vrss-tuner") &&
        gGazeStore.Read(live::Target::Pimax, gaze);
    if (!legacyLayout) {
        auto folder = backupFolder();
        if (folder.empty()) return false;
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error) return false;
        const bool common = gGazeStore.Read(live::Target::Common, gaze);
        std::ostringstream state;
        state << "format=2\nmode=" << (values.mode.found ? values.mode.raw : 0)
              << "\narea=" << (values.area.found ? values.area.raw : floatBits(50))
              << "\nlayout=" << (values.layout.found ? values.layout.raw : 0)
              << "\ncalibration_target=" << (common ? "common" : "none")
              << "\ncalibration_sha256=" << (common ? gaze.hash : "none") << '\n';
        DWORD winError{};
        return live::AtomicWrite(folder / L"startup-state-portable.ini", state.str(), winError);
    }
    auto root = gResearch / L"vrss-tuner";
    auto snapshot = root / (L"startup-pimax-calibration-" + live::Wide(gaze.hash) + L".txt");
    std::string retained; DWORD error{};
    if (std::filesystem::exists(snapshot)) {
        if (!live::ReadBytes(snapshot, retained) || retained != gaze.bytes) return false;
    } else if (!live::WriteNew(snapshot, gaze.bytes, error)) return false;
    std::ostringstream state;
    state << "format=1\nmode=" << (values.mode.found ? values.mode.raw : 0)
          << "\narea=" << (values.area.found ? values.area.raw : floatBits(50))
          << "\nlayout=" << (values.layout.found ? values.layout.raw : 0)
          << "\ncalibration_sha256=" << gaze.hash << '\n';
    return live::AtomicWrite(root / L"startup-state.ini", state.str(), error);
}
live::Target parseTarget(const wchar_t* text) {
    if (wcscmp(text, L"pimax") == 0) return live::Target::Pimax;
    if (wcscmp(text, L"common") == 0) return live::Target::Common;
    throw std::runtime_error("Unknown gaze target");
}
int gazeCommand(int argc, wchar_t** argv, bool restore) {
    if (argc != (restore ? 4 : 8)) return 60;
    try {
        auto target = parseTarget(argv[2]);
        if (target == live::Target::Pimax && !compatiblePimaxDriver()) return 68;
        std::wstring hash = argv[restore ? 3 : 7];
        if (hash.size() != 64 || hash.find_first_not_of(L"0123456789abcdef") != std::wstring::npos) return 60;
        std::string expected; for (wchar_t ch : hash) expected.push_back(static_cast<char>(ch));
        live::Result result;
        if (restore) result = gGazeStore.Restore(target, expected);
        else {
            live::Values values;
            if (!parseNumber(argv[3], values.x_gain) || !parseNumber(argv[4], values.y_gain) ||
                !parseNumber(argv[5], values.left_deg) || !parseNumber(argv[6], values.right_deg)) return 60;
            result = gGazeStore.Apply(target, expected, values);
        }
        if (result.code) return result.code;
        return saveStartupState() ? 0 : 69;
    } catch (...) { return 60; }
}

enum ControlId {
    kRefreshButton = 100, kApplyButton, kRestoreButton,
    kModeCombo, kAreaEdit, kAspectCheck, kAspectEdit, kLayoutCombo, kInvertCheck, kStatusEdit,
    kDriverTab, kGazeTab, kGazeTarget, kGazeX, kGazeY, kGazeLeft, kGazeRight, kGazeInvert,
    kGazeRefresh, kGazeApply, kGazeRestore, kGazeReset,
    kAreaSlider, kEstimateMsaa, kProfileName, kProfileSave, kProfileSwitch,
    kInputTab, kInputPort, kInputForward, kInputStart, kInputStop, kInputStatus,
    kLanguage
};
struct Ui {
    HWND window{}, mode{}, area{}, aspectCheck{}, aspect{}, layout{}, invert{}, status{}, restore{};
    ProfileValues loaded{};
    bool loadedOk{};
    HWND driverTab{}, gazeTab{}, gazeTarget{}, gazeX{}, gazeY{}, gazeLeft{}, gazeRight{}, gazeInvert{}, gazeStatus{}, gazeApply{}, gazeRestore{};
    HWND profileName{}, profileSave{}, profileSwitch{}, inputTab{}, inputPort{}, inputForward{}, inputStart{}, inputStop{}, inputStatus{}, language{};
    live::Target target{live::Target::Pimax};
    live::Snapshot gazeLoaded{};
    bool gazeLoadedOk{};
    int panel{};
    std::vector<HWND> driverControls, gazeControls, inputControls, restartBadges;
    HWND areaSlider{}, estimateMsaa{}, regionPreview{}, regionArea{}, regionCost{}, regionNote{}, installInfo{};
    region::Estimate estimate;
};
Ui gUi;
HFONT gFont{};
HFONT gBoldFont{};
bool gBusy{};
int gCreatingPanel = -1;
std::vector<std::pair<HWND, std::wstring>> gLabels;
std::unordered_map<HWND, std::wstring> gOriginalText;

HWND control(const wchar_t* cls, const wchar_t* title, DWORD style, int x, int y, int w, int h, int id = 0) {
    std::wstring translated = language::Text(title);
    HWND handle = CreateWindowExW(0, cls, translated.c_str(), style | WS_CHILD | WS_VISIBLE,
                                 x, y, w, h, gUi.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetModuleHandleW(nullptr), nullptr);
    SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(gFont), TRUE);
    if (gCreatingPanel == 0) gUi.driverControls.push_back(handle);
    if (gCreatingPanel == 1) gUi.gazeControls.push_back(handle);
    if (gCreatingPanel == 2) gUi.inputControls.push_back(handle);
    if (*title && (_wcsicmp(cls, L"STATIC") == 0 || _wcsicmp(cls, L"BUTTON") == 0))
        gLabels.emplace_back(handle, title);
    return handle;
}
void setText(HWND target, const std::wstring& value) {
    gOriginalText[target] = value;
    auto translated = language::Text(value);
    SetWindowTextW(target, translated.c_str());
}
int localizedMessageBox(HWND owner, const wchar_t* message, const wchar_t* title, UINT flags) {
    const auto body = language::Text(message ? message : L"");
    const auto caption = language::Text(title ? title : L"");
    return ::MessageBoxW(owner, body.c_str(), caption.c_str(), flags);
}
#define MessageBoxW localizedMessageBox
std::wstring getText(HWND target) {
    wchar_t buffer[4096]{};
    GetWindowTextW(target, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}
void applyLanguage() {
    for (const auto& [handle, source] : gLabels) {
        const auto translated = language::Text(source);
        SetWindowTextW(handle, translated.c_str());
    }
    auto repopulate = [](HWND combo, const wchar_t* first, const wchar_t* second) {
        int selection = int(SendMessageW(combo, CB_GETCURSEL, 0, 0));
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        const auto a = language::Text(first), b = language::Text(second);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(a.c_str()));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(b.c_str()));
        SendMessageW(combo, CB_SETCURSEL, selection, 0);
    };
    repopulate(gUi.mode, L"オフ", L"オン");
    repopulate(gUi.gazeTarget, L"Pimax経路", L"共通プロバイダー");
    for (HWND handle : {gUi.status, gUi.gazeStatus, gUi.inputStatus,
                        gUi.regionArea, gUi.regionCost, gUi.regionNote, gUi.installInfo}) {
        auto found = gOriginalText.find(handle);
        if (found != gOriginalText.end()) {
            const auto translated = language::Text(found->second);
            SetWindowTextW(handle, translated.c_str());
        }
    }
    InvalidateRect(gUi.regionPreview, nullptr, TRUE);
}
bool isChecked(HWND target) { return SendMessageW(target, BM_GETCHECK, 0, 0) == BST_CHECKED; }
void check(HWND target, bool on) { SendMessageW(target, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }
std::wstring decimal(double value, int digits=1) {
    std::wostringstream out; out.imbue(std::locale::classic()); out << std::fixed << std::setprecision(digits) << value; return out.str();
}
void updateEstimate() {
    if (!gUi.regionPreview || !gUi.estimateMsaa) return;
    float area{}, aspect=1;
    int index = int(SendMessageW(gUi.estimateMsaa, CB_GETCURSEL, 0, 0));
    int samples = index == 0 ? 2 : index == 2 ? 8 : 4;
    int mode = int(SendMessageW(gUi.mode, CB_GETCURSEL, 0, 0));
    int layout = 0;
    bool valid = parseNumber(getText(gUi.area), area) &&
        (!isChecked(gUi.aspectCheck) || parseNumber(getText(gUi.aspect), aspect));
    gUi.estimate = valid ? region::Calculate(area,aspect,samples,mode,layout) : region::Estimate{};
    auto& e = gUi.estimate;
    if (!e.valid) {
        setText(gUi.regionArea,L"領域・縦横比の入力値を確認してください。");
        setText(gUi.regionCost,L"色計算の増加：—");
        setText(gUi.regionNote,L"領域1～100、縦横比0.5～2で試算できます。");
    } else if (!e.supported) {
        setText(gUi.regionArea,L"この実験用モードは試算対象外です。");
        setText(gUi.regionCost,L"色計算の増加：—");
        setText(gUi.regionNote,L"2領域・Force/Adaptive/Offでプレビューできます。");
    } else {
        SendMessageW(gUi.areaSlider,TBM_SETPOS,TRUE,LPARAM(std::lround(area)));
        setText(gUi.regionArea,(e.active ? L"追加SSの面積：約" : L"設定領域の面積：約") + decimal(e.visibleFraction*100) + L"% / 各目");
        setText(gUi.regionCost,L"色計算：" + decimal(e.shadingRatio,2) + L"倍（＋" + decimal((e.shadingRatio-1)*100) + L"%）");
        std::wstring note = L"横幅 " + decimal(e.radiusX*200) + L"% × 高さ " + decimal(e.radiusY*200) + L"%";
        if (!e.active) note += L"\r\nVRSS Off：追加SSはありません。";
        else if (e.adaptive) note += L"\r\n入力サイズでの仮定。Adaptiveの実領域は変動。";
        else note += L"\r\nMSAA単独比の色計算。GPU全体・FPSの比ではありません。";
        setText(gUi.regionNote,note);
    }
    InvalidateRect(gUi.regionPreview,nullptr,FALSE);
}

LRESULT CALLBACK regionPreviewProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window,&paint); RECT client{}; GetClientRect(window,&client);
        HDC buffer = CreateCompatibleDC(dc); HBITMAP bitmap = CreateCompatibleBitmap(dc,client.right,client.bottom);
        auto oldBitmap = SelectObject(buffer,bitmap);
        FillRect(buffer,&client,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetBkMode(buffer,TRANSPARENT); auto oldFont=SelectObject(buffer,gFont);
        SetTextColor(buffer,RGB(70,85,105));
        const auto& e = gUi.estimate;
        if (e.valid && e.supported) {
            int side=std::min(126,int(client.bottom)-25);
            for(int eye=0;eye<2;++eye) {
                int x=eye*(side+24),y=22;
                const auto label = language::Text(eye==0?L"左目":L"右目");
                TextOutW(buffer,x,0,label.c_str(),int(label.size()));
                RECT bounds{x,y,x+side,y+side};
                HBRUSH background=CreateSolidBrush(RGB(241,245,251));FillRect(buffer,&bounds,background);DeleteObject(background);
                int saved=SaveDC(buffer);IntersectClipRect(buffer,bounds.left,bounds.top,bounds.right,bounds.bottom);
                HBRUSH fill=CreateSolidBrush(e.active?RGB(192,219,253):RGB(233,236,240));
                HPEN pen=CreatePen(e.active?PS_SOLID:PS_DASH,1,e.active?RGB(36,107,205):RGB(125,136,151));
                auto oldBrush=SelectObject(buffer,fill);auto oldPen=SelectObject(buffer,pen);
                int cx=x+side/2,cy=y+side/2;
                int rx=int(std::lround(e.radiusX*side)),ry=int(std::lround(e.radiusY*side));
                Ellipse(buffer,cx-rx,cy-ry,cx+rx,cy+ry);
                MoveToEx(buffer,cx-4,cy,nullptr);LineTo(buffer,cx+5,cy);MoveToEx(buffer,cx,cy-4,nullptr);LineTo(buffer,cx,cy+5);
                SelectObject(buffer,oldBrush);SelectObject(buffer,oldPen);DeleteObject(fill);DeleteObject(pen);RestoreDC(buffer,saved);
                FrameRect(buffer,&bounds,static_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH)));
            }
        } else {
            RECT messageRect{4,35,client.right-4,client.bottom};
            const auto previewMessage = language::Text(e.valid?L"領域配置は未検証です。":L"入力値を確認してください。");
            DrawTextW(buffer,previewMessage.c_str(),-1,&messageRect,DT_WORDBREAK);
        }
        SelectObject(buffer,oldFont);BitBlt(dc,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);
        SelectObject(buffer,oldBitmap);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(window,&paint);return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
bool vrchatRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{sizeof(entry)};
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) do {
        if (_wcsicmp(entry.szExeFile, L"VRChat.exe") == 0) { found = true; break; }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return found;
}
unsigned processCount(const wchar_t* name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{sizeof(entry)}; unsigned count{};
    if (Process32FirstW(snapshot, &entry)) do { if (_wcsicmp(entry.szExeFile, name) == 0) ++count; } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot); return count;
}
void showPanel(int panel) {
    gUi.panel = panel;
    for (HWND item : gUi.driverControls) ShowWindow(item, panel == 0 ? SW_SHOW : SW_HIDE);
    for (HWND item : gUi.gazeControls) ShowWindow(item, panel == 1 ? SW_SHOW : SW_HIDE);
    for (HWND item : gUi.inputControls) ShowWindow(item, panel == 2 ? SW_SHOW : SW_HIDE);
    SendMessageW(gUi.driverTab, BM_SETCHECK, panel == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gUi.gazeTab, BM_SETCHECK, panel == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gUi.inputTab, BM_SETCHECK, panel == 2 ? BST_CHECKED : BST_UNCHECKED, 0);
    InvalidateRect(gUi.window, nullptr, TRUE);
}
void refreshGaze() {
    int target = int(SendMessageW(gUi.gazeTarget, CB_GETCURSEL, 0, 0));
    gUi.target = target == 1 ? live::Target::Common : live::Target::Pimax;
    gUi.gazeLoadedOk = gGazeStore.Read(gUi.target, gUi.gazeLoaded);
    if (gUi.gazeLoadedOk) {
        const auto& v = gUi.gazeLoaded.values;
        setText(gUi.gazeX, gazeNumberText(v.x_gain)); setText(gUi.gazeY, gazeNumberText(std::abs(v.y_gain)));
        setText(gUi.gazeLeft, gazeNumberText(v.left_deg)); setText(gUi.gazeRight, gazeNumberText(v.right_deg));
        check(gUi.gazeInvert, v.y_gain < 0);
    }
    std::wstring info = L"Pimax: " + std::to_wstring(processCount(L"vrss_gaze_provider.exe")) +
        L" / 共通: " + std::to_wstring(processCount(L"common_vrss_provider.exe")) + L" プロセス\r\n";
    info += gUi.gazeLoadedOk ? L"補正を読込済み。保存後は実行中の経路へ反映します。" :
        L"この経路の補正ファイルを読み取れません。";
    setText(gUi.gazeStatus, info);
    EnableWindow(gUi.gazeApply, gUi.gazeLoadedOk);
    EnableWindow(gUi.gazeRestore, gUi.gazeLoadedOk);
}
void refresh() {
    Session session;
    NvAPI_Status result = session.open();
    ProfileValues values{};
    if (result == NVAPI_OK) result = session.readAll(values);
    gUi.loadedOk = result == NVAPI_OK;
    if (gUi.installInfo) {
        std::wstring install;
        if (result != NVAPI_OK) install = L"導入状態: NVIDIAのVRChatプロファイルを読み取れません。";
        else if (!values.allow.found || values.allow.raw != 1)
            install = L"導入状態: このVRChatプロファイルではVRSS許可が有効ではありません。";
        else if (gResearch.empty()) install = L"導入状態: 実行ファイル単体です。視線補正と共通プロバイダーは未接続です。";
        else if (compatiblePimaxDriver()) install = processCount(L"vrss_gaze_provider.exe")
            ? L"導入状態: Pimax経路を検出。実機での追従は別途確認してください。"
            : L"導入状態: Pimax補正ドライバーを確認。視線プロバイダーは停止中です。";
        else if (std::filesystem::is_regular_file(gResearch / L"common-vrss-provider/common_vrss_provider.exe"))
            install = L"導入状態: 共通プロバイダーを検出。登録・起動・視線入力は別途必要です。";
        else install = L"導入状態: 画質設定のみ使用可能。視線経路は未準備です。";
        setText(gUi.installInfo, install);
    }
    if (!gUi.loadedOk) {
        setText(gUi.status, L"VRChatプロファイルを読み取れません。NVAPI状態: " + std::to_wstring(result));
        EnableWindow(GetDlgItem(gUi.window, kApplyButton), FALSE);
        return;
    }
    gUi.loaded = values;
    SendMessageW(gUi.mode, CB_SETCURSEL, values.mode.found && values.mode.raw != 0 ? 1 : 0, 0);
    setText(gUi.area, numberText(values.area.found ? bitsFloat(values.area.raw) : 50.0f));
    bool customAspect = values.aspect.custom;
    check(gUi.aspectCheck, customAspect);
    setText(gUi.aspect, numberText(values.aspect.found ? bitsFloat(values.aspect.raw) : 1.0f));
    EnableWindow(gUi.aspect, customAspect);
    std::wostringstream info;
    info << L"VRChat: " << (vrchatRunning() ? L"起動中。変更後は再起動が必要。" : L"停止中。次回起動から反映。") << L"\r\n"
         << L"保存値: " << (values.mode.found && values.mode.raw ? L"オン" : L"オフ")
         << L" / 大きさ " << numberText(values.area.found ? bitsFloat(values.area.raw) : 50.0f)
         << L" / 縦横比 " << numberText(values.aspect.found ? bitsFloat(values.aspect.raw) : 1.0f)
         << L"\r\n既存の目別処理: " << (values.layout.found && (values.layout.raw & 1u) ? L"選択目" : L"両目")
         << L" / ドライバー縦反転: " << (values.layout.found && (values.layout.raw & 4u) ? L"オン" : L"オフ")
         << L"（保持）";
    setText(gUi.status, info.str());
    EnableWindow(GetDlgItem(gUi.window, kApplyButton), TRUE);
    EnableWindow(gUi.restore, TRUE);
}

int runElevated(const std::wstring& arguments) {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)))) return -1;
    SHELLEXECUTEINFOW info{sizeof(info)};
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = path;
    info.lpParameters = arguments.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) return GetLastError() == ERROR_CANCELLED ? -2 : -3;
    DWORD code = 99;
    while (true) {
        DWORD wait = MsgWaitForMultipleObjects(1, &info.hProcess, FALSE, INFINITE, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) break;
        if (wait != WAIT_OBJECT_0 + 1) break;
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return static_cast<int>(code);
}
bool applyFromUi(bool confirm = true) {
    if (!gUi.loadedOk) return false;
    if (!gUi.loaded.allow.found || gUi.loaded.allow.raw != 1) {
        MessageBoxW(gUi.window, L"このVRChatプロファイルではVRSS許可が有効ではありません。設定を保存しても効果がないため変更しません。",
            L"VRSS設定", MB_ICONWARNING);
        return false;
    }
    int mode = static_cast<int>(SendMessageW(gUi.mode, CB_GETCURSEL, 0, 0));
    float area{}, aspect{};
    bool defaultAspect = !isChecked(gUi.aspectCheck);
    if (mode < 0 || mode > 1 ||
        !parseNumber(getText(gUi.area), area) || area < 1 || area > 100 ||
        (!defaultAspect && (!parseNumber(getText(gUi.aspect), aspect) || aspect < 0.5f || aspect > 2))) {
        MessageBoxW(gUi.window, L"領域は1～100、縦横比は0.5～2の数値を指定してください。", L"入力エラー", MB_ICONWARNING);
        return false;
    }
    // Preserve the existing driver layout bits. The UI does not expose these
    // experimental per-eye/Y-axis switches because they are not gaze calibration.
    unsigned flags = gUi.loaded.layout.found ? (gUi.loaded.layout.raw & 5u) : 0u;
    if (confirm && MessageBoxW(gUi.window,
        L"この設定は、保存後にVRChatを再起動して反映します。\n起動中のVRChatには、その場で反映される保証がありません。\n\n今のワールドからは移動せず、自動再起動もしません。\n設定を保存しますか？",
        L"VRChatの再起動が必要です", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return false;
    std::wstring args = L"--apply " + std::to_wstring(mode) + L" " + numberText(area) + L" " +
                        (defaultAspect ? L"default" : numberText(aspect)) + L" " + std::to_wstring(flags);
    EnableWindow(GetDlgItem(gUi.window, kApplyButton), FALSE);
    EnableWindow(gUi.restore, FALSE);
    gBusy = true;
    int result = runElevated(args);
    gBusy = false;
    if (result != -2) refresh();
    else {
        EnableWindow(GetDlgItem(gUi.window, kApplyButton), TRUE);
        EnableWindow(gUi.restore, TRUE);
    }
    if (result == 0 && confirm) MessageBoxW(gUi.window, L"設定を保存し、読戻しました。VRChatを通常の方法で再起動すると反映されます。",
                                L"完了", MB_OK | MB_ICONINFORMATION);
    else if (result == 69) MessageBoxW(gUi.window, L"設定は保存されましたが、次回起動用の記録を更新できませんでした。", L"起動設定の記録", MB_ICONWARNING);
    else if (result != -2) MessageBoxW(gUi.window, (L"保存できませんでした。エラー番号: " + std::to_wstring(result) +
                                         L"\n現在値を再読込しました。バックアップは残っています。").c_str(),
                                       L"VRSS設定", MB_OK | MB_ICONERROR);
    return result == 0 || result == 69;
}
void restoreFromUi() {
    if (!gUi.loadedOk) return;
    const auto& v = gUi.loaded;
    SendMessageW(gUi.mode, CB_SETCURSEL, v.mode.found && v.mode.raw ? 1 : 0, 0);
    setText(gUi.area, numberText(v.area.found ? bitsFloat(v.area.raw) : 50.0f));
    check(gUi.aspectCheck, v.aspect.custom);
    setText(gUi.aspect, numberText(v.aspect.found ? bitsFloat(v.aspect.raw) : 1.0f));
    EnableWindow(gUi.aspect, v.aspect.custom);
    updateEstimate();
}

bool applyGazeFromUi(bool restore) {
    if (!gUi.gazeLoadedOk) return false;
    if (restore) {
        const auto& v = gUi.gazeLoaded.values;
        setText(gUi.gazeX, gazeNumberText(v.x_gain));
        setText(gUi.gazeY, gazeNumberText(std::abs(v.y_gain)));
        setText(gUi.gazeLeft, gazeNumberText(v.left_deg));
        setText(gUi.gazeRight, gazeNumberText(v.right_deg));
        check(gUi.gazeInvert, v.y_gain < 0);
        return true;
    }
    live::Values values{};
    if (!restore && (!parseNumber(getText(gUi.gazeX), values.x_gain) || !parseNumber(getText(gUi.gazeY), values.y_gain) ||
        values.y_gain < .1f || !parseNumber(getText(gUi.gazeLeft), values.left_deg) ||
        !parseNumber(getText(gUi.gazeRight), values.right_deg))) {
        MessageBoxW(gUi.window, L"倍率は0.1～2、左右の角度は-5～+5度で指定してください。", L"入力エラー", MB_ICONWARNING); return false;
    }
    if (!restore && isChecked(gUi.gazeInvert)) values.y_gain = -values.y_gain;
    if (!restore && !live::Valid(values)) {
        MessageBoxW(gUi.window, L"倍率は0.1～2、左右の角度は-5～+5度で指定してください。", L"入力エラー", MB_ICONWARNING); return false;
    }
    if (gUi.target == live::Target::Pimax && !compatiblePimaxDriver()) {
        MessageBoxW(gUi.window, L"Pimax用の補正対応ドライバーを確認できません。補正は変更していません。", L"対応ドライバーの確認", MB_ICONWARNING); return false;
    }
    gBusy = true;
    EnableWindow(gUi.gazeApply, FALSE); EnableWindow(gUi.gazeRestore, FALSE);
    auto result = gGazeStore.Apply(gUi.target, gUi.gazeLoaded.hash, values);
    int code = result.code;
    if (code && !result.changed && result.winError == ERROR_ACCESS_DENIED) {
        std::wstring args = L"--apply-gaze ";
        args += live::Name(gUi.target);
        args += L" " + numberText(values.x_gain) + L" " + numberText(values.y_gain) + L" " +
            numberText(values.left_deg) + L" " + numberText(values.right_deg);
        args += L" " + live::Wide(gUi.gazeLoaded.hash);
        code = runElevated(args);
    } else if (code == 0 && !saveStartupState()) code = 69;
    gBusy = false;
    refreshGaze();
    if (code == 0) {
        setText(gUi.gazeStatus, getText(gUi.gazeStatus) + L"\r\n補正を保存・読戻し済み。VRChatの再起動は不要です。");
    } else if (code == 61) MessageBoxW(gUi.window, L"読込後に補正が別の操作で変更されています。現在値を読み直しました。内容を確認してから再度操作してください。", L"変更の競合", MB_ICONWARNING);
    else if (code == 69) MessageBoxW(gUi.window, L"補正は保存されましたが、次回起動用の記録を更新できませんでした。", L"起動設定の記録", MB_ICONWARNING);
    else if (code != -2) MessageBoxW(gUi.window, (L"補正の保存・復元を完了できませんでした。現在値を再読込しました。\nエラー番号: " + std::to_wstring(code)).c_str(), L"視線補正", MB_ICONERROR);
    return code == 0 || code == 69;
}

struct NamedProfile {
    int mode{}, target{};
    float area{}, aspect{}, x{}, y{}, left{}, right{};
    bool customAspect{};
};
std::filesystem::path profileFolder() { return backupFolder() / L"profiles"; }
bool validProfileName(const std::wstring& name) {
    return !name.empty() && name.size() <= 40 && name != L"." && name != L".." &&
        name.front() != L' ' && name.back() != L' ' &&
        name.find_first_of(L"<>:\"/\\|?*\r\n\t") == std::wstring::npos;
}
void listProfiles() {
    auto name = getText(gUi.profileName);
    SendMessageW(gUi.profileName, CB_RESETCONTENT, 0, 0);
    std::error_code ec;
    auto folder = profileFolder();
    if (std::filesystem::exists(folder, ec)) {
        for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
            if (item.is_regular_file() && item.path().extension() == L".ini") {
                auto stem = item.path().stem().wstring();
                SendMessageW(gUi.profileName, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(stem.c_str()));
            }
        }
    }
    setText(gUi.profileName, name);
}
bool readProfile(const std::wstring& name, NamedProfile& p) {
    if (!validProfileName(name)) return false;
    std::ifstream in(profileFolder() / (name + L".ini"), std::ios::binary);
    std::string magic;
    if (!std::getline(in, magic) || magic != "VRSS-TUNER-PROFILE-1") return false;
    int custom{};
    if (!(in >> p.mode >> p.area >> p.aspect >> custom >> p.target >> p.x >> p.y >> p.left >> p.right)) return false;
    std::string extra;
    if (in >> extra) return false;
    p.customAspect = custom == 1;
    live::Values gaze{p.left, p.right, p.x, p.y};
    return (p.mode == 0 || p.mode == 1) && p.area >= 1 && p.area <= 100 &&
        p.aspect >= .5f && p.aspect <= 2 && (custom == 0 || custom == 1) &&
        (p.target == 0 || p.target == 1) && live::Valid(gaze);
}
bool profileFromUi(NamedProfile& p) {
    p.mode = int(SendMessageW(gUi.mode, CB_GETCURSEL, 0, 0));
    p.target = int(SendMessageW(gUi.gazeTarget, CB_GETCURSEL, 0, 0));
    p.customAspect = isChecked(gUi.aspectCheck);
    if (!parseNumber(getText(gUi.area), p.area) ||
        !parseNumber(getText(gUi.aspect), p.aspect) ||
        !parseNumber(getText(gUi.gazeX), p.x) ||
        !parseNumber(getText(gUi.gazeY), p.y) ||
        !parseNumber(getText(gUi.gazeLeft), p.left) ||
        !parseNumber(getText(gUi.gazeRight), p.right)) return false;
    if (isChecked(gUi.gazeInvert)) p.y = -p.y;
    live::Values gaze{p.left, p.right, p.x, p.y};
    return (p.mode == 0 || p.mode == 1) && (p.target == 0 || p.target == 1) &&
        p.area >= 1 && p.area <= 100 && p.aspect >= .5f && p.aspect <= 2 && live::Valid(gaze);
}
bool writeProfile(const std::wstring& name, const NamedProfile& p) {
    if (!validProfileName(name)) return false;
    std::error_code ec;
    auto folder = profileFolder();
    std::filesystem::create_directories(folder, ec);
    if (ec) return false;
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(9);
    out << "VRSS-TUNER-PROFILE-1\n" << p.mode << ' ' << p.area << ' ' << p.aspect << ' '
        << (p.customAspect ? 1 : 0) << ' ' << p.target << ' ' << p.x << ' ' << p.y
        << ' ' << p.left << ' ' << p.right << '\n';
    DWORD error{};
    return live::AtomicWrite(folder / (name + L".ini"), out.str(), error);
}
void saveProfileFromUi() {
    const auto name = getText(gUi.profileName);
    NamedProfile p{};
    if (!validProfileName(name) || !profileFromUi(p)) {
        MessageBoxW(gUi.window, L"プロファイル名または設定値を確認してください。", L"プロファイル", MB_ICONWARNING);
        return;
    }
    if (!writeProfile(name, p)) {
        MessageBoxW(gUi.window, L"プロファイルを保存できません。", L"プロファイル", MB_ICONERROR); return;
    }
    listProfiles();
    setText(gUi.status, L"プロファイルを保存しました。現在のドライバー設定は変更していません。");
}
bool loadProfileToUi() {
    NamedProfile p{};
    if (!readProfile(getText(gUi.profileName), p)) {
        MessageBoxW(gUi.window, L"保存済みプロファイルを読み取れません。", L"プロファイル", MB_ICONWARNING);
        return false;
    }
    SendMessageW(gUi.mode, CB_SETCURSEL, p.mode, 0);
    setText(gUi.area, numberText(p.area));
    check(gUi.aspectCheck, p.customAspect);
    setText(gUi.aspect, numberText(p.aspect));
    EnableWindow(gUi.aspect, p.customAspect);
    SendMessageW(gUi.gazeTarget, CB_SETCURSEL, p.target, 0);
    refreshGaze();
    setText(gUi.gazeX, gazeNumberText(p.x));
    setText(gUi.gazeY, gazeNumberText(std::abs(p.y)));
    setText(gUi.gazeLeft, gazeNumberText(p.left));
    setText(gUi.gazeRight, gazeNumberText(p.right));
    check(gUi.gazeInvert, p.y < 0);
    updateEstimate();
    return true;
}
void switchProfile() {
    NamedProfile selected{};
    if (!readProfile(getText(gUi.profileName), selected)) {
        MessageBoxW(gUi.window, L"保存済みプロファイルを読み取れません。", L"プロファイル", MB_ICONWARNING);
        return;
    }
    live::Snapshot available{};
    if (!gGazeStore.Read(selected.target == 0 ? live::Target::Pimax : live::Target::Common, available)) {
        MessageBoxW(gUi.window, L"この経路の視線補正ファイルがありません。適用していません。", L"プロファイル", MB_ICONWARNING);
        return;
    }
    if (MessageBoxW(gUi.window, L"このプロファイルを画質と視線補正へ適用します。画質設定はVRChatの再起動後に反映します。続けますか？",
        L"プロファイルの切替", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return;
    if (!loadProfileToUi()) return;
    NamedProfile wanted{};
    if (!profileFromUi(wanted)) return;
    const auto& current = gUi.loaded;
    const bool driverChanged = !current.mode.found || current.mode.raw != unsigned(wanted.mode) ||
        !current.area.found || std::abs(bitsFloat(current.area.raw) - wanted.area) > .00001f ||
        current.aspect.custom != wanted.customAspect ||
        (wanted.customAspect && (!current.aspect.found || std::abs(bitsFloat(current.aspect.raw) - wanted.aspect) > .00001f));
    const auto& gaze = gUi.gazeLoaded.values;
    const bool gazeChanged = std::abs(gaze.x_gain-wanted.x) > .00001f ||
        std::abs(gaze.y_gain-wanted.y) > .00001f ||
        std::abs(gaze.left_deg-wanted.left) > .00001f ||
        std::abs(gaze.right_deg-wanted.right) > .00001f;
    if (driverChanged && !applyFromUi(false)) return;
    if (gazeChanged && !applyGazeFromUi(false)) {
        MessageBoxW(gUi.window, driverChanged ? L"画質設定のみ保存されました。視線補正は適用できていません。" :
            L"視線補正を適用できませんでした。", L"一部だけ反映", MB_ICONWARNING);
    } else MessageBoxW(gUi.window, driverChanged ? L"プロファイルを適用しました。画質への反映にはVRChatの再起動が必要です。" : L"プロファイルを適用しました。",
                         L"切替完了", MB_ICONINFORMATION);
}

babble::Relay gInputRelay;
void updateInputStatus() {
    if (!gUi.inputStatus) return;
    const bool vendor = processCount(L"vrss_gaze_provider.exe") != 0;
    const bool common = processCount(L"common_vrss_provider.exe") != 0;
    if (gInputRelay.Running()) {
        live::Snapshot calibration{};
        if (gGazeStore.Read(live::Target::Common, calibration))
            gInputRelay.SetCalibration(calibration.values.x_gain, calibration.values.y_gain,
                calibration.values.left_deg, calibration.values.right_deg);
    }
    std::wstring status = gInputRelay.Running() ? L"Baballonia OSC: 受信中" : L"Baballonia OSC: 停止中";
    status += L"\r\n入力 " + std::to_wstring(gInputRelay.Received()) +
        L" / 共通経路へ送信 " + std::to_wstring(gInputRelay.Sent());
    status += vendor ? L"\r\nPimax公式経路を優先中。共通経路への送信は停止します。" :
        common ? L"\r\n共通プロバイダーを検出。UDP受信モードで起動してください。" :
                 L"\r\n共通プロバイダーが停止中。送信は行いません。";
    status += L"\r\nOSC値からの位置変換は実機での合わせ込みが必要です。";
    setText(gUi.inputStatus, status);
    EnableWindow(gUi.inputStart, !gInputRelay.Running());
    EnableWindow(gUi.inputStop, gInputRelay.Running());
}
void startInputRelay() {
    float portFloat{}, forwardFloat{};
    if (!parseNumber(getText(gUi.inputPort), portFloat) || portFloat != std::floor(portFloat) ||
        portFloat < 1 || portFloat > 65535 ||
        !parseNumber(getText(gUi.inputForward), forwardFloat) || forwardFloat != std::floor(forwardFloat) ||
        forwardFloat < 0 || forwardFloat > 65535 || forwardFloat == portFloat) {
        MessageBoxW(gUi.window, L"ポート番号を確認してください。転送先は0で無効にできます。", L"視線入力", MB_ICONWARNING);
        return;
    }
    live::Snapshot common{};
    live::Values gain{};
    if (gGazeStore.Read(live::Target::Common, common)) gain = common.values;
    const bool started = gInputRelay.Start(int(portFloat), gain.x_gain, gain.y_gain,
        gain.left_deg, gain.right_deg, [] {
            return processCount(L"vrss_gaze_provider.exe") == 0 &&
                processCount(L"common_vrss_provider.exe") > 0;
        }, 5556, int(forwardFloat));
    if (!started) MessageBoxW(gUi.window, L"OSC受信を開始できません。ポートが他のソフトで使用中か確認してください。", L"視線入力", MB_ICONWARNING);
    updateInputStatus();
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        gUi.window = window;
        gFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        control(L"STATIC", L"VRSS Tuner", SS_LEFT, 20, 14, 205, 25);
        control(L"STATIC", L"プロファイル", SS_LEFT, 230, 14, 95, 25);
        gUi.profileName = control(L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, 328, 10, 165, 250, kProfileName);
        gUi.profileSave = control(L"BUTTON", L"保存", WS_TABSTOP | BS_PUSHBUTTON, 505, 10, 72, 27, kProfileSave);
        gUi.profileSwitch = control(L"BUTTON", L"切替", WS_TABSTOP | BS_PUSHBUTTON, 585, 10, 85, 27, kProfileSwitch);
        gUi.driverTab = control(L"BUTTON", L"画質", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_TABSTOP, 20, 47, 150, 30, kDriverTab);
        gUi.gazeTab = control(L"BUTTON", L"視線補正", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_TABSTOP, 180, 47, 150, 30, kGazeTab);
        gUi.inputTab = control(L"BUTTON", L"視線入力", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_TABSTOP, 340, 47, 150, 30, kInputTab);
        gUi.language = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                               520, 47, 150, 100, kLanguage);
        SendMessageW(gUi.language, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"日本語"));
        SendMessageW(gUi.language, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessageW(gUi.language, CB_SETCURSEL, language::english ? 1 : 0, 0);
        gCreatingPanel = 0;
        control(L"STATIC", L"VRSSモード", SS_LEFT, 20, 102, 170, 23);
        gUi.mode = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 205, 98, 340, 100, kModeCombo);
        for (const wchar_t* name : {L"オフ", L"オン"}) {
            const auto translated = language::Text(name);
            SendMessageW(gUi.mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(translated.c_str()));
        }
        gUi.restartBadges.push_back(control(L"STATIC", L"● 要再起動", SS_LEFT, 565, 102, 110, 24));
        gUi.aspectCheck = control(L"BUTTON", L"縦横比を指定", BS_AUTOCHECKBOX | WS_TABSTOP, 20, 145, 185, 26, kAspectCheck);
        gUi.aspect = control(L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 225, 144, 95, 27, kAspectEdit);
        control(L"STATIC", L"0.5～2", SS_LEFT, 335, 148, 100, 25);
        gUi.restartBadges.push_back(control(L"STATIC", L"● 要再起動", SS_LEFT, 565, 148, 110, 24));
        control(L"STATIC", L"領域プレビュー", SS_LEFT,20,194,210,23);
        control(L"STATIC", L"大きさ（1～100）", SS_LEFT,20,226,180,23);
        gUi.area = control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 205, 222, 90, 27, kAreaEdit);
        gUi.areaSlider = control(TRACKBAR_CLASSW,L"",TBS_NOTICKS | WS_TABSTOP,305,220,245,30,kAreaSlider);
        SendMessageW(gUi.areaSlider,TBM_SETRANGE,TRUE,MAKELPARAM(1,100));
        SendMessageW(gUi.areaSlider,TBM_SETPAGESIZE,0,5);
        gUi.restartBadges.push_back(control(L"STATIC", L"● 要再起動", SS_LEFT, 565, 226, 110, 24));
        control(L"STATIC", L"試算用 MSAA", SS_LEFT,350,264,110,23);
        gUi.estimateMsaa=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST | WS_TABSTOP,470,260,160,100,kEstimateMsaa);
        for(const wchar_t* name:{L"MSAA x2",L"MSAA x4",L"MSAA x8"}) SendMessageW(gUi.estimateMsaa,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        SendMessageW(gUi.estimateMsaa,CB_SETCURSEL,1,0);
        gUi.regionPreview=control(L"VrssRegionPreview",L"",0,20,290,310,134);
        gUi.regionArea=control(L"STATIC",L"",SS_LEFT,350,304,325,24);
        gUi.regionCost=control(L"STATIC",L"",SS_LEFT,350,334,325,24);
        LOGFONTW bold{}; GetObjectW(gFont,sizeof(bold),&bold); bold.lfWeight=FW_BOLD; gBoldFont=CreateFontIndirectW(&bold);
        SendMessageW(gUi.regionCost,WM_SETFONT,reinterpret_cast<WPARAM>(gBoldFont),TRUE);
        gUi.regionNote=control(L"STATIC",L"",SS_LEFT,350,366,325,64);
        control(L"STATIC",L"視線は中央で固定。負荷は均等な場合の概算です。試算用MSAAはゲーム設定を変えません。",SS_LEFT,20,450,655,24);
        gUi.installInfo = control(L"STATIC", L"", SS_LEFT, 20, 488, 655, 44);
        control(L"BUTTON", L"画質設定を保存", WS_TABSTOP | BS_PUSHBUTTON, 20, 550, 160, 32, kApplyButton);
        gUi.restore = control(L"BUTTON", L"操作を戻す", WS_TABSTOP | BS_PUSHBUTTON, 195, 550, 145, 32, kRestoreButton);
        control(L"BUTTON", L"現在値を読込", WS_TABSTOP | BS_PUSHBUTTON, 505, 550, 165, 32, kRefreshButton);
        gUi.status = control(L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                             20, 595, 650, 55, kStatusEdit);
        control(L"STATIC", L"● 要再起動 = 対象アプリのみ。保存時に確認します。", SS_LEFT, 20, 664, 655, 23);
        gCreatingPanel = 1;
        control(L"STATIC", L"補正する経路", SS_LEFT, 20, 110, 170, 24);
        gUi.gazeTarget = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 205, 106, 340, 120, kGazeTarget);
        for (const wchar_t* name : {L"Pimax経路", L"共通プロバイダー"}) {
            const auto translated = language::Text(name);
            SendMessageW(gUi.gazeTarget, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(translated.c_str()));
        }
        const bool preferCommon = !processCount(L"vrss_gaze_provider.exe") &&
            (processCount(L"common_vrss_provider.exe") ||
             (!compatiblePimaxDriver() && !gResearch.empty() &&
              std::filesystem::is_regular_file(gResearch / L"common-vrss-provider/common_vrss_provider.exe")));
        SendMessageW(gUi.gazeTarget, CB_SETCURSEL, preferCommon ? 1 : 0, 0);
        control(L"STATIC", L"横方向の倍率", SS_LEFT, 20, 155, 170, 24);
        gUi.gazeX = control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 205, 151, 115, 27, kGazeX);
        control(L"STATIC", L"0.1～2", SS_LEFT, 335, 155, 200, 24);
        control(L"STATIC", L"縦方向の倍率", SS_LEFT, 20, 200, 170, 24);
        gUi.gazeY = control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 205, 196, 115, 27, kGazeY);
        gUi.gazeInvert = control(L"BUTTON", L"上下方向を反転", BS_AUTOCHECKBOX | WS_TABSTOP, 350, 197, 220, 27, kGazeInvert);
        control(L"STATIC", L"左目の横角度オフセット", SS_LEFT, 20, 245, 180, 24);
        gUi.gazeLeft = control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 205, 241, 115, 27, kGazeLeft);
        control(L"STATIC", L"度（-5～+5）", SS_LEFT, 335, 245, 200, 24);
        control(L"STATIC", L"右目の横角度オフセット", SS_LEFT, 20, 290, 180, 24);
        gUi.gazeRight = control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 205, 286, 115, 27, kGazeRight);
        control(L"STATIC", L"度（-5～+5）", SS_LEFT, 335, 290, 200, 24);
        control(L"STATIC", L"Pimax公式経路があれば優先。共通経路のUDP入力は補正済みの値として扱います。", SS_LEFT, 20, 328, 650, 26);
        gUi.gazeApply = control(L"BUTTON", L"視線補正を反映", WS_TABSTOP | BS_PUSHBUTTON, 20, 386, 150, 32, kGazeApply);
        gUi.gazeRestore = control(L"BUTTON", L"操作を戻す", WS_TABSTOP | BS_PUSHBUTTON, 180, 386, 145, 32, kGazeRestore);
        control(L"BUTTON", L"現在値を読込", WS_TABSTOP | BS_PUSHBUTTON, 505, 550, 165, 32, kGazeRefresh);
        gUi.gazeStatus = control(L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 20, 435, 650, 100);
        control(L"STATIC", L"VRChat再起動不要。保存前の補正は経路別にバックアップします。", SS_LEFT, 20, 664, 655, 23);
        gCreatingPanel = 2;
        control(L"STATIC",L"視線の入力元",SS_LEFT,20,105,180,23);
        control(L"STATIC",L"Baballonia OSC（左目・右目の視線）",SS_LEFT,205,105,420,23);
        control(L"STATIC",L"受信ポート",SS_LEFT,20,150,180,23);
        gUi.inputPort=control(L"EDIT",L"9020",WS_BORDER | ES_NUMBER | WS_TABSTOP,205,146,110,27,kInputPort);
        control(L"STATIC",L"OSC転送先（0=なし）",SS_LEFT,20,190,180,23);
        gUi.inputForward=control(L"EDIT",L"8888",WS_BORDER | ES_NUMBER | WS_TABSTOP,205,186,110,27,kInputForward);
        control(L"STATIC",L"VRCFaceTrackingへ転送する場合は8888",SS_LEFT,335,190,320,23);
        gUi.inputStart=control(L"BUTTON",L"配信を開始",WS_TABSTOP | BS_PUSHBUTTON,20,240,145,32,kInputStart);
        gUi.inputStop=control(L"BUTTON",L"停止",WS_TABSTOP | BS_PUSHBUTTON,180,240,105,32,kInputStop);
        gUi.inputStatus=control(L"EDIT",L"停止中",WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,20,295,650,145,kInputStatus);
        control(L"STATIC",L"Baballoniaの出力先を受信ポートに設定します。Pimax公式経路が有効なら共通経路への送信を止めます。",SS_LEFT,20,460,650,52);
        gCreatingPanel = -1;
        listProfiles();
        refresh();
        refreshGaze();
        updateEstimate();
        updateInputStatus();
        SetTimer(window, 1, 1000, nullptr);
        showPanel(0);
        return 0;
    }
    case WM_COMMAND:
        if (gBusy) return 0;
        if (LOWORD(wParam) == kLanguage && HIWORD(wParam) == CBN_SELCHANGE) {
            language::english = SendMessageW(gUi.language, CB_GETCURSEL, 0, 0) == 1;
            saveLanguage();
            applyLanguage();
            return 0;
        }
        if ((HIWORD(wParam)==EN_CHANGE && (LOWORD(wParam)==kAreaEdit || LOWORD(wParam)==kAspectEdit)) ||
            (HIWORD(wParam)==CBN_SELCHANGE && (LOWORD(wParam)==kModeCombo || LOWORD(wParam)==kLayoutCombo || LOWORD(wParam)==kEstimateMsaa))) {
            updateEstimate();return 0;
        }
        if (LOWORD(wParam) == kGazeTarget && HIWORD(wParam) == CBN_SELCHANGE) { refreshGaze(); return 0; }
        if (HIWORD(wParam) == BN_CLICKED) {
            switch (LOWORD(wParam)) {
            case kRefreshButton: refresh(); return 0;
            case kApplyButton: applyFromUi(); return 0;
            case kRestoreButton: restoreFromUi(); return 0;
            case kAspectCheck: EnableWindow(gUi.aspect, isChecked(gUi.aspectCheck)); updateEstimate();return 0;
            case kDriverTab: showPanel(0); return 0;
            case kGazeTab: showPanel(1); return 0;
            case kGazeRefresh: refreshGaze(); return 0;
            case kGazeApply: applyGazeFromUi(false); return 0;
            case kGazeRestore: applyGazeFromUi(true); return 0;
            case kGazeReset:
                return 0;
            case kProfileSave: saveProfileFromUi(); return 0;
            case kProfileSwitch: switchProfile(); return 0;
            case kInputTab: showPanel(2); return 0;
            case kInputStart: startInputRelay(); return 0;
            case kInputStop: gInputRelay.Stop(); updateInputStatus(); return 0;
            }
        }
        break;
    case WM_HSCROLL:
        if (!gBusy && reinterpret_cast<HWND>(lParam)==gUi.areaSlider) {
            setText(gUi.area,std::to_wstring(SendMessageW(gUi.areaSlider,TBM_GETPOS,0,0)));updateEstimate();return 0;
        }
        break;
    case WM_TIMER: if (wParam == 1) { updateInputStatus(); return 0; } break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam); HWND item = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(25, 25, 25));
        for (HWND badge : gUi.restartBadges) if (item == badge) SetTextColor(dc, RGB(164, 90, 0));
        if (item==gUi.regionCost) SetTextColor(dc,RGB(27,100,194));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_CLOSE: if (gBusy) return 0; break;
    case WM_DESTROY: KillTimer(window, 1); gInputRelay.Stop(); if(gBoldFont) DeleteObject(gBoldFont);PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    int argc{};
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 30;
    initializeGazePaths();
    loadLanguage();
    if (argc > 1) {
        int result = 31;
        if (wcscmp(argv[1], L"--readiness") == 0 && argc == 3) {
            Session session; ProfileValues values{};
            NvAPI_Status status = session.open();
            if (status == NVAPI_OK) status = session.readAll(values);
            live::Snapshot pimax{}, common{};
            const bool pimaxCalibration = gGazeStore.Read(live::Target::Pimax, pimax);
            const bool commonCalibration = gGazeStore.Read(live::Target::Common, common);
            const bool compatiblePimax = !gResearch.empty() && compatiblePimaxDriver();
            const bool commonBinary = !gResearch.empty() &&
                std::filesystem::is_regular_file(gResearch / L"common-vrss-provider/common_vrss_provider.exe");
            std::ofstream report(argv[2], std::ios::binary | std::ios::trunc);
            if (report) {
                report << "VRSS-TUNER-READINESS-1\n"
                       << "research_root=" << (gResearch.empty() ? "missing" : "found") << '\n'
                       << "nvapi_vrchat_profile=" << (status == NVAPI_OK ? "readable" : "unavailable") << '\n'
                       << "nvapi_status=" << status << '\n'
                       << "graphics_write_entry=" << (status == NVAPI_OK && session.hidden.set ? "available" : "unavailable") << '\n'
                       << "vrss_allow=" << (status == NVAPI_OK && values.allow.found && values.allow.raw == 1 ? "enabled" : "not_enabled") << '\n'
                       << "vrss_mode=" << (status != NVAPI_OK ? "unknown" : values.mode.raw ? "on" : "off") << '\n'
                       << "pimax_calibration=" << (pimaxCalibration ? "readable" : "missing") << '\n'
                       << "pimax_compatible_driver=" << (compatiblePimax ? "verified" : "unverified") << '\n'
                       << "pimax_provider_running=" << (processCount(L"vrss_gaze_provider.exe") ? "yes" : "no") << '\n'
                       << "common_provider_binary=" << (commonBinary ? "present" : "missing") << '\n'
                       << "common_calibration=" << (commonCalibration ? "readable" : "missing") << '\n'
                       << "common_provider_running=" << (processCount(L"common_vrss_provider.exe") ? "yes" : "no") << '\n'
                       << "live_gaze_mapping=not_verified_by_this_check\n";
                report.flush();
                result = report.good() ? 0 : 72;
            } else result = 72;
        }
        else if (wcscmp(argv[1], L"--apply") == 0) { result = applyCommand(argc, argv); if (!result && !saveStartupState()) result = 69; }
        else if (wcscmp(argv[1], L"--restore") == 0 && argc == 2) { result = restoreCommand(); if (!result && !saveStartupState()) result = 69; }
        else if (wcscmp(argv[1], L"--apply-gaze") == 0) result = gazeCommand(argc, argv, false);
        else if (wcscmp(argv[1], L"--restore-gaze") == 0) result = gazeCommand(argc, argv, true);
        else if (wcscmp(argv[1], L"--check") == 0 && argc == 2) {
            Session session;
            ProfileValues values{};
            NvAPI_Status status = session.open();
            if (status == NVAPI_OK) status = session.readAll(values);
            result = status == NVAPI_OK ? 0 : 32;
        }
        else if (wcscmp(argv[1], L"--self-test") == 0 && argc == 2) {
            float value{};
            result = parseNumber(L"5", value) && value == 5.0f &&
                     parseNumber(L"0.75", value) && value == 0.75f &&
                     !parseNumber(L"5junk", value) && !parseNumber(L"NaN", value) &&
                     bitsFloat(floatBits(1.25f)) == 1.25f ? 0 : 35;
        }
        else if (wcscmp(argv[1], L"--profile-self-test") == 0 && argc == 3) {
            SetEnvironmentVariableW(L"LOCALAPPDATA", argv[2]);
            NamedProfile original{};
            original.mode = 1; original.target = 1; original.area = 35;
            original.aspect = 1.25f; original.customAspect = true;
            original.x = .5f; original.y = -.55f; original.left = -2; original.right = 2;
            NamedProfile actual{};
            bool pass = writeProfile(L"CodexProfileTest", original) &&
                readProfile(L"CodexProfileTest", actual) &&
                actual.mode == original.mode && actual.target == original.target &&
                actual.area == original.area && actual.aspect == original.aspect &&
                actual.customAspect == original.customAspect && actual.x == original.x &&
                actual.y == original.y && actual.left == original.left && actual.right == original.right &&
                !validProfileName(L"../escape") && !validProfileName(L"bad:name");
            result = pass ? 0 : 37;
        }
        else if (wcscmp(argv[1], L"--startup-self-test") == 0 && argc == 3) {
            SetEnvironmentVariableW(L"LOCALAPPDATA", argv[2]);
            result = saveStartupState() ? 0 : 69;
        }
        else if (wcscmp(argv[1], L"--status") == 0 && argc == 2) {
            Session session;
            ProfileValues values{};
            NvAPI_Status status = session.open();
            if (status == NVAPI_OK) status = session.readAll(values);
            if (status == NVAPI_OK) {
                std::wostringstream line;
                line << L"mode=" << values.mode.raw << L" area=" << bitsFloat(values.area.raw)
                     << L" aspect=" << (values.aspect.found ? bitsFloat(values.aspect.raw) : 1.0f)
                     << L" layout=" << values.layout.raw << L" hmd_gate=" << values.gate.raw;
                MessageBoxW(nullptr, line.str().c_str(), L"VRSS Tuner / 読取専用", MB_OK);
                result = 0;
            } else result = 32;
        }
        LocalFree(argv);
        return result;
    }
    LocalFree(argv);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW cls{sizeof(cls)};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = instance;
    cls.lpszClassName = L"VrssTunerMainWindow";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&cls)) return 33;
    WNDCLASSW preview{}; preview.lpfnWndProc=regionPreviewProc;preview.hInstance=instance;
    preview.lpszClassName=L"VrssRegionPreview"; preview.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassW(&preview)) return 36;
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int x=work.left+std::max(0,(int(work.right-work.left)-710)/2),y=work.top+std::max(0,(int(work.bottom-work.top)-730)/2);
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"VRSS Tuner",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  x, y, 710, 730, nullptr, nullptr, instance, nullptr);
    if (!window) return 34;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
