/*
 * Seeing nothing but trees.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/effect.h"
#include "lib/particle.h"
#include "lib/prng.h"
#include "lib/camera_flow.h"
#include "lib/rapidjson/document.h"
#include "lib/sampler.h"


class Forest : public ParticleEffect
{
public:
    Forest(const CameraFlowAnalyzer &flow, const rapidjson::Value &config);
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void debug(const DebugInfo &di);

private:
    struct TreeInfo {
        Vec2 texCoord;
        Vec3 direction;
        Vec3 point;
        unsigned branchState;
    };

    unsigned maxParticles;
    float outsideMargin;
    float flowScale;
    float flowFilterRate;

    Texture palette;
    CameraFlowCapture flow;
    const rapidjson::Value& config;
    Sampler s;
    std::vector<TreeInfo> tree;

    void addPoint();
    void updateFlow(const FrameInfo &f);
    Vec3 pointOffset();
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline Forest::Forest(const CameraFlowAnalyzer &flow, const rapidjson::Value &config)
    : maxParticles(config["maxParticles"].GetUint()),
      outsideMargin(config["outsideMargin"].GetDouble()),
      flowScale(config["flowScale"].GetDouble()),
      flowFilterRate(config["flowFilterRate"].GetDouble()),
      palette(config["palette"].GetString()),
      flow(flow),
      config(config),
      s(42)
{}

inline void Forest::reseed(unsigned seed)
{
    appearance.clear();
    tree.clear();
    flow.capture(1.0);
    flow.origin();
    s = Sampler(seed);
}

inline void Forest::beginFrame(const FrameInfo &f)
{
    if (appearance.size() < maxParticles) {
        addPoint();
    }

    updateFlow(f);

    ParticleEffect::beginFrame(f);
}

inline Vec3 Forest::pointOffset()
{
    return Vec3(flow.model[0] * flowScale, 0, 0);
}

inline void Forest::updateFlow(const FrameInfo &f)
{
    flow.capture(flowFilterRate);

    unsigned i = 0, j = 0;
    for (; i < appearance.size(); i++) {
        appearance[i].point = tree[i].point + pointOffset();

        if (f.distanceOutsideBoundingBox(appearance[i].point) > outsideMargin * appearance[i].radius) {
            // Escaped
            continue;
        }

        if (j != i) {
            appearance[j] = appearance[i];
            tree[j] = tree[i];
        }
        j++;
    }
    appearance.resize(j);
    tree.resize(j);
}

inline void Forest::addPoint()
{
    ParticleAppearance pa;
    TreeInfo ti;

    pa.radius = s.value(config["radius"]);
    pa.intensity = s.value(config["intensity"]);

    int root = std::max<int>(0,
        s.uniform(appearance.size() - s.value(config["historyDepth"]),
                  appearance.size() + s.value(config["newnessBias"])));

    if (root >= (int)appearance.size()) {
        // Start a new tree
        ti.texCoord = s.value2D(config["newTexCoord"]);
        ti.direction = s.value3D(config["newDirection"]);
        ti.branchState = 0;
        ti.point = s.value3D(config["newPoint"]) - pointOffset();
    } else {
        ti.texCoord = tree[root].texCoord + s.value2D(config["deltaTexCoord"]);
        ti.direction = tree[root].direction + s.value3D(config["deltaDirection"]);
        ti.branchState = tree[root].branchState + 1;
        ti.point = appearance[root].point + tree[root].direction;
    }

    pa.color = palette.sample(ti.texCoord);

    appearance.push_back(pa);
    tree.push_back(ti);
}


inline void Forest::debug(const DebugInfo &di)
{
    ParticleEffect::debug(di);
    fprintf(stderr, "\t[forest] numParticles = %d\n", appearance.size());
}
