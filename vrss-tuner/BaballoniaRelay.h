#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <thread>

// Baballonia OSC is received on localhost and forwarded only to the opt-in
// common provider's normalized UDP input. No game-process access is involved.
namespace babble {
struct Reading { int eyeAxis{-1}; float value{}; };
inline Reading ParseOsc(const char* bytes, size_t length) {
    if (!bytes || length < 12 || length > 4096) return {};
    size_t addressEnd = 0;
    while (addressEnd < length && bytes[addressEnd]) ++addressEnd;
    if (addressEnd == length) return {};
    const size_t tagsAt = (addressEnd + 4) & ~size_t(3);
    if (tagsAt + 4 > length || bytes[tagsAt] != ',' || bytes[tagsAt + 1] != 'f' || bytes[tagsAt + 2]) return {};
    const size_t valueAt = (tagsAt + 4) & ~size_t(3);
    if (valueAt + 4 > length) return {};
    const char* address = bytes;
    constexpr const char* prefix = "/avatar/parameters/";
    if (std::strncmp(address, prefix, std::strlen(prefix)) == 0) address += std::strlen(prefix);
    if (*address == '/') ++address;
    int axis = -1;
    if (std::strcmp(address, "LeftEyeX") == 0) axis = 0;
    else if (std::strcmp(address, "LeftEyeY") == 0) axis = 1;
    else if (std::strcmp(address, "RightEyeX") == 0) axis = 2;
    else if (std::strcmp(address, "RightEyeY") == 0) axis = 3;
    uint32_t bits = (uint32_t(uint8_t(bytes[valueAt])) << 24) |
        (uint32_t(uint8_t(bytes[valueAt + 1])) << 16) |
        (uint32_t(uint8_t(bytes[valueAt + 2])) << 8) | uint8_t(bytes[valueAt + 3]);
    float value{}; std::memcpy(&value, &bits, sizeof(value));
    if (axis < 0 || !std::isfinite(value) || std::abs(value) > 1.0f) return {};
    return {axis, value};
}
struct PacketReadings { std::array<Reading, 16> items{}; size_t count{}; };
inline PacketReadings ParsePacket(const char* bytes, size_t length) {
    PacketReadings result{};
    if (!bytes || length < 12) return result;
    if (length >= 16 && std::memcmp(bytes, "#bundle\0", 8) == 0) {
        size_t at = 16; // OSC bundle header and timetag.
        while (at + 4 <= length && result.count < result.items.size()) {
            const size_t size = (size_t(uint8_t(bytes[at])) << 24) |
                (size_t(uint8_t(bytes[at + 1])) << 16) |
                (size_t(uint8_t(bytes[at + 2])) << 8) | uint8_t(bytes[at + 3]);
            at += 4;
            if (!size || size > length - at) break;
            auto reading = ParseOsc(bytes + at, size);
            if (reading.eyeAxis >= 0) result.items[result.count++] = reading;
            at += size;
        }
    } else {
        auto reading = ParseOsc(bytes, length);
        if (reading.eyeAxis >= 0) result.items[result.count++] = reading;
    }
    return result;
}
#pragma pack(push, 1)
struct UdpGazeV1 {
    uint32_t magic{0x3147564e}, version{1};
    uint64_t sequence{}, qpc{};
    float leftX{}, leftY{}, rightX{}, rightY{};
    uint32_t validMask{3};
};
#pragma pack(pop)
static_assert(sizeof(UdpGazeV1) == 44);

class Relay {
    SOCKET input_{INVALID_SOCKET}, output_{INVALID_SOCKET};
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> received_{0}, sent_{0};
    bool winsockReady_{};
    std::function<bool()> outputAllowed_;
    std::atomic<float> xGain_{1}, yGain_{1}, leftOffset_{0}, rightOffset_{0};
    int outputPort_{5556};
    int forwardPort_{};
    void Loop() {
        struct Slot { float value{}; uint64_t tick{}; };
        std::array<Slot, 4> slots{};
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(static_cast<u_short>(outputPort_));
        destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sockaddr_in forward = destination;
        forward.sin_port = htons(static_cast<u_short>(forwardPort_));
        uint64_t sequence = 0, checkedAt = 0;
        bool allowed = false;
        while (running_) {
            char bytes[4096];
            sockaddr_in sender{}; int senderSize = sizeof(sender);
            const int size = recvfrom(input_, bytes, sizeof(bytes), 0,
                reinterpret_cast<sockaddr*>(&sender), &senderSize);
            if (size <= 0 || sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) continue;
            if (forwardPort_)
                sendto(output_, bytes, size, 0, reinterpret_cast<const sockaddr*>(&forward), sizeof(forward));
            const auto readings = ParsePacket(bytes, size_t(size));
            for (size_t i = 0; i < readings.count; ++i) {
                const auto reading = readings.items[i];
                ++received_;
                const uint64_t now = GetTickCount64();
                slots[reading.eyeAxis] = {reading.value, now};
                bool complete = true;
                for (const auto& slot : slots) if (!slot.tick || now - slot.tick > 150) complete = false;
                if (!complete) continue;
                if (now - checkedAt >= 100) { allowed = outputAllowed_ && outputAllowed_(); checkedAt = now; }
                if (!allowed) continue;
                LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
                auto clamp = [](float v) { return std::fmax(-1.0f, std::fmin(1.0f, v)); };
                UdpGazeV1 packet{};
                packet.sequence = ++sequence;
                packet.qpc = uint64_t(qpc.QuadPart);
                packet.leftX = clamp(slots[0].value * xGain_.load() + leftOffset_.load() / 45.0f);
                packet.leftY = clamp(slots[1].value * yGain_.load());
                packet.rightX = clamp(slots[2].value * xGain_.load() + rightOffset_.load() / 45.0f);
                packet.rightY = clamp(slots[3].value * yGain_.load());
                if (sendto(output_, reinterpret_cast<const char*>(&packet), sizeof(packet), 0,
                    reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == sizeof(packet)) ++sent_;
            }
        }
    }
public:
    ~Relay() { Stop(); }
    bool Start(int port, float xGain, float yGain, float leftOffset, float rightOffset,
               std::function<bool()> outputAllowed, int outputPort = 5556, int forwardPort = 0) {
        if (running_ || port < 1 || port > 65535 || !std::isfinite(xGain) || !std::isfinite(yGain) ||
            !std::isfinite(leftOffset) || !std::isfinite(rightOffset) || outputPort < 1 || outputPort > 65535 ||
            forwardPort < 0 || forwardPort > 65535 || forwardPort == port) return false;
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa)) return false;
        winsockReady_ = true;
        input_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        output_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in address{};
        address.sin_family = AF_INET; address.sin_port = htons(static_cast<u_short>(port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        DWORD timeout = 100;
        bool ok = input_ != INVALID_SOCKET && output_ != INVALID_SOCKET &&
            setsockopt(input_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0 &&
            bind(input_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
        if (!ok) { Stop(); return false; }
        SetCalibration(xGain, yGain, leftOffset, rightOffset);
        outputPort_ = outputPort;
        forwardPort_ = forwardPort;
        outputAllowed_ = std::move(outputAllowed);
        received_ = sent_ = 0;
        running_ = true;
        worker_ = std::thread([this] { Loop(); });
        return true;
    }
    void Stop() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
        if (input_ != INVALID_SOCKET) { closesocket(input_); input_ = INVALID_SOCKET; }
        if (output_ != INVALID_SOCKET) { closesocket(output_); output_ = INVALID_SOCKET; }
        if (winsockReady_) { WSACleanup(); winsockReady_ = false; }
    }
    bool Running() const { return running_; }
    uint64_t Received() const { return received_; }
    uint64_t Sent() const { return sent_; }
    void SetCalibration(float xGain, float yGain, float leftOffset, float rightOffset) {
        xGain_ = xGain; yGain_ = yGain;
        leftOffset_ = leftOffset; rightOffset_ = rightOffset;
    }
};
} // namespace babble
