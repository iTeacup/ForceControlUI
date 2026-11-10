#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>

namespace nokov
{
constexpr uint32_t Device = 780;
constexpr uint32_t FrameMessage = 0x750, StatusMessage = 0x751, ControlMessage = 0x752;
constexpr uint32_t Magic = 0x564f4b4e, Version = 6, MaxMarkers = 4096;
// Local identifier only; never use this value as a measurement timestamp.
inline uint64_t NewLocalId()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
#pragma pack(push, 1)
constexpr size_t NameBytes = 256;
struct Marker
{
    int32_t id;
    float x, y, z;
    char asset[NameBytes] = {};
    char name[NameBytes] = {};
};
struct Header
{
    uint32_t magic = Magic, version = Version, count = 0;
    uint64_t session = 0, sequence = 0;
    int64_t timestamp = 0; // SDK iTimeStamp: milliseconds since 1970-01-01.
    int32_t frame = 0;
};
struct Status
{
    uint32_t version = Version, connected = 0;
    uint64_t overflow = 0, failed = 0, invalidFrames = 0, sdkGaps = 0;
    int32_t error = 0;
};
struct Control
{
    uint32_t version = Version, connect = 0;
};
#pragma pack(pop)
struct Frame
{
    Header header;
    std::vector<Marker> markers;
};
inline std::vector<char> Encode(const Frame& f)
{
    Header h = f.header;
    h.count = static_cast<uint32_t>(f.markers.size());
    std::vector<char> bytes(sizeof(h) + f.markers.size() * sizeof(Marker));
    memcpy(bytes.data(), &h, sizeof(h));
    if (h.count)
        memcpy(bytes.data() + sizeof(h), f.markers.data(), h.count * sizeof(Marker));
    return bytes;
}
inline bool Decode(const char* data, uint32_t length, Frame& f)
{
    if (!data || length < sizeof(Header))
        return false;
    Header h;
    memcpy(&h, data, sizeof(h));
    if (h.magic != Magic || h.version != Version || h.count > MaxMarkers ||
        length != sizeof(Header) + static_cast<size_t>(h.count) * sizeof(Marker))
        return false;
    f.header = h;
    f.markers.resize(h.count);
    if (h.count)
        memcpy(f.markers.data(), data + sizeof(h), h.count * sizeof(Marker));
    for (const auto& marker : f.markers)
        if (!memchr(marker.asset, 0, NameBytes) || !memchr(marker.name, 0, NameBytes))
            return false;
    return true;
}
} // namespace nokov
