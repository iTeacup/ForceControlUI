#pragma once
#include <NokovSDKTypes.h>
#include <CPSNokovDef.h>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <windows.h>

class MarkerNames
{
public:
    std::map<std::string, std::vector<std::string>> definitions;
    static std::string Utf8(const char* text)
    {
        if (!text)
            return {};
        int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
        if (size)
            return text;
        size = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
        if (!size)
            return {};
        std::vector<wchar_t> wide(size);
        MultiByteToWideChar(CP_ACP, 0, text, -1, wide.data(), size);
        int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, nullptr, 0, nullptr, nullptr);
        if (!bytes)
            return {};
        std::vector<char> result(bytes);
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, result.data(), bytes, nullptr, nullptr);
        return result.data();
    }
    void Load(const sDataDescriptions* descriptions)
    {
        definitions.clear();
        if (!descriptions || descriptions->nDataDescriptions < 0 || descriptions->nDataDescriptions > MAX_MODELS)
            return;
        for (int i = 0; i < descriptions->nDataDescriptions; ++i)
        {
            const auto& d = descriptions->arrDataDescriptions[i];
            if (d.type != Descriptor_MarkerSet || !d.Data.MarkerSetDescription)
                continue;
            const auto& set = *d.Data.MarkerSetDescription;
            if (set.nMarkers < 0 || set.nMarkers > static_cast<int>(nokov::MaxMarkers) ||
                (set.nMarkers && !set.szMarkerNames))
                continue;
            std::vector<std::string> names;
            for (int j = 0; j < set.nMarkers; ++j)
                names.push_back(Utf8(set.szMarkerNames[j]));
            const auto asset = Utf8(set.szName);
            if (definitions.count(asset))
                definitions[asset].clear();
            else
                definitions.emplace(asset, std::move(names));
        }
    }
    void Resolve(const sFrameOfMocapData& source, std::vector<nokov::Marker>& markers) const
    {
        for (auto& marker : markers)
            marker.asset[0] = marker.name[0] = 0;
        struct Named
        {
            std::string asset, name;
            float x, y, z;
        };
        std::vector<Named> points;
        if (source.nMarkerSets < 0 || source.nMarkerSets > MAX_MODELS)
            return;
        for (int i = 0; i < source.nMarkerSets; ++i)
        {
            const auto& set = source.MocapData[i];
            const auto asset = Utf8(set.szName);
            auto found = definitions.find(asset);
            if (found == definitions.end() || set.nMarkers < 0 ||
                static_cast<size_t>(set.nMarkers) != found->second.size() || (set.nMarkers && !set.Markers))
                continue;
            for (int j = 0; j < set.nMarkers; ++j)
            {
                const auto& xyz = set.Markers[j];
                const auto& name = found->second[j];
                if (asset.empty() || name.empty() || asset.size() >= nokov::NameBytes ||
                    name.size() >= nokov::NameBytes)
                    continue;
                if (!std::isfinite(xyz[0]) || !std::isfinite(xyz[1]) || !std::isfinite(xyz[2]))
                    continue;
                // Some streams use all-zero named coordinates for absent points.
                if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0)
                    continue;
                points.push_back({asset, name, xyz[0], xyz[1], xyz[2]});
            }
        }
        std::vector<int> candidate(markers.size(), -1), usage(points.size(), 0);
        for (size_t i = 0; i < markers.size(); ++i)
        {
            auto& marker = markers[i];
            for (size_t j = 0; j < points.size(); ++j)
            {
                const auto& p = points[j];
                if (marker.x != p.x || marker.y != p.y || marker.z != p.z)
                    continue;
                ++usage[j];
                candidate[i] = candidate[i] == -1 ? static_cast<int>(j) : -2;
            }
        }
        for (size_t i = 0; i < markers.size(); ++i)
        {
            int j = candidate[i];
            if (j < 0 || usage[j] != 1)
                continue;
            const auto& p = points[j];
            // Duplicate asset/point names are also ambiguous.
            size_t sameName = 0;
            for (const auto& other : points)
                if (other.asset == p.asset && other.name == p.name)
                    ++sameName;
            if (sameName != 1)
                continue;
            memcpy(markers[i].asset, p.asset.c_str(), p.asset.size() + 1);
            memcpy(markers[i].name, p.name.c_str(), p.name.size() + 1);
        }
    }
};
