/*
 * Memory-reactive effect
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <math.h>
#include "lib/effect.h"
#include "visualmemory.h"


class ReactEffect : public Effect
{
public:
    ReactEffect(VisualMemory *mem)
        : mem(mem), phase(0)
    {}

    VisualMemory *mem;
    float phase;

    static const VisualMemory::memory_t kLimitFilterGain = 0.1;

    // Upper and lower limits seen while walking the recall buffer
    VisualMemory::Cell recallMin, recallMax;
    VisualMemory::Cell filteredRecallMin, filteredRecallMax;

    virtual void beginFrame(const FrameInfo &f)
    {
        // Reset limits for each frame
        recallMin = recallMax = mem->recall()[0];

        phase = fmodf(phase + f.timeDelta * 0.001, 2 * M_PI);
    }

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        const VisualMemory::Cell &recall = mem->recall()[p.index];
        VisualMemory::Cell normalized;
        normalized.shortTerm = std::max(0.0, std::min(1.0, (recall.shortTerm - filteredRecallMin.shortTerm)
                / (recallMax.shortTerm - recallMin.shortTerm)));
        normalized.longTerm = std::max(0.0, std::min(1.0, (recall.longTerm - filteredRecallMin.longTerm)
                / (recallMax.longTerm - recallMin.shortTerm)));

        float hue = 0;
        //float sat = std::min(1.0f, powf(std::max(0.0, normalized.longTerm), 4.0));
        float sat = std::min(1.0f, powf(std::max(0.0, normalized.longTerm), 10.0f));

        // Asymmetric sine thing
        float dist = len(p.point);
        float angle = atan2(p.point[2], p.point[0]);
        float value = 0.5 + 0.2 * sinf(4999.0f * phase + 2.0f * dist + 4.0f * sinf(879.0f * phase) * angle);

        hsv2rgb(rgb, hue, sat, value);
    }

    inline void postProcess(const Vec3& rgb, const PixelInfo& p)
    {
        // Update recall limits
        const VisualMemory::Cell &recall = mem->recall()[p.index];
        recallMin.shortTerm = std::min(recallMin.shortTerm, recall.shortTerm);
        recallMin.longTerm = std::min(recallMin.longTerm, recall.longTerm);
        recallMax.shortTerm = std::max(recallMax.shortTerm, recall.shortTerm);
        recallMax.longTerm = std::max(recallMax.longTerm, recall.longTerm);
    }

    virtual void endFrame(const FrameInfo &f)
    {
        // Filter the recall limits
        filteredRecallMin.shortTerm += (recallMin.shortTerm - filteredRecallMin.shortTerm) * kLimitFilterGain;
        filteredRecallMin.longTerm += (recallMin.longTerm - filteredRecallMin.longTerm) * kLimitFilterGain;
        filteredRecallMax.shortTerm += (recallMax.shortTerm - filteredRecallMax.shortTerm) * kLimitFilterGain;
        filteredRecallMax.longTerm += (recallMax.longTerm - filteredRecallMax.longTerm) * kLimitFilterGain;
    }

};
