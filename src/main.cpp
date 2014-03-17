// XXX hacky test code

#include <time.h>

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
#include "react.h"
#include "latencytimer.h"


static CameraFramegrab grab;
static VisualMemory vismem;
static EffectTap tap;


static void videoCallback(const Camera::VideoChunk &video, void *)
{
    if (grab.isGrabbing()) {
        grab.process(video);
    }
    vismem.process(video);
}


static void debugThread(void *)
{
    char buffer[1024];
    unsigned counter = 0;
    struct tm tm;
    time_t clk;

    while (true) {
        clk = time(NULL);
        localtime_r(&clk, &tm);

        strftime(buffer, sizeof buffer, "output/%Y%m%d-%H%M%S-camera.jpeg", &tm);
        grab.begin(buffer);

        strftime(buffer, sizeof buffer, "output/%Y%m%d-%H%M%S-memory.png", &tm);
        vismem.debug(buffer);

        counter++;
        sleep(10);
    }
}

int main(int argc, char **argv)
{
    Camera::start(videoCallback);

    EffectMixer mixer;
    EffectRunner r;

    SpokesEffect spokes;
    mixer.add(&spokes, 0.2);

    RingsEffect rings("data/glass.png");
    mixer.add(&rings, 0.5);

    ReactEffect react(&vismem);
    mixer.add(&react);

    tap.setEffect(&mixer);
    r.setEffect(&tap);
    r.setLayout("layouts/window6x12.json");
    if (!r.parseArguments(argc, argv)) {
        return 1;
    }

    // Init visual memory, now that the layout is known
    vismem.start("imprint.mem", &r, &tap);

    // Dump memory in the background, to avoid delaying the main thread
    new tthread::thread(debugThread, 0);

    r.run();
    return 0;
}
