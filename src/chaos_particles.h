/*
 * Complex particle system pattern.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/particle.h"
#include "lib/prng.h"
#include "lib/texture.h"


class ChaosParticles : public ParticleEffect
{
public:
    ChaosParticles();
    void reseed();
    virtual void beginFrame(const FrameInfo &f);

private:
    static const unsigned numParticles = 1000;
    static const float agingRate = 0.1;
    static const float speedGain = 3.0;
    static const float relativeSize = 0.05;
    static const float intensity = 1.5;
    static const float initialSpeed = 0.001;
    static const float stepSize = 0.002;
    static const unsigned maxAge = 30000;

    struct ParticleDynamics {
        Vec2 position;
        Vec2 velocity;
        bool escaped, dead;
        unsigned generation;
        unsigned age;
    };

    Texture temperatureScale;
    std::vector<ParticleDynamics> dynamics;
    float timeDeltaRemainder;

    static Vec2 circularRandomVector(PRNG &prng);
    void updateColor(unsigned i);
    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline ChaosParticles::ChaosParticles()
    : temperatureScale("data/temperature-scale.png"),
      timeDeltaRemainder(0)
{
    reseed();
}

inline Vec2 ChaosParticles::circularRandomVector(PRNG &prng)
{
    // Random vector with a uniform circular distribution
    for (unsigned i = 0; i < 100; i++) {
        Vec2 v;
        v[0] = prng.uniform(-1, 1);
        v[1] = prng.uniform(-1, 1);
        if (sqrlen(v) <= 1.0) {
            return v;
        }
    }
    return Vec2(0, 0);
}

inline void ChaosParticles::reseed()
{
    appearance.resize(numParticles);
    dynamics.resize(numParticles);

    PRNG prng;
    prng.seed(42);

    for (unsigned i = 0; i < dynamics.size(); i++) {
        dynamics[i].position = Vec2(0,0);
        dynamics[i].velocity = circularRandomVector(prng) * initialSpeed;
        dynamics[i].age = 0;
        dynamics[i].generation = 0;
        dynamics[i].dead = false;
        dynamics[i].escaped = false;
        updateColor(i);
    }
}

inline void ChaosParticles::updateColor(unsigned i)
{
    appearance[i].color = temperatureScale.sample(dynamics[i].generation * agingRate, 0);
}


inline void ChaosParticles::beginFrame(const FrameInfo &f)
{    
    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }
}

inline void ChaosParticles::runStep(const FrameInfo &f)
{
    PRNG prng;
    prng.seed(19);

    // Update dynamics
    for (unsigned i = 0; i < dynamics.size(); i++) {

        dynamics[i].position += dynamics[i].velocity;
        dynamics[i].age++;

        if (dynamics[i].age > maxAge) {
            dynamics[i].dead = true;
        }
        if (dynamics[i].dead) {
            appearance[i].intensity = 0;
            continue;
        }

        // XZ plane
        appearance[i].point[0] = dynamics[i].position[0];
        appearance[i].point[2] = dynamics[i].position[1];

        // Fade in/out
        appearance[i].intensity = intensity * sinf(dynamics[i].age * (2.0 * M_PI) / maxAge);
        appearance[i].radius = f.modelDiameter * relativeSize;

        dynamics[i].escaped = f.distanceOutsideBoundingBox(appearance[i].point) > appearance[i].radius;

        prng.remix(dynamics[i].position[0] * 1e8);
        prng.remix(dynamics[i].position[1] * 1e8);
    }

    // Reassign each escaped particle randomly to be near a non-escaped one
    for (unsigned i = 0; i < dynamics.size(); i++) {
        if (dynamics[i].escaped && !dynamics[i].dead) {
            for (unsigned attempt = 0; attempt < 100; attempt++) {
                unsigned seed = prng.uniform(0, dynamics.size() - 0.0001);
                if (!dynamics[seed].escaped) {

                    // Fractal respawn at seed position
                    dynamics[i] = dynamics[seed];
                    dynamics[i].generation++;
                    dynamics[i].age = 0;
                    dynamics[i].velocity += circularRandomVector(prng) * len(dynamics[i].velocity) * speedGain;
                    updateColor(i);

                    break;
                }
            }
        }
    }

    ParticleEffect::beginFrame(f);
}
