#pragma once
#include <cstdint>
namespace frame {
inline bool StatusFresh(uint64_t now,uint64_t modified) {
    return modified!=0 && now>=modified && now-modified<=30000000;
}
enum class Action { WaitSteamVr, WaitVrchat, Conflict, Launch, Starting, Running, Stopped, Failed };
struct Environment { bool steamVr{}, vrchat{}, conflict{}, workerAlive{}; };
class Startup {
    bool enabled_{true};
    enum class Attempt { None, Pending, Launched, Failed } attempt_{};
public:
    Action Next(const Environment& env) {
        if (!enabled_) return Action::Stopped;
        if (env.workerAlive) return Action::Running;
        if (attempt_ == Attempt::Pending) return Action::Starting;
        if (attempt_ != Attempt::None) return Action::Failed;
        if (env.conflict) return Action::Conflict;
        if (!env.steamVr) return Action::WaitSteamVr;
        if (env.vrchat) return Action::WaitVrchat;
        attempt_ = Attempt::Pending;
        return Action::Launch;
    }
    void LaunchFinished(bool success) { attempt_ = success ? Attempt::Launched : Attempt::Failed; }
    void Stop() { enabled_ = false; }
    void Retry() { enabled_ = true; attempt_ = Attempt::None; }
};
}
