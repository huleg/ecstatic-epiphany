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


static CameraPeriodicFramegrab periodicGrab;
static VisualMemory vismem;


static void videoCallback(const Camera::VideoChunk &video, void *)
{
    periodicGrab.process(video);
    vismem.process(video);
}


int main(int argc, char **argv)
{
    Camera::start(videoCallback);

    RingsEffect rings("data/glass.png");
    SpokesEffect spokes;

    EffectMixer mixer;
    mixer.add(&rings);

    EffectRunner r;
    r.setEffect(&mixer);
    r.setLayout("layouts/window6x12.json");
    if (!r.parseArguments(argc, argv)) {
        return 1;
    }

    // Init visual memory, now that the layout is known
    vismem.init(&r);

    while (true) {
        float dt = r.doFrame();
        periodicGrab.timeStep(dt);

        static float debugTimer = 0;
        debugTimer += dt;
        if (debugTimer > 20.0f) {
            debugTimer = 0;
            vismem.debug("vismem-debug.png");
        }
    }

    return 0;
}
