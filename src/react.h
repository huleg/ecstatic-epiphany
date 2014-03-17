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
        : mem(mem)
    {}

    typedef VisualMemory::memory_t memory_t;
    typedef VisualMemory::recallVector_t recallVector_t;

    recallVector_t shortFilter;
    recallVector_t longFilter;

    VisualMemory *mem;

    static const memory_t kShortFilterGain = 1e-3;
    static const memory_t kLongFilterGain = 1e-4;

    virtual void beginFrame(const FrameInfo &f)
    {
        const recallVector_t &recall = mem->recall();
        shortFilter.resize(recall.size());
        longFilter.resize(recall.size());

        for (unsigned i = 0; i < recall.size(); i++) {

            // Log scale
            memory_t r = recall[i];
            r = r ? log(sq(r)) : 0;

            // Filters
            shortFilter[i] += (r - shortFilter[i]) * kShortFilterGain;
            longFilter[i] += (r - longFilter[i]) * kLongFilterGain;
        }
    }

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        memory_t f = sq(shortFilter[p.index] - longFilter[p.index]) * 1e-2;

        f = isfinite(f) ? f : f;
        f = -std::min(1.0, std::max(0.0, f));

        rgb = Vec3(f,f,f);
    }
};
