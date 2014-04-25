/*
 * Debugging the computer vision with LEDs.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/camera_flow.h"


class VisionDebug : public ParticleEffect
{
public:
    VisionDebug(CameraFlowAnalyzer& flow);

    virtual void beginFrame(const FrameInfo &f);

private:
    static constexpr float originInterval = 2.0f;

    CameraFlowCapture flow;
    float originTimer;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline VisionDebug::VisionDebug(CameraFlowAnalyzer& flow)
    : flow(flow), originTimer(0)
{}

inline void VisionDebug::beginFrame(const FrameInfo &f)
{
    flow.capture();

    originTimer += f.timeDelta;
    if (originTimer > originInterval) {
        originTimer -= originInterval;
        flow.origin();
    }

    appearance.resize(1);

    appearance[0].intensity = 0.7f;
    appearance[0].radius = 0.6f;
    appearance[0].color = Vec3(1,1,1);
    appearance[0].point = flow.model;

    ParticleEffect::beginFrame(f);
}
