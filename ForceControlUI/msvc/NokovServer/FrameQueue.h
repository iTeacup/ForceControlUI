#pragma once
#include <CPSNokovDef.h>
#include <NokovSDKTypes.h>
#include <deque>
#include "MarkerNames.h"
#include <mutex>
// SDK callback copies only the required fields. No file or bus I/O here.
class FrameQueue
{
public:
    std::mutex mutex;
    std::deque<nokov::Frame> frames;
    nokov::Status status;
    MarkerNames names;
    size_t capacity = 2048;
    uint64_t session = 0, sequence = 0;
    int previousFrame = 0;
    bool haveFrame = false;
    void Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        frames.clear();
        sequence = 0;
        haveFrame = false;
        session = nokov::NewLocalId();
    }
    static void Callback(sFrameOfMocapData* source, void* context)
    {
        if (!source)
            return;
        auto& self = *static_cast<FrameQueue*>(context);
        std::lock_guard<std::mutex> lock(self.mutex);
        if (self.haveFrame && static_cast<int64_t>(source->iFrame) != static_cast<int64_t>(self.previousFrame) + 1)
            ++self.status.sdkGaps;
        self.previousFrame = source->iFrame;
        self.haveFrame = true;
        if (source->nLabeledMarkers < 0 || source->nLabeledMarkers > MAX_LABELED_MARKERS ||
            source->nLabeledMarkers > nokov::MaxMarkers)
        {
            ++self.status.invalidFrames;
            return;
        }
        const auto sequence = ++self.sequence;
        if (self.frames.size() >= self.capacity)
        {
            ++self.status.overflow;
            return;
        }
        nokov::Frame frame;
        frame.header.session = self.session;
        frame.header.sequence = sequence;
        frame.header.frame = source->iFrame;
        frame.header.timestamp = source->iTimeStamp;
        frame.markers.reserve(source->nLabeledMarkers);
        for (int i = 0; i < source->nLabeledMarkers; ++i)
        {
            const auto& m = source->LabeledMarkers[i];
            frame.markers.push_back({m.ID, m.x, m.y, m.z});
        }
        self.names.Resolve(*source, frame.markers);
        self.frames.push_back(std::move(frame));
    }
};
