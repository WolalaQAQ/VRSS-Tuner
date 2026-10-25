#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include "private_contract.h"
#include "projection.h"
#include "gaze_state.h"
#include "config_io.h"
#include <atomic>
#include <filesystem>
#include <map>
#include <thread>
#pragma comment(lib,"ws2_32.lib")
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static std::atomic<bool> running{true};
static BOOL WINAPI Control(DWORD) { running=false; return TRUE; }
static std::map<std::string,std::string> ReadFields(const fs::path& path) {
    return ReadSharedConfig(path);
}
static std::vector<double> Numbers(std::string text,size_t expected) {
    std::replace(text.begin(),text.end(),',',' '); std::istringstream in(text);
    std::vector<double> out; double value;
    while(in>>value) { if(!std::isfinite(value)) throw std::runtime_error("nonfinite config"); out.push_back(value); }
    if(out.size()!=expected) throw std::runtime_error("Wrong geometry field length");
    return out;
}
static Gain ReadGain(const fs::path& path) {
    auto config=ReadFields(path); Gain gain;
    gain.x=std::stod(config.at("x_gain")); gain.y=std::stod(config.at("y_gain"));
    gain.leftDegrees=std::stod(config.at("left_deg"));gain.rightDegrees=std::stod(config.at("right_deg"));
    if(!std::isfinite(gain.x)||!std::isfinite(gain.y)||gain.x<.1||gain.x>2||
       std::abs(gain.y)<.1||std::abs(gain.y)>2||!std::isfinite(gain.leftDegrees)||
       !std::isfinite(gain.rightDegrees)||std::abs(gain.leftDegrees)>5||std::abs(gain.rightDegrees)>5)
        throw std::runtime_error("Invalid input gain config");
    return gain;
}
#pragma pack(push,1)
struct UdpGazeV1 {
    uint32_t magic,version;
    uint64_t sequence,qpc;
    float leftX,leftY,rightX,rightY;
    uint32_t validMask;
};
#pragma pack(pop)
static_assert(sizeof(UdpGazeV1)==44);
class Input {
    GazeState state_; std::mutex mutex_; UdpGazeV1 udp_{};
    std::thread reader_; std::atomic<bool> stop_{false};
    std::atomic<uint64_t> sequence_{0};
    bool udpMode_; int port_;
    static bool Exact(SOCKET socket,char* data,size_t size) {
        while(size && running) { int n=recv(socket,data,int(size),0);if(n<=0)return false;data+=n;size-=n; }
        return size==0;
    }
    void Tcp() {
        while(!stop_ && running) {
            SOCKET socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); if(socket==INVALID_SOCKET) break;
            DWORD timeout=250;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
            sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(static_cast<u_short>(port_));
            address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
            bool connected=connect(socket,(sockaddr*)&address,sizeof(address))==0;
            const char request=0;
            if(connected) connected=send(socket,&request,1,0)==1;
            while(connected && !stop_ && running) {
                char header[5];if(!Exact(socket,header,5)||header[0]!=0)break;
                uint32_t length=0;std::memcpy(&length,header+1,4);
                if(!length||length>4096)break;
                std::string body(length,'\0');if(!Exact(socket,body.data(),length))break;
                state_.publish_frame(body,Clock::now());++sequence_;
            }
            closesocket(socket);state_.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    void Udp() {
        SOCKET socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socket==INVALID_SOCKET)return;
        DWORD timeout=100;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(static_cast<u_short>(port_));
        address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(socket,(sockaddr*)&address,sizeof(address))) {closesocket(socket);return;}
        while(!stop_ && running) {
            UdpGazeV1 packet{};
            int n=recv(socket,(char*)&packet,sizeof(packet),0);
            if(n!=sizeof(packet)||packet.magic!=0x3147564e||packet.version!=1||packet.validMask!=3)continue;
            bool good=true;
            for(float f:{packet.leftX,packet.leftY,packet.rightX,packet.rightY}) if(!std::isfinite(f)||std::abs(f)>1)good=false;
            if(!good)continue;
            std::lock_guard<std::mutex> lock(mutex_);
            if(packet.sequence>udp_.sequence && packet.qpc>udp_.qpc) {udp_=packet;sequence_=packet.sequence;}
        }
        closesocket(socket);
    }
