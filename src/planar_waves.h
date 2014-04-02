/*
 * Wave functions on the XZ plane.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include "lib/prng.h"
#include "lib/effect.h"


class PlanarWaves : public Effect {
public:
    PlanarWaves();
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo& p) const;

private:
    struct Wave {
        float amplitude, frequency, angle, phase;
        float eval(const PixelInfo &p) const;
        void reseed(PRNG &prng);
        void update(const FrameInfo &f);
    };

    std::vector<Wave> waves;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline PlanarWaves::PlanarWaves()
{
    reseed(12);
}

inline void PlanarWaves::reseed(unsigned seed)
{
    PRNG prng;
    prng.seed(seed);

    waves.resize(8);
    for (unsigned i = 0; i < waves.size(); i++) {
        waves[i].reseed(prng);
    }
}

inline void PlanarWaves::beginFrame(const FrameInfo &f)
{
    for (unsigned i = 0; i < waves.size(); i++) {
        waves[i].update(f);
    }
}

inline void PlanarWaves::shader(Vec3& rgb, const PixelInfo& p) const
{
    float a = 0;
    for (unsigned i = 0; i < waves.size(); i++) {
        a += waves[i].eval(p);
    }
    rgb = Vec3(1,1,1) * (a*a*a);
}

inline float PlanarWaves::Wave::eval(const PixelInfo &p) const
{
    // Rotate in XZ plane
    float x = p.point[0] * cos(angle) - p.point[2] * sin(angle);

    // Stationary point at the origin, for predictable transition in
    return amplitude * sin(x * frequency + phase);
}

inline void PlanarWaves::Wave::reseed(PRNG &prng)
{
    amplitude = prng.uniform(0.1, 0.4);
    frequency = 0;
    angle = prng.uniform(0, M_PI*2);
    phase = prng.uniform(0, M_PI*2);
}

inline void PlanarWaves::Wave::update(const FrameInfo &f)
{
    // amplitude *= 0.999;
    frequency += 0.001;
    angle *= 1.0001;
}
