/*
 * Complex particle system.
 * Inspired by fractals,
 * leading to chaos.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/camera_flow.h"
#include "lib/particle.h"
#include "lib/prng.h"
#include "lib/texture.h"


class ChaosParticles : public ParticleEffect
{
public:
    ChaosParticles(const CameraFlowAnalyzer &flow, const rapidjson::Value &config);
    void reseed(Vec2 location, unsigned seed);

    bool isRunning();
    float getTotalIntensity();

    virtual void beginFrame(const FrameInfo &f);
    virtual void debug(const DebugInfo &di);

private:
    unsigned numParticles;
    unsigned numDarkParticles;
    unsigned maxAge;
    float generationScale;
    float speedMin;
    float speedMax;
    float spinMin;
    float spinMax;
    float relativeSize;
    float intensity;
    float intensityExp;
    float initialSpeed;
    float stepSize;
    float colorRate;
    float outsideMargin;
    float darkMultiplier;
    float flowScale;

    struct ParticleDynamics {
        Vec2 position;
        Vec2 velocity;
        bool escaped, dead;
        unsigned generation;
        unsigned age;
    };

    CameraFlowCapture flow;

    Texture palette;
    std::vector<ParticleDynamics> dynamics;
    float timeDeltaRemainder;
    float colorCycle;
    float totalIntensity;
    bool running;

    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


    unsigned numParticles;
    unsigned numDarkParticles;
    unsigned maxAge;
    float generationScale;
    float speedMin;
    float speedMax;
    float spinMin;
    float spinMax;
    float relativeSize;
    float intensity;
    float intensityExp;
    float initialSpeed;
    float stepSize;
    float colorRate;
    float outsideMargin;
    float darkMultiplier;
    float flowScale;

inline ChaosParticles::ChaosParticles(const CameraFlowAnalyzer &flow, const rapidjson::Value &config)
    : numParticles(config["numParticles"].GetUint()),
      numDarkParticles(config["numDarkParticles"].GetUint()),
      maxAge(config["maxAge"].GetUint()),
      generationScale(config["generationScale"].GetDouble()),
      speedMin(config["speedMin"].GetDouble()),
      speedMax(config["speedMax"].GetDouble()),
      relativeSize(config["relativeSize"].GetDouble()),
      intensity(config["intensity"].GetDouble()),
      intensityExp(config["intensityExp"].GetDouble()),
      initialSpeed(config["initialSpeed"].GetDouble()),
      stepSize(config["stepSize"].GetDouble()),
      colorRate(config["colorRate"].GetDouble()),
      outsideMargin(config["outsideMargin"].GetDouble()),
      darkMultiplier(config["darkMultiplier"].GetDouble()),
      flowScale(config["flowScale"].GetDouble()),
      flow(flow),
      palette(config["palette"].GetString()),
      timeDeltaRemainder(0),
      colorCycle(0)
{
    reseed(Vec2(0,0), 42);
}

inline bool ChaosParticles::isRunning()
{
    return running;
}

inline float ChaosParticles::getTotalIntensity()
{
    return totalIntensity;
}

inline void ChaosParticles::reseed(Vec2 location, unsigned seed)
{
    running = true;
    totalIntensity = nanf("");

    appearance.resize(numParticles);
    dynamics.resize(numParticles);

    PRNG prng;
    prng.seed(seed);

    colorCycle = prng.uniform(0, M_PI * 2);

    for (unsigned i = 0; i < dynamics.size(); i++) {
        dynamics[i].position = location;
        dynamics[i].velocity = prng.ringVector(0.01, 1.0) * initialSpeed;
        dynamics[i].age = 0;
        dynamics[i].generation = 0;
        dynamics[i].dead = false;
        dynamics[i].escaped = false;
    }
}

inline void ChaosParticles::beginFrame(const FrameInfo &f)
{    
    if (!running) {
        return;
    }

    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }

    colorCycle = fmodf(colorCycle + f.timeDelta * colorRate, 2 * M_PI);

    ParticleEffect::beginFrame(f);
}

inline void ChaosParticles::debug(const DebugInfo &di)
{
    fprintf(stderr, "\t[chaos-particles] running = %d\n", running);
    fprintf(stderr, "\t[chaos-particles] totalIntensity = %f\n", totalIntensity);
    ParticleEffect::debug(di);
}

inline void ChaosParticles::runStep(const FrameInfo &f)
{
    PRNG prng;
    prng.seed(19);

    unsigned numLiveParticles = 0;
    float intensityAccumulator = 0;

    // Capture the impulse between the last step and this one
    flow.capture(1.0);
    flow.origin();

    // Update dynamics
    for (unsigned i = 0; i < dynamics.size(); i++) {

        // Horizontal flow -> position
        dynamics[i].position[0] += dynamics[i].velocity[0] - flow.model[0] * flowScale;
        dynamics[i].position[1] += dynamics[i].velocity[1];
        dynamics[i].age++;

        if (dynamics[i].age > maxAge) {
            dynamics[i].dead = true;
        }
        if (dynamics[i].dead) {
            appearance[i].intensity = 0;
            continue;
        }
        float ageF = dynamics[i].age / (float)maxAge;

        // XZ plane
        appearance[i].point[0] = dynamics[i].position[0];
        appearance[i].point[2] = dynamics[i].position[1];

        // Dark matter, to break up the monotony of lightness
        bool darkParticle = i < numDarkParticles;

        // Fade in/out
        float fade = pow(std::max(0.0f, sinf(ageF * M_PI)), intensityExp);
        float particleIntensity = intensity * fade;
        appearance[i].intensity = darkParticle ? particleIntensity * darkMultiplier : particleIntensity;
        appearance[i].radius = f.modelRadius * relativeSize * fade;

        numLiveParticles++;
        intensityAccumulator += particleIntensity;

        float c = (dynamics[i].generation + ageF) * generationScale;
        appearance[i].color = darkParticle ? Vec3(1,1,1) : palette.sample(c, 0.5 + 0.5 * sinf(colorCycle));

        dynamics[i].escaped = f.distanceOutsideBoundingBox(appearance[i].point) >
            outsideMargin * appearance[i].radius;

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
                    Vec2 &v = dynamics[i].velocity;

                    // Speed modulation
                    v *= prng.uniform(speedMin, speedMax);

                    // Direction modulation
                    float t = prng.uniform(spinMin, spinMax);
                    float c = cosf(t);
                    float s = sinf(t);
                    v = Vec2( v[0] * c - v[1] * s ,
                              v[0] * s + v[1] * c );

                    break;
                }
            }
        }
    }

    totalIntensity = intensityAccumulator;
    if (!numLiveParticles) {
        running = false;
    }
}
