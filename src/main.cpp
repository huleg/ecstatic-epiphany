#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "lib/particle.h"
#include "lib/camera.h"


class MyEffect : public ParticleEffect
{
public:
    MyEffect()
    {
        appearance.resize(Camera::kPixelsPerLine * Camera::kLinesPerField);

        for (unsigned line = 0; line < Camera::kLinesPerField; line++) {
            for (unsigned pixel = 0; pixel < Camera::kPixelsPerLine; pixel++) {

                ParticleAppearance &p = appearance[pixel + line * Camera::kPixelsPerLine];

                p.point = Vec3(
                    -8.0f * (pixel / float(Camera::kPixelsPerLine) - 0.5f), 0,
                    -6.0f * (line / float(Camera::kLinesPerField) - 0.5f));

                p.color = Vec3(1, 1, 1);
                p.radius = 10.0f / float(Camera::kLinesPerField);
                p.intensity = 0.0f;
            }
        }
    }

    static void videoCallback(const Camera::VideoChunk &video, void *context)
    {
        MyEffect *self = static_cast<MyEffect*>(context);

        Camera::VideoChunk iter = video;
        while (iter.byteCount) {

            if ((iter.byteOffset & 1) == 1) {
                // Luminance

                unsigned x = iter.byteOffset / 2;
                ParticleAppearance &p = self->appearance[x + iter.line * Camera::kPixelsPerLine];
                p.intensity = 0.0008f * *iter.data;
            }

            iter.byteOffset++;
            iter.byteCount--;
            iter.data++;
        }
    }
};


int main(int argc, char **argv)
{
    MyEffect effect;

    Camera::start(MyEffect::videoCallback, &effect);

    EffectMixer mixer;
    mixer.add(&effect);

    EffectRunner r;
    r.setEffect(&mixer);
    r.setLayout("layouts/window6x12.json");
    return r.main(argc, argv);
}