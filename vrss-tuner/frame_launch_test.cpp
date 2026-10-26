#include "FrameGameLaunch.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace frame;
    constexpr auto name=L"VRSS_TUNER_TEST_PROCESS_ENV";
    const auto original=ReadProcessEnvironment(name);
    SetEnvironmentVariableW(name,nullptr);
    {
        ScopedProcessEnvironment test({{name,L"first"}});
        assert(ReadProcessEnvironment(name)==L"first");
        assert(test.Restore() && !ReadProcessEnvironment(name));
    }
    SetEnvironmentVariableW(name,L"before");
    try {
        ScopedProcessEnvironment test({{name,L"during"}});
        assert(ReadProcessEnvironment(name)==L"during");
        throw std::runtime_error("launch failure");
    } catch(const std::runtime_error&) {}
    assert(ReadProcessEnvironment(name)==L"before");
    bool rejected=false;
    try { ScopedProcessEnvironment test({{name,L"partial"},{L"INVALID=NAME",L"x"}}); }
    catch(const std::exception&) {rejected=true;}
    assert(rejected && ReadProcessEnvironment(name)==L"before");
    SetEnvironmentVariableW(name,original?original->c_str():nullptr);
    wchar_t own[32768]{};GetModuleFileNameW(nullptr,own,32768);
    assert(!IsTrustedEacLauncher(own));
    assert(!LaunchStatusReady(100,101,"{\"pid\":100,\"registered\":true,\"identityMode\":\"frame-experimental\",\"queryStatus\":0}"));
    const std::string ready="{\"pid\":100,\"registered\":true,\"identityMode\":\"frame-experimental\",\"queryStatus\":0}";
    assert(LaunchStatusReady(100,100,ready));
    assert(!LaunchStatusReady(10,10,ready));
    assert(!LaunchStatusReady(100,100,"{\"pid\":100,\"registered\":false}"));
    assert(!LaunchStatusReady(100,100,"{\"pid\":100,\"registered\":true,\"identityMode\":\"synthetic-test\",\"queryStatus\":0}"));
    assert(!LaunchStatusReady(100,100,"{\"pid\":100,\"registered\":true,\"identityMode\":\"frame-experimental\",\"queryStatus\":-1}"));
    std::cout<<"FRAME_LAUNCH_PASS process_environment rollback exception unsigned_rejection session_identity\n";
}
