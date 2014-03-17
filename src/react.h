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

    recallVector_t filter;

    static const memory_t kFilterRate = 0.1;

    VisualMemory *mem;

    virtual void beginFrame(const FrameInfo &f)
    {
        const recallVector_t &recall = mem->recall();

        filter.resize(recall.size());

        memory_t max = 0;
        for (unsigned i = 0; i < recall.size(); i++) {
            memory_t r = recall[i];
            max = std::max(max, r);
        }

        if (max > 0) {
            memory_t scale = 1.0 / max;

            for (unsigned i = 0; i < recall.size(); i++) {
                memory_t s = recall[i] * scale;
                s *= s * s;

                memory_t f = filter[i];
                f += (s - f) * kFilterRate;
                filter[i] = f;
            }
        }
    }

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        memory_t f = std::max<memory_t>(0.0, filter[p.index]) * -1e4;

        rgb = Vec3(f,f,f);
    }
};
