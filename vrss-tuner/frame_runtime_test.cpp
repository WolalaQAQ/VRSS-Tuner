#include "FrameStartup.h"
#include "../common-vrss-provider/FrameGaze.h"
#include "FrameProcess.h"
#include "../common-vrss-provider/HmdIdentity.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using namespace frame;
    using hmd::Profile;
    using hmd::Property;
    const auto missing = Property{hmd::PropertyState::Missing,0};
    const auto error = Property{hmd::PropertyState::Error,0};
    const auto vid = Property{hmd::PropertyState::Available,123};
    const auto pid = Property{hmd::PropertyState::Available,456};
    auto identity = hmd::SelectIdentity(Profile::SteamFrame,"Steam Frame","Valve",missing,missing);
    assert(identity && identity->vendor==0 && identity->product==0 && identity->unspecified);
    const auto frameAlias=hmd::SelectIdentity(Profile::SteamFrame,"Deckard MP","Valve",missing,missing);
    if(!frameAlias) {std::cerr<<"FRAME_MODEL_ALIAS_REJECTED Deckard MP\n";return 2;}
    assert(frameAlias->unspecified && frameAlias->vendor==0 && frameAlias->product==0);
    assert(!hmd::SelectIdentity(Profile::SteamFrame,"Deckard MP","Other",missing,missing));
    assert(!hmd::SelectIdentity(Profile::SteamFrame,"Deckard unknown","Valve",missing,missing));
    identity = hmd::SelectIdentity(Profile::Standard,"Another HMD","Vendor",vid,pid);
    assert(identity && identity->vendor==123 && identity->product==456 && !identity->unspecified);
    identity = hmd::SelectIdentity(Profile::SteamFrame,"Steam Frame","Valve",vid,pid);
    assert(identity && !identity->unspecified && identity->vendor==123);
    assert(!hmd::SelectIdentity(Profile::Standard,"Steam Frame","Valve",missing,missing));
    assert(!hmd::SelectIdentity(Profile::SteamFrame,"Other HMD","Valve",missing,missing));
    assert(!hmd::SelectIdentity(Profile::SteamFrame,"Steam Frame","Other",missing,missing));
    assert(!hmd::SelectIdentity(Profile::SteamFrame,"Steam Frame","Valve",error,missing));
    assert(!hmd::SelectIdentity(Profile::SteamFrame,"Steam Frame","Valve",missing,pid));
    for(int invalid : {-1,0,65536})
        assert(!hmd::SelectIdentity(Profile::SteamFrame,"Steam Frame","Valve",{hmd::PropertyState::Available,invalid},pid));
    assert(hmd::ParseProfile(L"steam-frame")==Profile::SteamFrame);
    assert(hmd::ParseProfile(L"standard")==Profile::Standard);
    assert(!hmd::ParseProfile(L"unknown"));
    assert(!hmd::CanRegister({0,0,true}));
    assert(hmd::CanRegister({123,456,false}));
    assert(!hmd::CanRegister({0,456,false}));
    assert(!hmd::CanRegister({123,65536,false}));
    const auto session=hmd::SelectSessionIdentity(Profile::SteamFrame,{0,0,true},true,true);
    assert(session && session->vendor==0xffff && session->product==0xffff);
    assert(hmd::CanRegister(*session));
    assert(!hmd::SelectSessionIdentity(Profile::SteamFrame,{0,0,true},false,true));
    assert(!hmd::SelectSessionIdentity(Profile::SteamFrame,{0,0,true},true,false));
    assert(!hmd::SelectSessionIdentity(Profile::Standard,{0,0,true},true,true));
    assert(!hmd::SelectSessionIdentity(Profile::SteamFrame,{123,456,false},true,true));
    assert(!hmd::SelectSessionIdentity(Profile::SteamFrame,{123,0,true},true,true));
    assert(std::string_view(DeliveryState(true,0,0,0))=="waiting-consumer");
    assert(std::string_view(DeliveryState(false,0,1,0))=="waiting-gaze");
    assert(std::string_view(DeliveryState(true,0,1,0))=="submitting");
    assert(std::string_view(DeliveryState(true,-1,1,0))=="driver-error");
    assert(std::string_view(DeliveryState(true,0,1,-1))=="driver-error");
    assert(ShouldSubmit(true,0,1,1,0,100,0));
    assert(!ShouldSubmit(false,0,1,1,0,100,0));
    assert(!ShouldSubmit(true,-1,1,1,0,100,0));
    assert(!ShouldSubmit(true,0,0,1,0,100,0));
    assert(!ShouldSubmit(true,0,1,1,1,100,100));
    assert(!ShouldSubmit(true,0,1,2,1,99,100));
    Startup state;
    Environment env;
    assert(state.Next(env) == Action::WaitSteamVr);
    env.steamVr = true; env.vrchat = true;
    assert(state.Next(env) == Action::WaitVrchat);
    env.vrchat = false; env.conflict = true;
    assert(state.Next(env) == Action::Conflict);
    env.conflict = false;
    assert(state.Next(env) == Action::Launch);
    assert(state.Next(env) == Action::Starting); // One launch, not one UAC prompt per timer tick.
    state.LaunchFinished(false);
    for (int i=0;i<20;++i) assert(state.Next(env) == Action::Failed);
    state.Retry(); assert(state.Next(env) == Action::Launch);
    state.LaunchFinished(true); env.workerAlive = true; env.vrchat = true;
    assert(state.Next(env) == Action::Running);
    state.Stop(); assert(state.Next(env) == Action::Stopped);
    env.workerAlive = false; state.Retry(); env.vrchat = false;
    assert(state.Next(env) == Action::Launch); state.LaunchFinished(true);
    assert(state.Next(env) == Action::Failed); // A dead worker is never reported as active.

    Gain gain; gain.x = gain.y = 1;
    std::array<float,4> output{9,9,9,9};
    assert(MapNdc(true, {0,0,.4f,-.3f}, gain, output));
    assert(output[0]==0 && output[1]==0 && output[2]==.4f && output[3]==-.3f);
    assert(MapNdc(true, {-1,1,1,-1}, gain, output));
    assert((output == std::array<float,4>{-1,1,1,-1}));
    assert(MapNdc(true,{.2f,.3f,-.2f,-.3f},gain,output,true));
    assert((output==std::array<float,4>{.2f,-.3f,-.2f,.3f}));
    auto old = output;
    assert(!MapNdc(false, {0,0,0,0}, gain, output) && output==old);
    assert(!MapNdc(true, {0,0,std::numeric_limits<float>::quiet_NaN(),0}, gain, output));
    assert(!MapNdc(true, {0,0,0,std::numeric_limits<float>::infinity()}, gain, output));
    gain.x=.5; gain.y=-1; gain.leftDegrees=4.5; gain.rightDegrees=-4.5;
    assert(MapNdc(true, {.4f,.3f,-.4f,-.3f}, gain, output));
    assert(std::abs(output[0]-.3f)<1e-6 && std::abs(output[2]+.3f)<1e-6);
    assert(output[1]==-.3f && output[3]==.3f);
    assert(MapNdc(true, {9,-9,9,-9}, gain, output));
    assert((output==std::array<float,4>{1,1,1,1}));
    assert(ValidRunId(L"1234-56789"));
    for(const auto* value : {L"", L"../escape", L"x", L"/123", L"1:2", L"1\"2"})
        assert(!ValidRunId(value));
    const auto created=ProcessCreated(GetCurrentProcess());
    assert(created!=0);
    auto owner=OpenOwner(GetCurrentProcessId(),created);
    assert(owner.get() && WaitForSingleObject(owner.get(),0)==WAIT_TIMEOUT);
    assert(!OpenOwner(GetCurrentProcessId(),created+1).get());
    assert(!OpenOwner(0,created).get());
    assert(StatusFresh(50000000,40000000));
    assert(!StatusFresh(50000000,10000000));
    assert(!StatusFresh(50000000,60000000));
    assert(!StatusFresh(50000000,0));
    std::cout << "FRAME_RUNTIME_PASS auto_start wait conflict cancel no_repeat stop worker_death ndc both_eyes invalid gain bounds owner_identity run_path fresh_only frame_y_direction\n";
}
