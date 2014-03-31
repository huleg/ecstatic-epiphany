/*
 * Grid, leading to emergent order.
 * Langton's ants. Chunky pixels.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include "pixelator.h"


class Ants : public Pixelator {
public:
    Ants();
    void reseed();

    virtual void beginFrame(const FrameInfo& f);

    float stepSize;

private:
    static const float noiseFadeRate = 0.1;
    static const float colorFadeRate = 0.02;

    struct Ant {
        int x, y, direction;
        void reseed();
        void update(Ants &world);
    };

    Ant ant;
    float timeDeltaRemainder;

    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


static inline int umod(int a, int b)
{
    int c = a % b;
    if (c < 0) {
        c += b;
    }
    return c;
}

inline Ants::Ants()
    : timeDeltaRemainder(0)
{
    reseed();
}

inline void Ants::reseed()
{
    stepSize = 1;
    ant.reseed();
}

inline void Ants::beginFrame(const FrameInfo& f)
{
    Pixelator::beginFrame(f);

    // Fixed timestep
    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }
}

inline void Ants::Ant::reseed()
{
    x = 2;
    y = 5;
    direction = 2;
}

inline void Ants::Ant::update(Ants &world)
{
    static const Vec3 onColor = Vec3(0.8, 0.7, 0.9);
    static const Vec3 offColor = Vec3(0, 0.2, 0.5);

    x = umod(x, world.width());
    y = umod(y, world.height());
    PixelAppearance &a = world.pixelAppearance(x, y);

    if (a.color[0]) {
        direction--;
        a.color = offColor;

    } else {
        direction++;
        a.color = onColor;

        a.angle += 0.1;
        a.contrast = 0.5;
        a.noise = 0.5;
    }

    direction = umod(direction, 4);
    switch (direction) {
        case 0: x++; break;
        case 1: y++; break;
        case 2: x--; break;
        case 3: y--; break;
    }
}

inline void Ants::runStep(const FrameInfo &f)
{
    ant.update(*this);

    for (unsigned y = 0; y < height(); y++) {
        for (unsigned x = 0; x < width(); x++) {
            PixelAppearance &a = pixelAppearance(x, y);

            a.noise *= 1.0 - noiseFadeRate;
            a.color *= 1.0 - colorFadeRate;
        }
    }
}
