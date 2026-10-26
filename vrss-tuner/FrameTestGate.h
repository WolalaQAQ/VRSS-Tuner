#pragma once
#include <cstdint>
#include <istream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace frame {
struct GateValue { bool found{},custom{};uint32_t raw{}; };
inline bool operator==(GateValue a,GateValue b) {
    return a.found==b.found && a.custom==b.custom && a.raw==b.raw;
}
inline std::string SerializeGateBackup(GateValue value) {
    std::ostringstream out;
    out<<"VRSS-FRAME-GATE-BACKUP-1\n"<<value.found<<' '<<value.custom<<' '<<value.raw<<'\n';
    return out.str();
}
inline std::optional<GateValue> ReadGateBackup(std::istream& in) {
    std::string header,extra;uint64_t found{},custom{},raw{};
    if(!std::getline(in,header) || header!="VRSS-FRAME-GATE-BACKUP-1" ||
       !(in>>found>>custom>>raw) || found>1 || custom>1 || raw>0xffffffffULL ||
       (!found && (custom || raw)) || (in>>extra))return {};
    return GateValue{found!=0,custom!=0,uint32_t(raw)};
}
template<class Store> class TestGateOverride {
    Store& store_;
    std::optional<GateValue> original_;
    bool armed_{};
public:
    explicit TestGateOverride(Store& store):store_(store) {}
    TestGateOverride(Store& store,GateValue original):store_(store),original_(original),armed_(true) {}
    void Apply() {
        if(original_)throw std::runtime_error("Gate override already prepared");
        original_=store_.Read();
        store_.Backup(*original_);
        if(original_->found && original_->raw==0)return;
        armed_=true;
        store_.WriteTest();
        if(!(store_.Read()==GateValue{true,true,0}))throw std::runtime_error("Test gate read-back mismatch");
    }
    void Recover() {
        if(!armed_)return;
        const auto current=store_.Read();
        if(current==*original_) {armed_=false;return;}
        if(!(current==GateValue{true,true,0}))
            throw std::runtime_error("Gate changed externally; recovery refused to overwrite it");
        store_.Restore(*original_);
        if(!(store_.Read()==*original_))throw std::runtime_error("Gate recovery read-back mismatch");
        armed_=false;
    }
};
template<class Store> void RecoverPendingGate(Store& store) {
    const auto original=store.Pending();
    if(!original)return;
    TestGateOverride<Store> recovery(store,*original);
    recovery.Recover();
    store.ClearPending();
}
}
