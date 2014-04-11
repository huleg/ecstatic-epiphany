/*
 * Two entities, exploring ways to interact.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <math.h>
#include <vector>
#include "lib/effect.h"
#include "lib/particle.h"
#include "lib/texture.h"
#include "lib/noise.h"


class PartnerDance : public ParticleEffect
{
public:
    PartnerDance();
    void reseed(uint32_t seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo& p) const;

    Texture palette;

private:
    static const unsigned particlesPerDancer = 20;
    static const unsigned numDancers = 2;
    static const unsigned numParticles = numDancers * particlesPerDancer;
    static const float stepSize = 1.0 / 200;

    static const float targetGain = 0.0004;
    static const float targetSpin = 0.00015;
    static const float damping = 0.012;
    static const float colorRate = 0.8;
    static const float noiseRate = 0.1;

    struct ParticleDynamics {
        Vec2 position;
        Vec2 velocity;
        Vec2 target;
    };

    std::vector<ParticleDynamics> dynamics;
    float timeDeltaRemainder;
    float colorCycle;
    float noiseCycle;

    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline PartnerDance::PartnerDance()
    : palette("data/shoreline-palette.png"),
      timeDeltaRemainder(0)
{
    reseed(42);
}

inline void PartnerDance::reseed(uint32_t seed)
{
    PRNG prng;
    prng.seed(seed);

    colorCycle = prng.uniform(0, M_PI * 2);
    noiseCycle = prng.uniform(0, 1000);

    appearance.resize(numParticles);
    dynamics.resize(numParticles);

    ParticleAppearance *pa = &appearance[0];
    ParticleDynamics *pd = &dynamics[0]; 

    for (unsigned dancer = 0; dancer < numDancers; dancer++) {
        for (unsigned i = 0; i < particlesPerDancer; i++, pa++, pd++) {

            pd->position = prng.circularVector() * 3.0 + (dancer ? Vec2(4, 0) : Vec2(-4, 0));
            pd->target = prng.circularVector() * 0.1;
            pd->velocity = prng.circularVector() * 0.1;

            pa->intensity = 0.4;
            pa->radius = 0.5;
            pa->color = dancer ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        }
    }
}

inline void PartnerDance::beginFrame(const FrameInfo &f)
{    
    colorCycle = fmodf(colorCycle + f.timeDelta * colorRate, 2 * M_PI);
    noiseCycle += f.timeDelta * noiseRate;

    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }

    ParticleEffect::beginFrame(f);
}

inline void PartnerDance::runStep(const FrameInfo &f)
{
    ParticleAppearance *pa = &appearance[0];
    ParticleDynamics *pd = &dynamics[0]; 

    PRNG prng;
    prng.seed(42);

    for (unsigned dancer = 0; dancer < numDancers; dancer++) {
        for (unsigned i = 0; i < particlesPerDancer; i++, pa++, pd++) {

            prng.remix(pd->position[0] * 1e8);
            prng.remix(pd->position[1] * 1e8);

            Vec2 disparity = pd->target - pd->position;
            Vec2 normal = Vec2(disparity[1], -disparity[0]);
            Vec2 v = pd->velocity;
            v *= 1.0 - damping;
            v += disparity * targetGain;
            v += normal * targetSpin;

            pd->target[0] = fbm_noise2(noiseCycle, 0, 4);
            pd->target[1] = fbm_noise2(noiseCycle, 1, 4);

            pd->velocity = v;
            pd->position += v;

            pa->point = Vec3(pd->position[0], 0, pd->position[1]);

            if (sqrlen(v) < sq(0.001)) {
                pd->position = prng.circularVector() * 3.0 + Vec2(4, 0);
            }
        }
    }
}

inline void PartnerDance::shader(Vec3& rgb, const PixelInfo& p) const
{
    ParticleEffect::shader(rgb, p);
    rgb = palette.sample(rgb[0], rgb[1]);
}