public:
    Input(bool udpMode,int port):udpMode_(udpMode),port_(port) {
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("WSAStartup failed");
        reader_=std::thread([this]{if(udpMode_)Udp();else Tcp();});
    }
    ~Input(){stop_=true;if(reader_.joinable())reader_.join();WSACleanup();}
    bool Read(StereoGaze& raw,std::array<float,4>& normalized,uint64_t& qpc,uint64_t& sequence,double& age) {
        LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
        sequence=sequence_;
        if(udpMode_) {
            std::lock_guard<std::mutex> lock(mutex_);
            age=double(now.QuadPart-int64_t(udp_.qpc))/frequency.QuadPart;
            if(!udp_.qpc||age<0||age>.15)return false;
            normalized={udp_.leftX,udp_.leftY,udp_.rightX,udp_.rightY};qpc=udp_.qpc;sequence=udp_.sequence;
            return true;
        }
        if(!state_.read_fresh(Clock::now(),raw))return false;
        age=raw.max_age_seconds;qpc=uint64_t(now.QuadPart-int64_t(age*frequency.QuadPart));
        return true;
    }
};
struct NvidiaSession {
    ID3D11Device* device=nullptr; ID3D11DeviceContext* context=nullptr; ID3DNvGazeHandler* gaze=nullptr;
    UnregisterProvider unregisterFn=nullptr;bool registered=false;
    ~NvidiaSession() {
        if(registered && unregisterFn) std::cout<<"unregister="<<unregisterFn(gaze,context)<<'\n';
        if(gaze)gaze->Release();if(context)context->Release();if(device)device->Release();NvAPI_Unload();
    }
};
struct ConsumerQuery {uint32_t version=0x10028,pad=0;uint64_t count=0,a=0,b=0,c=0;};
static_assert(sizeof(ConsumerQuery)==40);
using QueryConsumer=int(__fastcall*)(void*,void*,ConsumerQuery*);
using SetupListener=int(__fastcall*)(void*,void*,HANDLE*);
int wmain(int argc,wchar_t** argv) {
    std::cout<<std::unitbuf;
    try {
        std::map<std::wstring,std::wstring> args;
        for(int i=1;i+1<argc;i+=2) args[argv[i]]=argv[i+1];
        if(args.count(L"--driver-check-only") && args.at(L"--driver-check-only")==L"1") {
            NvidiaSession nv;D3D_FEATURE_LEVEL level{};
            HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&nv.device,&level,&nv.context);
            if(FAILED(hr)||NvAPI_Initialize()!=0)throw std::runtime_error("NVIDIA/D3D initialization failed");
            if(HashModule(L"nvapi64_impl.dll")!="df6455ccf83e43cfe68f405af1eec4e053c7f95da998bf358053b7583980c2f4" ||
               HashModule(L"nvwgf2umx.dll")!="04c01cfc849e5b58ad50da89dc83bff15f15847684de7964ff5900fedfab18fe")
                throw std::runtime_error("Unsupported driver image: private ABI refused");
            std::cout<<"DRIVER_CHECK_PASS exact audited images\n";
            return 0;
        }
        const auto geometry=ReadFields(fs::path(args.at(L"--geometry")));
        EyeGeometry eyes[2];
        for(int eye=0;eye<2;++eye) {
            auto p=Numbers(geometry.at("projection"+std::to_string(eye)),4);
            auto r=Numbers(geometry.at("eyeToHead"+std::to_string(eye)),9);
            std::copy(p.begin(),p.end(),eyes[eye].projection.begin());std::copy(r.begin(),r.end(),eyes[eye].eyeToHead.begin());
            double det=r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])+r[2]*(r[3]*r[7]-r[4]*r[6]);
            if(std::abs(det-1)>.01)throw std::runtime_error("Invalid eye rotation");
        }
        const uint32_t vendor=std::stoul(geometry.at("vendor")),product=std::stoul(geometry.at("product"));
        if(!vendor||!product||vendor>65535||product>65535)throw std::runtime_error("Invalid HMD EDID");
        const fs::path output=args.at(L"--output"),config=args.at(L"--config");
        const bool checkOnly=args.count(L"--check-only") && args.at(L"--check-only")==L"1";
        Gain gain=ReadGain(config);
        if(checkOnly) { std::cout<<"GEOMETRY_CONFIG_PASS vendor="<<vendor<<" product="<<product<<'\n';return 0; }
        if(!AdminEnabled())throw std::runtime_error("Enabled Administrators token required");
        std::wstring event=L"Global\\nfdv8Dup8UqK_JnJG2fkew:"+std::to_wstring(vendor)+L":"+std::to_wstring(product)+L"-evt";
        if(!EventAbsent(event))throw std::runtime_error("An existing provider bridge is present or inaccessible");
        fs::create_directories(output);
        std::ofstream csv(output/"gaze.csv");
        csv<<"qpc,sourceSequence,fresh,ageSeconds,consumerCount,queryStatus,submitted,submitStatus,leftX,leftY,rightX,rightY\n";
        PrintToken();NvidiaSession nv;D3D_FEATURE_LEVEL level{};
        HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&nv.device,&level,&nv.context);
        if(FAILED(hr)||NvAPI_Initialize()!=0)throw std::runtime_error("NVIDIA/D3D initialization failed");
        NV_GAZE_HANDLER_INIT_PARAMS init{};init.version=NV_GAZE_HANDLER_INIT_PARAMS_VER;
        init.GazeDataDeviceId=0;init.GazeDataType=NV_GAZE_DATA_STEREO;
        const auto& p=eyes[0].projection;
        init.fHorizontalFOV=float((std::atan(-p[0])+std::atan(p[1]))*180/3.141592653589793);
        init.fVericalFOV=float((std::atan(-p[2])+std::atan(p[3]))*180/3.141592653589793);
        init.ppNvGazeHandler=&nv.gaze;
        int status=NvAPI_D3D_InitializeNvGazeHandler(nv.device,&init);
        if(status||!nv.gaze)throw std::runtime_error("Gaze handler initialization failed");
        if(HashModule(L"nvapi64_impl.dll")!="df6455ccf83e43cfe68f405af1eec4e053c7f95da998bf358053b7583980c2f4" ||
           HashModule(L"nvwgf2umx.dll")!="04c01cfc849e5b58ad50da89dc83bff15f15847684de7964ff5900fedfab18fe")
            throw std::runtime_error("Unsupported driver image: private ABI refused");
        auto table=*reinterpret_cast<uintptr_t**>(nv.gaze);
        const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(L"nvapi64_impl.dll"));
        const std::array<uintptr_t,8> expected={0x2fcc80,0x328040,0x3280a0,0x327440,0x327bd0,0x326b80,0x3272e0,0x325f70};
        for(size_t i=0;i<8;++i)if(table[i]!=base+expected[i])throw std::runtime_error("Private vtable mismatch");
        Challenge challenge{};auto get=reinterpret_cast<GetChallenge>(table[3]);
        status=get(nv.gaze,nv.context,&challenge);if(status)throw std::runtime_error("Challenge failed");
        Registration request{};request.vendor=vendor;request.product=product;
        for(int i=0;i<16;++i)request.response[i]=challenge.bytes[i]^kMask[i];
        nv.unregisterFn=reinterpret_cast<UnregisterProvider>(table[7]);
        status=reinterpret_cast<RegisterProvider>(table[4])(nv.gaze,nv.context,&request);
        std::cout<<"register="<<status<<" vendor="<<vendor<<" product="<<product<<'\n';
        if(status)throw std::runtime_error("Provider registration failed");nv.registered=true;
        HANDLE listener=nullptr;
        status=reinterpret_cast<SetupListener>(table[6])(nv.gaze,nv.context,&listener);
        std::cout<<"listener="<<status<<" handlePresent="<<(listener!=nullptr)<<'\n';
        if(status||!listener)throw std::runtime_error("Consumer listener setup failed");
        auto query=reinterpret_cast<QueryConsumer>(table[5]);
        const bool udp=args.count(L"--source")&&args.at(L"--source")==L"udp";
        int port=args.count(L"--port")?std::stoi(args.at(L"--port")):(udp?5556:5555);
        if(port<1||port>65535)throw std::runtime_error("Invalid source port");
        Input input(udp,port);SetConsoleCtrlHandler(Control,TRUE);
        const double seconds=args.count(L"--seconds")?std::stod(args.at(L"--seconds")):1200;
        if(seconds<=0||seconds>3600)throw std::runtime_error("Duration must be 1..3600 seconds");
        const auto started=Clock::now();auto next=started,nextStatus=started,nextConfig=started;
        uint64_t sent=0,freshCount=0,activeCount=0,failed=0,lastSequence=0,lastQpc=0;
        while(running && std::chrono::duration<double>(Clock::now()-started).count()<seconds) {
            if(fs::exists(output/"stop.request"))break;
            auto now=Clock::now();
            if(now>=nextConfig) {
                try { gain=ReadGain(config); }
                catch(const std::exception& e) { std::cerr<<"CONFIG_RELOAD_REJECTED retaining_last_good: "<<e.what()<<'\n'; }
                nextConfig=now+std::chrono::milliseconds(250);
            }
            ConsumerQuery consumer{};const int queried=query(nv.gaze,nv.context,&consumer);
            StereoGaze raw{};std::array<float,4> location{};uint64_t qpc=0,seq=0;double age=-1;
            bool fresh=input.Read(raw,location,qpc,seq,age);
            if(fresh&&!udp) {
                auto left=ProjectHeadTangent(raw.left.tan_x,raw.left.tan_y,eyes[0],gain,0);
                auto right=ProjectHeadTangent(raw.right.tan_x,raw.right.tan_y,eyes[1],gain,1);
                location={left[0],left[1],right[0],right[1]};
            }
            if(fresh)++freshCount;
            if(consumer.count)++activeCount;
            bool submitted=false;int updated=0;
            if(fresh&&queried==0&&consumer.count>0&&seq!=lastSequence&&qpc>lastQpc) {
                NV_FOVEATED_RENDERING_UPDATE_GAZE_DATA_PARAMS sample{};
                sample.version=NV_FOVEATED_RENDERING_UPDATE_GAZE_DATA_PARAMS_VER;sample.Timestamp=qpc;
                auto& l=sample.sStereoData.sLeftEye;auto& r=sample.sStereoData.sRightEye;
                l.version=r.version=NV_FOVEATED_RENDERING_GAZE_DATA_PER_EYE_VER;
                l.GazeDataValidityFlags=r.GazeDataValidityFlags=NV_GAZE_LOCATION_VALID;
                l.fGazeNormalizedLocation[0]=location[0];l.fGazeNormalizedLocation[1]=location[1];
                r.fGazeNormalizedLocation[0]=location[2];r.fGazeNormalizedLocation[1]=location[3];
                updated=nv.gaze->UpdateGazeData(nv.context,&sample);submitted=true;++sent;
                if(updated)++failed;lastSequence=seq;lastQpc=qpc;
            }
            LARGE_INTEGER host{};QueryPerformanceCounter(&host);
            csv<<host.QuadPart<<','<<seq<<','<<fresh<<','<<std::setprecision(9)<<age<<','<<consumer.count<<','<<queried<<','<<submitted<<','<<updated;
            for(float f:location)csv<<','<<f;csv<<'\n';
            if(now>=nextStatus) {
                std::ofstream state(output/"status.tmp");
                state<<"{\"pid\":"<<GetCurrentProcessId()<<",\"registered\":true,\"fresh\":"<<(fresh?"true":"false")
                     <<",\"consumerCount\":"<<consumer.count<<",\"queryStatus\":"<<queried<<",\"sent\":"<<sent
                     <<",\"submitFailures\":"<<failed<<",\"sourceSequence\":"<<seq<<",\"locations\":[";
                for(int i=0;i<4;++i)state<<(i?",":"")<<location[i];
                state<<"],\"xGain\":"<<gain.x<<",\"yGain\":"<<gain.y<<",\"source\":\""<<(udp?"normalized-udp":"brokeneye-tcp")<<"\"}";
                state.close();MoveFileExW((output/"status.tmp").c_str(),(output/"status.json").c_str(),MOVEFILE_REPLACE_EXISTING);
                csv.flush();std::cout<<"fresh="<<fresh<<" consumer="<<consumer.count<<" sent="<<sent<<" failures="<<failed<<'\n';
                nextStatus=now+std::chrono::seconds(1);
            }
            next+=std::chrono::milliseconds(10);if(next<Clock::now())next=Clock::now();std::this_thread::sleep_until(next);
        }
        std::cout<<"STOP sent="<<sent<<" freshPolls="<<freshCount<<" consumerPolls="<<activeCount<<" failures="<<failed<<'\n';
        return failed?12:0;
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<'\n';return 1; }
}
