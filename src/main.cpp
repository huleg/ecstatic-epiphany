#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "rings.h"

#include "camera.h"

static void videoCallback(const Camera::VideoChunk &video, void *context)
{
    printf("Video byteCount=%d byteOffset=%d line=%d field=%d\n",
        video.byteCount, video.byteOffset, video.line, video.field);
}


int main(int argc, char **argv)
{
    Camera::start(videoCallback);

    RingsEffect rings("data/glass.png");

    EffectMixer mixer;
    mixer.add(&rings);

    EffectRunner r;
    r.setEffect(&mixer);
    r.setLayout("layouts/window6x12.json");
    return r.main(argc, argv);
}