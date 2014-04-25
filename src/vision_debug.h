/*
 * Debugging the computer vision with LEDs.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/camera_flowvectorizer.h"


class VisionDebug : public ParticleEffect
{
public:
    VisionDebug(CameraFlowVectorizer& flow);

    virtual void beginFrame(const FrameInfo &f);

private:
    CameraFlowVectorizer& flow;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline VisionDebug::VisionDebug(CameraFlowVectorizer& flow)
    : flow(flow)
{}

inline void VisionDebug::beginFrame(const FrameInfo &f)
{
    const float s = 0.4;

    appearance.resize(1);

    appearance[0].intensity = 0.7f;
    appearance[0].radius = 0.6f;
    appearance[0].color = Vec3(1,1,1);
    appearance[0].point = Vec3(
        flow.overall.x * s,
        0,
        flow.overall.y * -s);

    ParticleEffect::beginFrame(f);
}
