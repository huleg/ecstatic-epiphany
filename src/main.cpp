// XXX hacky test code

#include <time.h>

#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "lib/particle.h"
#include "lib/camera.h"
#include "lib/camera_framegrab.h"
#include "lib/camera_sampler.h"
#include "lib/prng.h"
#include "narrator.h"

static CameraFramegrab grab;
static Narrator narrator;


static void videoCallback(const Camera::VideoChunk &video, void *)
{
    if (grab.isGrabbing()) {
        grab.process(video);
    }
    // narrator.vismem.process(video);
}

int main(int argc, char **argv)
{
    // Camera::start(videoCallback);

    narrator.runner.setLayout("layouts/window6x12.json");
    if (!narrator.runner.parseArguments(argc, argv)) {
        return 1;
    }

    narrator.run();
    return 0;
}
