#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "lib/particle.h"
#include "lib/brightness.h"
#include "lib/camera.h"
#include "lib/camera_framegrab.h"
#include "lib/camera_sampler.h"


class MyEffect : public ParticleEffect
{
public:

    static const float grabInterval = 2.0;

    CameraFramegrab grab;
    unsigned counter;
    float grabTimer;

    MyEffect()
        : counter(0), grabTimer(0)
    {
        appearance.resize(CameraSampler::kSamples);

        for (unsigned sample = 0; sample < CameraSampler::kSamples; sample++) {
            ParticleAppearance &p = appearance[sample];

            float x = CameraSampler::sampleX(sample) / float(Camera::kPixelsPerLine);
            float y = CameraSampler::sampleY(sample) / float(Camera::kLinesPerFrame);

            p.point = Vec3( -8.0f * (x - 0.5f), 0, -6.0f * (y - 0.5f) );
            p.color = Vec3(1, 1, 1);
            p.radius = 20.0f / float(Camera::kLinesPerField);
            p.intensity = 0.0f;
        }
    }

    virtual void beginFrame(const FrameInfo& f)
    {
        grabTimer += f.timeDelta;
        if (grabTimer > grabInterval) {
            grabTimer = 0;
            counter++;
            
            char buffer[128];
            snprintf(buffer, sizeof buffer, "frame-%04d.jpeg", counter);
            grab.begin(buffer);
        }

        ParticleEffect::beginFrame(f);
    }

    static void videoCallback(const Camera::VideoChunk &video, void *context)
    {
        MyEffect *self = static_cast<MyEffect*>(context);

        if (self->grab.isGrabbing()) {
            self->grab.process(video);
        }

        CameraSampler sampler(video);
        unsigned index;
        uint8_t luma;

        while (sampler.next(index, luma)) {
            ParticleAppearance &p = self->appearance[index];
            p.intensity = sq(luma / 255.0f);
        }
    }
};


int main(int argc, char **argv)
{
    MyEffect effect;

    Camera::start(MyEffect::videoCallback, &effect);

    EffectMixer mixer;
    mixer.add(&effect);

    Brightness br(mixer);

    EffectRunner r;
    r.setEffect(&br);
    r.setLayout("layouts/window6x12.json");
    return r.main(argc, argv);
}
