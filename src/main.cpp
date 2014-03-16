// XXX hacky test code


#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "lib/particle.h"
#include "lib/camera.h"
#include "lib/camera_framegrab.h"
#include "lib/camera_sampler.h"

#include "visualmemory.h"
#include "rings.h"
#include "spokes.h"
#include "latencytimer.h"


static CameraFramegrab grab;
static VisualMemory vismem;
static EffectTap tap;
static LatencyTimer latency(tap);


static void videoCallback(const Camera::VideoChunk &video, void *)
{
    if (grab.isGrabbing()) {
        grab.process(video);
    }
    vismem.process(video);
    latency.process(video);
}


int main(int argc, char **argv)
{
    Camera::start(videoCallback);

    RingsEffect rings("data/glass.png");
    SpokesEffect spokes;

    EffectMixer mixer;
    mixer.add(&latency.effect);

    tap.setEffect(&mixer);

    EffectRunner r;
    r.setEffect(&tap);
    r.setLayout("layouts/window6x12.json");
    if (!r.parseArguments(argc, argv)) {
        return 1;
    }

    // Init visual memory, now that the layout is known
    vismem.start(&r);

    while (true) {
        float dt = r.doFrame();

        static char buffer[1024];
        static unsigned counter = 0;
        static float debugTimer = 0;
        debugTimer += dt;
        if (debugTimer > 1.0f) {
            debugTimer = 0;

            latency.debug();

            /*
            snprintf(buffer, sizeof buffer, "frame-%04d.jpeg", counter);
            grab.begin(buffer);

            snprintf(buffer, sizeof buffer, "frame-%04d-memory.png", counter);
            vismem.debug(buffer);
            */

            counter++;
        }
    }

    return 0;
}
