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

    static const memory_t kSensitivity = 1e2;

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        memory_t f = 0.5 + mem->recall()[p.index] * kSensitivity;
        f = std::min(1.0, std::max(0.0, f));
        rgb = Vec3(0,f,0);
    }
};
