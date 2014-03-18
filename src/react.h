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

    VisualMemory *mem;
    recallVector_t fastFilter;
    recallVector_t slowFilter;

    static const memory_t kFastFilterRate = 0.1;
    static const memory_t kSlowFilterRate = 1e-3;
    static const memory_t kSensitivity = 10.0;

    virtual void beginFrame(const FrameInfo &f)
    {
        const recallVector_t &recall = mem->recall();
        slowFilter.resize(recall.size());
        fastFilter.resize(recall.size());

        for (unsigned i = 0; i != recall.size(); i++) {
            memory_t f = fastFilter[i];
            f += (sq(recall[i] - 1.0) - f) * kFastFilterRate;
            fastFilter[i] = f;

            memory_t g = slowFilter[i];
            g += (f - g) * kSlowFilterRate;
            slowFilter[i] = g;
        }
    }

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        memory_t f = (fastFilter[p.index] - slowFilter[p.index]) * kSensitivity;
        hsv2rgb(rgb, 0.25 + f, 0.9, 0.9);
    }
};
