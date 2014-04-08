/*
 * Grid, leading to emergent order.
 * Langton's ants. Chunky pixels.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include "pixelator.h"
#include "lib/prng.h"


class Ants : public Pixelator {
public:
    Ants();
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo& f);

    float stepSize;

private:
    static const float noiseFadeRate = 0.004;
    static const float colorFadeRate = 0.01;
    static const float angleRate = 10.0;

    struct Ant {
        int x, y, direction;
        void reseed(PRNG &prng);
        void update(Ants &world);
    };

    Ant ant;
    float timeDeltaRemainder;
    std::vector<unsigned> state;

    void runStep(const FrameInfo &f);
    void filterColor(int x, int y);
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
    reseed(42);
}

inline void Ants::reseed(unsigned seed)
{
    stepSize = 1;
    clear();
    memset(&state[0], 0, state.size() * sizeof state[0]);

    PRNG prng;
    prng.seed(seed);
    ant.reseed(prng);
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

    // Visual updates, at least once per device frame
    for (unsigned y = 0; y < height(); y++) {
        for (unsigned x = 0; x < width(); x++) {
            filterColor(x, y);
        }
    }
}

inline void Ants::filterColor(int x, int y)
{
    static const Vec3 colors[] = {
        Vec3(0, 0, 0),
        Vec3(0.251, 0.804, 0.765),
        Vec3(0.078, 0.090, 0.153)
    };

    PixelAppearance &a = pixelAppearance(x, y);
    unsigned &st = state[pixelIndex(x, y)];
    Vec3 targetColor = colors[st];

    a.color += (targetColor - a.color) * colorFadeRate;
    a.noise *= 1.0 - noiseFadeRate;
}

inline void Ants::Ant::reseed(PRNG &prng)
{
    x = prng.uniform(1, 4);
    y = prng.uniform(1, 10);
    direction = prng.uniform(0, 10);
}

inline void Ants::Ant::update(Ants &world)
{
    x = umod(x, world.width());
    y = umod(y, world.height());

    // Also update color here, in case our step rate is much faster than
    // the device frame rate
    world.filterColor(x, y);

    PixelAppearance &a = world.pixelAppearance(x, y);
    unsigned &st = world.state[world.pixelIndex(x, y)];

    if (st == 1) {
        direction--;
        st = 2;

    } else {
        direction++;
        st = 1;

        a.angle += world.stepSize * angleRate;
        a.contrast = 0.5;
        a.noise = 1.5;
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
