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

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        memory_t recall = mem->recall()[p.index];

        memory_t f = 0.25 + sq(recall - 1.0) * 10;

        hsv2rgb(rgb, f, 0.9, 0.9);
    }
};
