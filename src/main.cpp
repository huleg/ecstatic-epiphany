// XXX hacky test code

#include <time.h>
#include <SDL/SDL.h>

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
static EffectRunner runner;
static EffectMixer mixer;


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

static void effectThread(void *)
{
    runner.run();
}

static void sdlThread()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        perror("SDL init failed");
        return;
    }

    SDL_Surface *screen = SDL_SetVideoMode(1024, 768, 32, 0);
    if (!screen) {
        perror("SDL failed to set video mode");
        return;
    }

    SDL_Event event;
    do {

        SDL_Flip(screen);
        SDL_LockSurface(screen);

        const VisualMemory::recallVector_t &recall = vismem.recall();

        // Draw camera image
        int pitch = screen->pitch / 4;
        for (unsigned i = 0; i < CameraSampler::kSamples; i++) {
            int x = 1 + CameraSampler::sampleX(i);
            int y = 1 + CameraSampler::sampleY(i);
            uint8_t s = vismem.samples()[i];

            uint32_t rgba = (s << 24) | (s << 16) | (s << 8) | s;
            uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;
            pixel[pitch] = pixel[1] = pixel[0] = pixel[-1] = pixel[-pitch] = rgba;
        }
 
        // Draw recall buffer
        unsigned left = 750;

        VisualMemory::memory_t scale = 1.0 / 4;

        for (unsigned i = 0; i < screen->h && i < recall.size(); i++) {

            double r = i < recall.size() ? scale * recall[i] : 0;
   
            unsigned s = std::min<int>(255, std::max<int>(0, r * 255.0)); 
            uint32_t rgba = (s << 24) | (s << 16) | (s << 8) | s;
            uint32_t *pixel = pitch*i + (uint32_t*)screen->pixels;
            unsigned bar = std::min(double(screen->w), (screen->w - left) * r + left);
            for (unsigned x = left; x < screen->w; x++) {
                pixel[x] = x < bar ? rgba : 0;
            }
        }

        SDL_UnlockSurface(screen);
        usleep(10000);

        while (SDL_PollEvent(&event) && event.type != SDL_QUIT);
    } while (event.type != SDL_QUIT);
}

int main(int argc, char **argv)
{
    Camera::start(videoCallback);

    SpokesEffect spokes;
    mixer.add(&spokes, 0.9);

    // RingsEffect rings("data/glass.png");
    // mixer.add(&rings, 0.5);

    ReactEffect react(&vismem);
    mixer.add(&react, 0.2);

    tap.setEffect(&mixer);
    runner.setEffect(&tap);
    runner.setLayout("layouts/window6x12.json");
    if (!runner.parseArguments(argc, argv)) {
        return 1;
    }

    // Init visual memory, now that the layout is known
    vismem.start("imprint.mem", &runner, &tap);

    new tthread::thread(debugThread, 0);
    new tthread::thread(effectThread, 0);
    sdlThread();
    return 0;
}
