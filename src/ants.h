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
#include "lib/texture.h"


class Ants : public Pixelator {
public:
    Ants();
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo& f);

    float stepSize;
    float colorParam;

private:
    static const float noiseMax = 1.5;
    static const float noiseFadeRate = 0.007;
    static const float colorFadeRate = 0.015;
    static const float angleRate = 10.0;
    static const float colorParamRate = 0.08;

    struct Ant {
        int x, y, direction;
        void reseed(PRNG &prng);
        void update(Ants &world);
    };

    Ant ant;
    float timeDeltaRemainder;
    std::vector<unsigned> state;
    std::vector<Vec3> targetColors;
    Texture palette;

    void runStep(const FrameInfo &f);
    void filterColor(int x, int y);
    Vec3 targetColorForState(unsigned st);
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
    : timeDeltaRemainder(0),
      palette("data/succulent-palette.png")
{
    reseed(42);
}

inline void Ants::reseed(unsigned seed)
{
    stepSize = 1;

    clear();
    state.clear();
    targetColors.clear();

    PRNG prng;
    prng.seed(seed);
    ant.reseed(prng);

    colorParam = prng.uniform(0, M_PI * 2);
}

inline void Ants::beginFrame(const FrameInfo& f)
{
    Pixelator::beginFrame(f);

    if (state.size() != width() * height()) {
        // Resize and erase parallel arrays, now that we know size
        state.resize(width() * height());
        targetColors.resize(width() * height());
        memset(&state[0], 0, state.size() * sizeof state[0]);
        memset(&targetColors[0], 0, targetColors.size() * sizeof targetColors[0]);
    }

    colorParam += f.timeDelta * colorParamRate;

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
    PixelAppearance &a = pixelAppearance(x, y);
    Vec3 targetColor = targetColors[pixelIndex(x, y)];

    a.color += (targetColor - a.color) * colorFadeRate;
    a.noise *= 1.0 - noiseFadeRate;
}

inline Vec3 Ants::targetColorForState(unsigned st)
{
    static float brightness[] = { 0, 1.65, 0.3 };
    float r = 0.65 / (1 + colorParam * 0.2);
    float t = colorParam;
    float br = st < 3 ? brightness[st] : 0;
    return palette.sample(cosf(t) * r + 0.5,
                          sinf(t) * r + 0.5) * br;
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
        a.contrast = 0.4;
        a.noise = noiseMax;
    }

    world.targetColors[world.pixelIndex(x, y)] = world.targetColorForState(st);

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
