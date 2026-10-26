#pragma once
#include "FrameStartup.h"
#include "FrameProcess.h"
#include "FrameGameLaunch.h"
#include "../common-vrss-provider/HmdIdentity.h"
#include <shellapi.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace frame {
class LogRedirect {
    std::streambuf* out_;
    std::streambuf* err_;
public:
    explicit LogRedirect(std::ostream& log):out_(std::cout.rdbuf(log.rdbuf())),err_(std::cerr.rdbuf(log.rdbuf())) {}
    ~LogRedirect() { std::cout.rdbuf(out_);std::cerr.rdbuf(err_); }
    LogRedirect(const LogRedirect&)=delete;
    LogRedirect& operator=(const LogRedirect&)=delete;
};
inline std::wstring LogTail(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return {};
    const auto length=file.tellg();
    file.seekg(length>std::streamoff(2400)?length-std::streamoff(2400):std::streampos(0));
    std::string bytes((std::istreambuf_iterator<char>(file)),{});
    const int count=MultiByteToWideChar(CP_UTF8,0,bytes.data(),int(bytes.size()),nullptr,0);
    std::wstring result(size_t(count),L'\0');
    if(count)MultiByteToWideChar(CP_UTF8,0,bytes.data(),int(bytes.size()),result.data(),count);
    return result;
}
class Runtime {
    Startup startup_;
    HANDLE process_{};
    std::filesystem::path run_;
    std::wstring message_{L"Steam Frameの自動接続を準備しています。"};
    bool launching_{};
    bool stopping_{},gameRequested_{};
    DWORD exitCode_{};
    bool Alive() const { return process_ && WaitForSingleObject(process_,0)==WAIT_TIMEOUT; }
    bool ReadOwnedStatus(std::string& status) const {
        if(!Alive())return false;
        WIN32_FILE_ATTRIBUTE_DATA attributes{};FILETIME now{};GetSystemTimeAsFileTime(&now);
        if(!GetFileAttributesExW((run_/L"status.json").c_str(),GetFileExInfoStandard,&attributes) ||
           (attributes.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) ||
           attributes.nFileSizeHigh || attributes.nFileSizeLow>=4096)return false;
        const auto written=attributes.ftLastWriteTime;
        if(!StatusFresh((uint64_t(now.dwHighDateTime)<<32)|now.dwLowDateTime,
           (uint64_t(written.dwHighDateTime)<<32)|written.dwLowDateTime))return false;
        std::ifstream file(run_/L"status.json",std::ios::binary);
        char bytes[4096]{};file.read(bytes,sizeof(bytes));
        status.assign(bytes,size_t(file.gcount()));
        return status.find("\"pid\":"+std::to_string(GetProcessId(process_))+",")!=std::string::npos;
    }
    void Launch(const std::filesystem::path& root,HWND owner) {
        launching_=true;exitCode_=0;stopping_=false;gameRequested_=false;
        try {
            if(root.empty())throw std::runtime_error("LOCALAPPDATA is unavailable");
            const auto id=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
            run_=root/L"frame-runs"/id;
            std::filesystem::create_directories(run_);
            RequirePlainDirectory(run_);
            wchar_t executable[32768]{};
            if(!GetModuleFileNameW(nullptr,executable,32768))throw std::runtime_error("Cannot resolve the executable");
            const std::wstring args=L"--frame-worker "+std::to_wstring(GetCurrentProcessId())+L" "+
                std::to_wstring(ProcessCreated(GetCurrentProcess()))+L" "+id+L" steam-frame";
            SHELLEXECUTEINFOW info{sizeof(info)};
            info.hwnd=owner;info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpVerb=L"runas";
            info.lpFile=executable;info.lpParameters=args.c_str();info.nShow=SW_HIDE;
            if(!ShellExecuteExW(&info)) {
                message_=GetLastError()==ERROR_CANCELLED ?
                    L"管理者確認がキャンセルされました。「再試行」で再開できます。" :
                    L"ワーカーを起動できません。Windowsエラー: "+std::to_wstring(GetLastError());
                startup_.LaunchFinished(false);
            } else {
                process_=info.hProcess;startup_.LaunchFinished(process_!=nullptr);
                message_=L"596.02検査・HMD接続・プロバイダー登録を開始しています。";
            }
        } catch(const std::exception& error) {
            const std::string text=error.what();message_=L"起動準備に失敗: "+std::wstring(text.begin(),text.end());
            startup_.LaunchFinished(false);
        }
        launching_=false;
    }
public:
    Runtime() {startup_.Stop();message_=L"Steam Frameの接続を開始してください。";}
    ~Runtime() { Stop();if(process_)CloseHandle(process_); }
    bool WorkerAlive() const { return Alive(); }
    bool CanLaunchGame() const {
        if(!Alive() || stopping_ || gameRequested_)return false;
        std::string status;
        return ReadOwnedStatus(status) && LaunchStatusReady(GetProcessId(process_),GetProcessId(process_),status);
    }
    void LaunchGame(HWND owner) {
        if(!CanLaunchGame())throw std::runtime_error("The current Frame session is not ready for a game launch");
        const auto result=StartFrameVrchat(owner);
        gameRequested_=true;
        std::ofstream log(run_/L"vrchat-launch.log",std::ios::binary);
        log<<"launcher=start_protected_game.exe pid="<<result.pid<<" shellExecute=1 environmentRestored="
           <<result.environmentRestored<<" persistentEnvironmentWrites=0 gameFilesChanged=0\n";
        message_=result.environmentRestored ? L"EACランチャーへ起動を要求しました。VRChat側の接続を待っています。" :
            L"ゲーム起動を要求しましたが、Tuner内の環境変数復元に失敗しました。重複起動せずTunerを終了してください。";
    }
    const std::wstring& Message() const { return message_; }
    void Retry() { if(!Alive()&&!launching_){startup_.Retry();message_=L"再接続を準備しています。";} }
    void Stop() {
        startup_.Stop();stopping_=true;
        if(Alive()&&!run_.empty()) {
            std::ofstream stop(run_/L"stop.request");
            message_=stop ? L"停止を要求しました。登録解除を待っています。" : L"停止要求を書き込めません。アプリ終了で所有者監視による停止を行います。";
        } else message_=L"停止中。再試行するまで自動起動しません。";
    }
    void Tick(Environment env,const std::filesystem::path& root,HWND owner) {
        if(launching_)return;
        if(process_&&!Alive()) {
            GetExitCodeProcess(process_,&exitCode_);CloseHandle(process_);process_=nullptr;
            message_=L"ワーカーが終了しました。終了コード: "+std::to_wstring(exitCode_)+L"\r\n"+LogTail(run_/L"worker.log");
        }
        env.workerAlive=Alive();
        if(!env.workerAlive) {
            SetLastError(ERROR_SUCCESS);
            Handle other(OpenMutexW(SYNCHRONIZE,FALSE,kWorkerMutex));
            env.conflict=env.conflict || other.get()!=nullptr || GetLastError()==ERROR_ACCESS_DENIED;
        }
        switch(startup_.Next(env)) {
        case Action::WaitSteamVr: message_=L"SteamVR待機中。Steam FrameをPCへ接続すると自動開始します。";break;
        case Action::WaitVrchat: message_=L"VRChatまたはUnityの終了待ちです。保存して終了すると自動開始します。自動終了はしません。";break;
        case Action::Conflict: message_=L"別の視線プロバイダーが動作中です。競合するため自動で置き換えません。";break;
        case Action::Launch: Launch(root,owner);break;
        case Action::Running: {
            std::string status;
            if(!ReadOwnedStatus(status)) {
                message_=L"今回のワーカーから新しい状態通知を確認できません。送信状態は不明です。";
                break;
            }
            if(status.find("\"registered\":true")!=std::string::npos) {
                if(status.find("\"deliveryState\":\"driver-error\"")!=std::string::npos)
                    message_=L"登録済みですが、受信側の照会または視線送信に失敗しています。ワーカーログを確認してください。";
                else if(status.find("\"deliveryState\":\"submitting\"")!=std::string::npos)
                    message_=L"登録済み・受信側に視線を送信中です。実際のVRSS描画はVRChat内で別途確認が必要です。";
                else if(status.find("\"deliveryState\":\"waiting-consumer\"")!=std::string::npos)
                    message_=!gameRequested_ ?
                        L"登録済み・視線取得中です。「VRChatを起動（Frame用）」から起動してください。受信側の接続と描画効果はまだ未確認です。" :
                        L"登録済み・視線取得中です。現在の受信側状態は送信待ちです。実描画は別途確認してください。";
                else message_=L"登録済み・有効な視線を待っています。Frameの装着状態とSteamVRの視線共有設定を確認してください。";
            }
            else message_=L"状態通知から登録を確認できません。ワーカーログを確認してください。";
            break;
        }
        case Action::Stopped: if(!Alive()&&!exitCode_)message_=L"停止中。再試行するまで自動起動しません。";break;
        default: break;
        }
    }
};
}
