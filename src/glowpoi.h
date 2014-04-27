/*
 * Spinning light, harmonics under your control.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "grid_structure.h" 
#include "lib/effect.h"
#include "lib/particle.h"
#include "lib/prng.h"
#include "lib/texture.h"
#include "lib/noise.h"
#include "lib/camera_flow.h"
#include "lib/rapidjson/document.h"


class GlowPoi : public ParticleEffect
{
public:
    GlowPoi(const CameraFlowAnalyzer &flow, const rapidjson::Value &config);
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void debug(const DebugInfo &d);
    virtual void shader(Vec3& rgb, const PixelInfo &p) const;

private:
    unsigned numPoi;
    unsigned particlesPerPoi;
    unsigned numParticles;
    float radius;
    float intensity;
    float rate;
    float flowScale;
    float r1, r2, a1, a2, u1, u2, v1, v2;

    CameraFlowCapture flow;
    Texture palette;
    float cycle;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline GlowPoi::GlowPoi(const CameraFlowAnalyzer& flow, const rapidjson::Value &config)
    : numPoi(config["numPoi"].GetUint()),
      particlesPerPoi(config["particlesPerPoi"].GetUint()),
      numParticles(numPoi * particlesPerPoi),
      radius(config["radius"].GetDouble()),
      intensity(config["intensity"].GetDouble()),
      rate(config["rate"].GetDouble()),
      flowScale(config["flowScale"].GetDouble()),
      r1(config["r1"].GetDouble()),
      r2(config["r2"].GetDouble()),
      a1(config["a1"].GetDouble()),
      a2(config["a2"].GetDouble()),
      u1(config["u1"].GetDouble()),
      u2(config["u2"].GetDouble()),
      v1(config["v1"].GetDouble()),
      v2(config["v2"].GetDouble()),
      flow(flow),
      palette(config["palette"].GetString()),
      cycle(0)
{
    reseed(42);
}

inline void GlowPoi::reseed(unsigned seed)
{
    PRNG prng;
    prng.seed(seed);

    flow.capture();
    flow.origin();

    cycle = prng.uniform();
}

inline void GlowPoi::beginFrame(const FrameInfo &f)
{
    cycle += f.timeDelta * rate;
    flow.capture();
    appearance.resize(numParticles);

    for (unsigned poi = 0; poi < numPoi; poi++) {
        for (unsigned i = 0; i < particlesPerPoi; i++) {
            ParticleAppearance& p = appearance[poi * particlesPerPoi + i];

            p.radius = radius;
            p.intensity = intensity;

            p.point = Vec3(r1 * cos(cycle * a1 + poi * u1 + i * v1) + r2 * cos(cycle * a2 + poi * u2 + i * v2), 0,
                           r1 * sin(cycle * a1 + poi * u1 + i * v1) + r2 * sin(cycle * a2 + poi * u2 + i * v2));
        }
    }

    ParticleEffect::beginFrame(f);
}

inline void GlowPoi::debug(const DebugInfo &d)
{
    fprintf(stderr, "\t[glowpoi] numParticles = %d\n", (int)appearance.size());
    fprintf(stderr, "\t[glowpoi] cycle = %f\n", cycle);
    fprintf(stderr, "\t[glowpoi] point[0] = %f, %f, %f\n", appearance[0].point[0], appearance[0].point[1], appearance[0].point[2]);
    ParticleEffect::debug(d);
}

inline void GlowPoi::shader(Vec3& rgb, const PixelInfo &p) const
{
    float r = std::max(0.0f, 1.0f - sampleIntensity(p.point));
    float a = p.getNumber("blockAngle");
    rgb = palette.sample(0.5 + r * cos(a), 0.5 + r * sin(a));
}
