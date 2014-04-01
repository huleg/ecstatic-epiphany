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
    static const float noiseFadeRate = 0.002;
    static const float colorFadeRate = 0.01;

    struct Ant {
        int x, y, direction;
        void reseed();
        void update(Ants &world);
    };

    Ant ant;
    float timeDeltaRemainder;
    std::vector<unsigned> state;

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
    clear();
    memset(&state[0], 0, state.size() * sizeof state[0]);
    ant.reseed();
}

inline void Ants::beginFrame(const FrameInfo& f)
{
    Pixelator::beginFrame(f);
    state.resize(width() * height());

    // Fixed timestep
    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }

    // Visual updates

    static const Vec3 colors[] = {
        Vec3(0, 0, 0),
        Vec3(0.8, 0.7, 0.9),
        Vec3(0.0, 0.1, 0.4)
    };

    for (unsigned y = 0; y < height(); y++) {
        for (unsigned x = 0; x < width(); x++) {
            PixelAppearance &a = pixelAppearance(x, y);
            unsigned &st = state[pixelIndex(x, y)];
            Vec3 targetColor = colors[st];

            a.color += (targetColor - a.color) * colorFadeRate;
            a.noise *= 1.0 - noiseFadeRate;
        }
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
    x = umod(x, world.width());
    y = umod(y, world.height());
    PixelAppearance &a = world.pixelAppearance(x, y);
    unsigned &st = world.state[world.pixelIndex(x, y)];

    if (st == 1) {
        direction--;
        st = 2;

    } else {
        direction++;
        st = 1;

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
}
