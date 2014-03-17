/*
 * Experimental learning algorithm
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include <sys/time.h>
#include "lib/camera_sampler.h"
#include "lib/effect_runner.h"
#include "lib/jpge.h"
#include "lib/lodepng.h"
#include "lib/prng.h"
#include "latencytimer.h"


class VisualMemory
{
public:
    typedef double memory_t;
    typedef std::vector<memory_t> recallVector_t;

    // Starts a dedicated processing thread
    void start(const EffectRunner *runner, const EffectTap *tap);

    // Handle incoming video
    void process(const Camera::VideoChunk &chunk);

    // Snapshot memory state as a PNG file
    void debug(const char *outputPngFilename) const;

    // Buffer of current memory recall, by LED pixel index
    const recallVector_t& recall() const;

private:
    struct Cell {
        memory_t shortTerm;
        memory_t longTerm;
    };

    typedef std::vector<Cell> memoryVector_t;

    // Updated on the learning thread constantly
    memoryVector_t memoryBuffer;
    recallVector_t recallBuffer;

    PRNG random;
    const EffectTap *tap;
    std::vector<unsigned> denseToSparsePixelIndex;

    // Updated on the video thread constantly, via process()
    uint8_t samples[CameraSampler::kSamples];

    // Separate learning thread
    tthread::thread *learnThread;
    static void learnThreadFunc(void *context);

    static const memory_t kLearningThreshold = 0.1;
    static const memory_t kShortTermPermeability = 1e-1;
    static const memory_t kLongTermPermeability = 1e-4;

    // Main loop for learning thread
    void learnWorker();
    memory_t reinforcementFunction(int luminance, Vec3 led);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline void VisualMemory::start(const EffectRunner *runner, const EffectTap *tap)
{
    this->tap = tap;
    const Effect::PixelInfoVec &pixelInfo = runner->getPixelInfo();

    // Make a densely packed pixel index, skipping any unmapped pixels

    denseToSparsePixelIndex.clear();
    for (unsigned i = 0; i < pixelInfo.size(); ++i) {
        const Effect::PixelInfo &pixel = pixelInfo[i];
        if (pixel.isMapped()) {
            denseToSparsePixelIndex.push_back(i);
        }
    }

    // Calculate size of full visual memory

    unsigned denseSize = denseToSparsePixelIndex.size();
    unsigned cells = CameraSampler::kSamples * denseSize;
    fprintf(stderr, "vismem: %d camera samples * %d LED pixels = %d cells\n",
        CameraSampler::kSamples, denseSize, cells);

    memoryBuffer.resize(cells);
    recallBuffer.resize(pixelInfo.size());
    random.seed(27);
    memset(samples, 0, sizeof samples);

    // Let the thread loose. This starts learning right away- no other thread should be
    // writing to memoryBuffer. from now on.

    learnThread = new tthread::thread(learnThreadFunc, this);
}

inline const VisualMemory::recallVector_t& VisualMemory::recall() const
{
    return recallBuffer;
}

inline void VisualMemory::process(const Camera::VideoChunk &chunk)
{
    CameraSampler sampler(chunk);
    unsigned sampleIndex;
    uint8_t luminance;

    while (sampler.next(sampleIndex, luminance)) {
        samples[sampleIndex] = luminance;
    }
}

inline void VisualMemory::learnThreadFunc(void *context)
{
    VisualMemory *self = static_cast<VisualMemory*>(context);
    self->learnWorker();
}

inline VisualMemory::memory_t VisualMemory::reinforcementFunction(int luminance, Vec3 led)
{
    Real r = std::min(1.0f, led[0]);
    Real g = std::min(1.0f, led[1]);
    Real b = std::min(1.0f, led[2]);

    memory_t lumaSq = (luminance * luminance) / 65025.0;
    memory_t pixSq = (r*r + g*g + b*b) / 3.0f;

    return lumaSq * pixSq;
}

inline void VisualMemory::learnWorker()
{
    // Performance counters
    unsigned loopCount = 0;
    struct timeval timeA, timeB;

    unsigned denseSize = denseToSparsePixelIndex.size();
    recallVector_t recallAccumulator;
    recallAccumulator.resize(recallBuffer.size());
    gettimeofday(&timeA, 0);
    gettimeofday(&timeB, 0);

    // Keep iterating over the memory buffer in the order it's stored
    while (true) {

        // Once per iteration, calculate sums that will become recallBuffer
        memset(&recallAccumulator[0], 0, recallAccumulator.size() * sizeof recallAccumulator[0]);

        for (unsigned sampleIndex = 0; sampleIndex != CameraSampler::kSamples; sampleIndex++) {
            uint8_t luminance = samples[sampleIndex];

            // Compute maximum possible reinforcement from this luminance; if it's below the
            // learning threshold, we can skip the entire sample.
            if (reinforcementFunction(luminance, Vec3(1,1,1)) >= kLearningThreshold) {

                // Look up a delayed version of what the LEDs were doing then, to adjust for the system latency
                const EffectTap::Frame *effectFrame = tap->get(LatencyTimer::kExpectedDelay);
                if (!effectFrame) {
                    // This frame isn't in our buffer yet
                    usleep(10 * 1000);
                    continue;
                }

                Cell* cell = &memoryBuffer[sampleIndex * denseSize];
                for (unsigned denseIndex = 0; denseIndex != denseSize; denseIndex++, cell++) {
                    unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];
                    Vec3 pixel = effectFrame->colors[sparseIndex];

                    memory_t reinforcement = reinforcementFunction(luminance, pixel);

                    if (reinforcement >= kLearningThreshold) {
                        Cell state = *cell;

                        state.shortTerm = (state.shortTerm - state.shortTerm * kShortTermPermeability) + reinforcement;

                        if (!isfinite(state.shortTerm)) {
                            // Oops. Reset to zero
                            state.shortTerm = 0;
                        }

                        state.longTerm += (state.shortTerm - state.longTerm) * kLongTermPermeability;

                        if (!isfinite(state.longTerm)) {
                            state.longTerm = state.shortTerm;
                        }

                        // In all cells where we access the memory, recall is proportional to
                        // the difference between short term and long term state (novelty factor)
                        recallAccumulator[sparseIndex] += state.shortTerm - state.longTerm;

                        *cell = state;
                    }
                }
            }
        }

        recallBuffer = recallAccumulator;

        /*
         * Periodic performance stats
         */
        
        loopCount++;
        gettimeofday(&timeB, 0);
        double timeDelta = (timeB.tv_sec - timeA.tv_sec) + 1e-6 * (timeB.tv_usec - timeA.tv_usec);
        if (timeDelta > 2.0f) {
            fprintf(stderr, "vismem: %.02f cycles / second\n", loopCount / timeDelta);
            loopCount = 0;
            timeA = timeB;
        }
    }


}

