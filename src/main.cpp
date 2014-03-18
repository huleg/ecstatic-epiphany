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

#include "visual_memory.h"
#include "reactive_rings.h"
#include "spokes.h"

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
        int pitch = screen->pitch / 4;

        // Draw motion detector image
        for (unsigned y = 0, i = 0; y < Camera::kLinesPerFrame; y++) {
            for (unsigned x = 0; x < Camera::kPixelsPerLine; x++, i++) {

                int s = std::min(255, std::max<int>(0, 10 * vismem.sobel.motionMagnitude(i)));

                uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;
                *pixel = s << 8;   // Red
            }
        }

        // Draw camera image
        for (unsigned i = 0; i < CameraSampler8Q::kSamples; i++) {
            int x = CameraSampler8Q::sampleX(i);
            int y = CameraSampler8Q::sampleY(i);
            uint8_t s = vismem.samples()[i];

            uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;
            *pixel |= (s << 24) | (s << 16);  // Green / Blue
        }

        // Draw recall buffer
        unsigned left = 750;

        for (unsigned i = 0; i < screen->h && i < recall.size(); i++) {

            double r = i < recall.size() ? 0.5 + recall[i] : 0;
   
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
    mixer.add(&spokes, 1.0);

    ReactiveRingsEffect rings("data/glass.png", &vismem);
    mixer.add(&rings, 0.2);

    // RecallDebugEffect recallDebug(&vismem);
    // mixer.add(&recallDebug, 1.0);

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
