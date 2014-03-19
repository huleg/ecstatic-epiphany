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
static SpokesEffect spokes;
static ReactiveRingsEffect rings("data/glass.png", &vismem);
static RecallDebugEffect recallDebug(&vismem);


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
    float phase = 0;
    const float rate = 0.1;

    while (true) {
        float dt = runner.doFrame();

        phase = fmodf(phase + rate * dt, 2*M_PI);
        // float f = std::max(0.0, sin(phase));
        // mixer.setFader(0, f);
        // mixer.setFader(1, 1 - f);
    }
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

        const VisualMemory::memoryVector_t &recall = vismem.recall();
        int pitch = screen->pitch / 4;

        // Draw camera image with motion detection
        for (unsigned i = 0; i < CameraSampler8Q::kSamples; i++) {
            int x = 1 + CameraSampler8Q::sampleX(i);
            int y = 1 + CameraSampler8Q::sampleY(i);

            uint8_t s = vismem.luminance.buffer[i];
            uint8_t m = std::min(255, std::max<int>(0, 0.1 * vismem.sobel.motion[i]));
            uint8_t l = vismem.learnFlags[i] ? 0xFF : 0;

            uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;

            // Pack lots of debug info into RGB channels...
            uint32_t bgra = (m << 8) | (l << 16) | (s << 24);

            // Splat
            pixel[-pitch] = pixel[1] = pixel[0] = pixel[-1] = pixel[pitch] = bgra;
        }

        // Draw recall buffer
        unsigned left = 750;
        unsigned top = 400;

        for (unsigned i = 0; i < screen->h && i < recall.size(); i++) {

            // Convert to RGB color
            double r = i < recall.size() ? 0.5 + recall[i] : 0;   
            unsigned s = std::min<int>(255, 255 * std::max(0.0, r*r*r)); 
            uint32_t bgra = (s << 24) | (s << 16) | (s << 8);

            // Bars
            uint32_t *pixel = pitch*i + (uint32_t*)screen->pixels;
            unsigned bar = std::min(double(screen->w), (screen->w - left) * r + left);
            for (unsigned x = left; x < screen->w; x++) {
                pixel[x] = x < bar ? bgra : 0;
            }

            // XZ plane
            Vec3 point = runner.getPixelInfo()[i].point;
            int scale = -80;
            int x = screen->w/2 + point[0] * scale;
            int y = (screen->h + top) / 2 + point[2] * scale;
            if (x > 1 && x < screen->w - 1 && y > 1 && y < screen->h - 1) {
                uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;
            
                // Splat
                pixel[-pitch] = pixel[1] = pixel[0] = pixel[-1] = pixel[pitch] = bgra;
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

    // mixer.add(&spokes, 0.2);
    // mixer.add(&recallDebug);
    mixer.add(&rings);

    tap.setEffect(&mixer);
    runner.setEffect(&tap);
    // runner.setLayout("layouts/window6x12.json");
    runner.setLayout("layouts/grid32x16z.json");
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