inline void VisualMemory::debug(const char *outputPngFilename) const
{
    unsigned denseSize = denseToSparsePixelIndex.size();

    // Tiled array of camera samples, one per LED. Artificial square grid of LEDs.
    const int ledsWide = int(sqrt(denseSize));
    const int width = ledsWide * CameraSampler::kBlocksWide;
    const int ledsHigh = (denseToSparsePixelIndex.size() + ledsWide - 1) / ledsWide;
    const int height = ledsHigh * CameraSampler::kBlocksHigh;
    std::vector<uint8_t> image;
    image.resize(width * height * 3);

    // Extents, using long-term memory to set expected range
    memory_t cellMax = memoryBuffer[0].longTerm;
    for (unsigned c = 1; c < memoryBuffer.size(); c++) {
        cellMax = std::max(cellMax, memoryBuffer[c].longTerm);
    }
    memory_t cellScale = 1.0 / cellMax;

    fprintf(stderr, "vismem: range %f\n", cellMax);

    for (unsigned sample = 0; sample < CameraSampler::kSamples; sample++) {
        for (unsigned led = 0; led < denseSize; led++) {

            int sx = CameraSampler::blockX(sample);
            int sy = CameraSampler::blockY(sample);

            int x = sx + (led % ledsWide) * CameraSampler::kBlocksWide;
            int y = sy + (led / ledsWide) * CameraSampler::kBlocksHigh;

            const Cell &cell = memoryBuffer[ sample * denseSize + led ];
            uint8_t *pixel = &image[ 3 * (y * width + x) ];

            memory_t s = cell.shortTerm * cellScale;
            memory_t l = cell.longTerm * cellScale;

            int shortTermI = std::min<memory_t>(255.5f, s*s*s*s * 255.0f + 0.5f);
            int longTermI = std::min<memory_t>(255.5f, l*l*l*l * 255.0f + 0.5f);

            pixel[0] = shortTermI;
            pixel[1] = pixel[2] = longTermI;
        }
    }

    if (lodepng_encode_file(outputPngFilename, &image[0], width, height, LCT_RGB, 8)) {
        fprintf(stderr, "vismem: error saving %s\n", outputPngFilename);
    } else {
        fprintf(stderr, "vismem: saved %s\n", outputPngFilename);
    }
}
