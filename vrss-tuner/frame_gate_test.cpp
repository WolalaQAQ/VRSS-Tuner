#include "FrameTestGate.h"
#include <cassert>
#include <iostream>
#include <sstream>

using namespace frame;
struct Store {
    GateValue value{true,false,1};
    bool failBackup{},failAfterWrite{};
    int backups{},writes{},restores{};
    GateValue Read() { return value; }
    std::optional<GateValue> pending;
    bool failClear{};
    std::optional<GateValue> Pending() { return pending; }
    void ClearPending() { if(failClear)throw std::runtime_error("clear failed");pending.reset(); }
    void Backup(GateValue before) {
        assert(before==value);++backups;
        if(failBackup)throw std::runtime_error("backup failure");
    }
    void WriteTest() {
        ++writes;value={true,true,0};
        if(failAfterWrite)throw std::runtime_error("save/readback failure");
    }
    void Restore(GateValue before) { ++restores;value=before; }
};
template<class F> bool Throws(F action) {
    try { action();return false; } catch(const std::exception&) { return true; }
}
int main() {
    for(const GateValue before : {GateValue{true,false,1},GateValue{true,true,1},GateValue{false,false,0}}) {
        Store store;store.value=before;TestGateOverride<Store> gate(store);
        gate.Apply();assert(store.value==GateValue({true,true,0}));
        gate.Recover();assert(store.value==before && store.restores==1);
        gate.Recover();assert(store.restores==1);
        std::istringstream in(SerializeGateBackup(before));
        assert(ReadGateBackup(in)==before);
    }
    Store zero;zero.value={true,false,0};TestGateOverride<Store> unchanged(zero);
    unchanged.Apply();unchanged.Recover();assert(zero.writes==0 && zero.restores==0 && zero.backups==1);
    Store badBackup;badBackup.failBackup=true;TestGateOverride<Store> protectedGate(badBackup);
    assert(Throws([&]{protectedGate.Apply();}));protectedGate.Recover();assert(badBackup.writes==0);
    Store partial;partial.failAfterWrite=true;TestGateOverride<Store> partialGate(partial);
    assert(Throws([&]{partialGate.Apply();}));partialGate.Recover();
    assert(partial.value==GateValue({true,false,1}) && partial.restores==1);
    Store conflict;TestGateOverride<Store> conflictGate(conflict);conflictGate.Apply();
    conflict.value={true,true,2};assert(Throws([&]{conflictGate.Recover();}));assert(conflict.restores==0);
    Store recovered;recovered.value={true,true,0};TestGateOverride<Store> recovery(recovered,GateValue{true,false,1});
    recovery.Recover();assert(recovered.value==GateValue({true,false,1}));
    Store interrupted;interrupted.value={true,true,0};interrupted.pending=GateValue{false,false,0};
    RecoverPendingGate(interrupted);
    assert(!interrupted.pending && interrupted.value==GateValue({false,false,0}));
    Store changed;changed.value={true,true,3};changed.pending=GateValue{true,false,1};
    assert(Throws([&]{RecoverPendingGate(changed);}));assert(changed.pending && changed.restores==0);
    Store clearFailed;clearFailed.value={true,true,0};clearFailed.pending=GateValue{true,false,1};clearFailed.failClear=true;
    assert(Throws([&]{RecoverPendingGate(clearFailed);}));
    assert(clearFailed.pending && clearFailed.value==GateValue({true,false,1}));
    clearFailed.failClear=false;RecoverPendingGate(clearFailed);
    assert(!clearFailed.pending && clearFailed.restores==1);
    for(const auto* invalid : {"", "OTHER\n1 1 1\n", "VRSS-FRAME-GATE-BACKUP-1\n0 1 1\n",
        "VRSS-FRAME-GATE-BACKUP-1\n1 1 4294967296\n", "VRSS-FRAME-GATE-BACKUP-1\n1 0 1\nextra"}) {
        std::istringstream in(invalid);assert(!ReadGateBackup(in));
    }
    std::cout<<"FRAME_GATE_PASS backup_before_write restore_predefined_custom_absent partial_failure conflict crash_recovery\n";
}
