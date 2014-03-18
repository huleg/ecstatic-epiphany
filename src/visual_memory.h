/*
 * Experimental learning algorithm
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <string>
#include <vector>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include "lib/camera_sampler.h"
#include "lib/effect_runner.h"
#include "lib/jpge.h"
#include "lib/lodepng.h"
#include "latency_timer.h"


class VisualMemory
{
public:
    typedef double memory_t;
    typedef std::vector<memory_t> recallVector_t;

    // Starts a dedicated processing thread
    void start(const char *memoryPath, const EffectRunner *runner, const EffectTap *tap);

    // Handle incoming video
    void process(const Camera::VideoChunk &chunk);

    // Snapshot memory state as a PNG file
    void debug(const char *outputPngFilename) const;

    // Buffer of current memory recall, by LED pixel index
    const recallVector_t& recall() const;

    // Update the memory recall buffer. Call once per LED / Camera frame.
    void updateRecall();

    // Buffer of camera samples
    const uint8_t *samples() const;

    // Sobel filter, with camera motion and edge detection info
    CameraSamplerSobel sobel;

private:
    struct Cell {
        memory_t shortTerm;
        memory_t longTerm;
    };

    typedef std::vector<Cell> memoryVector_t;

    // Mapped memory buffer, updated on the learning thread
    Cell *mappedMemory;
    unsigned mappedMemorySize;

    // Updated by updateRecall()
    recallVector_t recallBuffer;
    recallVector_t recallAccumulator;
    recallVector_t recallTolerance;

    const EffectTap *tap;
    std::vector<unsigned> denseToSparsePixelIndex;

    // Updated on the video thread constantly, via process()
    uint8_t sampleBuffer[CameraSampler8Q::kSamples];

    // Separate learning thread
    tthread::thread *learnThread;
    static void learnThreadFunc(void *context);

    static const memory_t kLearningThreshold = 0.5;
    static const memory_t kMotionThreshold = 0.1;
    static const memory_t kShortTermPermeability = 1e-1;
    static const memory_t kLongTermPermeability = 1e-4;
    static const memory_t kToleranceRate = 2e-3;

    // Main loop for learning thread
    void learnWorker();
    memory_t reinforcementFunction(int luminance, Vec3 led);
};


// Simple effect that visualizes recall data directly
class RecallDebugEffect : public Effect
{
public:
    RecallDebugEffect(VisualMemory *mem, double sensitivity = -8.0)
        : mem(mem), sensitivity(sensitivity) {}

    VisualMemory *mem;
    double sensitivity;

    virtual void shader(Vec3& rgb, const PixelInfo &p) const
    {
        float f = std::min(1.0, std::max(0.0, 0.5 + mem->recall()[p.index] * sensitivity));
        rgb = Vec3(f,f,f);
    }
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline void VisualMemory::start(const char *memoryPath, const EffectRunner *runner, const EffectTap *tap)
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
    unsigned cells = CameraSampler8Q::kSamples * denseSize;
    fprintf(stderr, "vismem: %d camera samples * %d LED pixels = %d cells\n",
        CameraSampler8Q::kSamples, denseSize, cells);

    // Recall and camera buffers

    recallBuffer.resize(pixelInfo.size());
    recallAccumulator.resize(denseSize);
    recallTolerance.resize(denseSize);

    std::fill(recallTolerance.begin(), recallTolerance.end(), 1.0);

    memset(sampleBuffer, 0, sizeof sampleBuffer);

    // Memory mapped file

    int fd = open(memoryPath, O_CREAT | O_RDWR | O_NOFOLLOW, 0666);
    if (fd < 0) {
        perror("vismem: Error opening mapping file");
        return;
    }

    if (ftruncate(fd, cells * sizeof(Cell))) {
        perror("vismem: Error setting length of mapping file");
        close(fd);
        return;
    }

    mappedMemorySize = cells;
    mappedMemory = (Cell*) mmap(0, cells * sizeof(Cell),
        PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);

    if (!mappedMemory) {
        perror("vismem: Error mapping memory file");
        close(fd);
        return;
    }

    // Scrub any non-finite values out of memory

    for (unsigned i = 0; i != cells; i++) {
        if (!isfinite(mappedMemory[i].longTerm)) {
            mappedMemory[i].longTerm = 0;
        }
        if (!isfinite(mappedMemory[i].shortTerm)) {
            mappedMemory[i].shortTerm = 0;
        }
    }

    // Let the thread loose. This starts learning right away- no other thread should be
    // writing to the memory buffer from now on.

    learnThread = new tthread::thread(learnThreadFunc, this);
}

inline const VisualMemory::recallVector_t& VisualMemory::recall() const
{
    return recallBuffer;
}

inline const uint8_t* VisualMemory::samples() const
{
    return sampleBuffer;
}

inline void VisualMemory::process(const Camera::VideoChunk &chunk)
{
    // Store samples for the learning thread to process
    unsigned sampleIndex;
    uint8_t luminance;
    CameraSampler8Q s8q(chunk);
    while (s8q.next(sampleIndex, luminance)) {
        sampleBuffer[sampleIndex] = luminance;
    }

    // Compute kernel functions for motion detection
    sobel.process(chunk);
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
    Cell *memoryBuffer = mappedMemory;

    // Performance counters
    unsigned loopCount = 0;
    struct timeval timeA, timeB;

    unsigned denseSize = denseToSparsePixelIndex.size();

    gettimeofday(&timeA, 0);
    gettimeofday(&timeB, 0);

    // Keep iterating over the memory buffer in the order it's stored
    while (true) {

        for (unsigned sampleIndex = 0; sampleIndex != CameraSampler8Q::kSamples; sampleIndex++) {
            uint8_t luminance = sampleBuffer[sampleIndex];

            // Compute maximum possible reinforcement from this luminance; if it's below the
            // learning threshold, we can skip the entire sample.
            if (reinforcementFunction(luminance, Vec3(1,1,1)) < kLearningThreshold) {
                continue;
            }

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
                Cell state = *cell;

                memory_t reinforcement = reinforcementFunction(luminance, pixel);
                if (reinforcement >= kLearningThreshold) {

                    // Short term learning: Fixed decay rate at each access, additive reinforcement
                    state.shortTerm = (state.shortTerm - state.shortTerm * kShortTermPermeability) + reinforcement;

                    // Long term learning: Nonlinear; coarse approximation at high distances, resolve finer
                    // details once the gap narrows.

                    double r = double(state.shortTerm) - double(state.longTerm);
                    state.longTerm += (r * r * r) * kLongTermPermeability;

                    *cell = state;
                }
            }
        }

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

inline void VisualMemory::updateRecall()
{
    const Cell *memoryBuffer = mappedMemory;
    unsigned denseSize = denseToSparsePixelIndex.size();

    double total = 0;
    std::fill(recallAccumulator.begin(), recallAccumulator.end(), 0);

    for (unsigned sampleIndex = 0; sampleIndex != CameraSampler8Q::kSamples; sampleIndex++) {

        float motion = sobel.motionMagnitude(sampleIndex);
        if (motion < kMotionThreshold) {
            continue;
        }

        const Cell* cell = &memoryBuffer[sampleIndex * denseSize];    
        for (unsigned denseIndex = 0; denseIndex != denseSize; denseIndex++, cell++) {
            unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];

            double a = motion * cell->longTerm;
            recallAccumulator[sparseIndex] += a;
            total += a;
        }
    }

    double scale = denseSize / total;

    for (unsigned denseIndex = 0; denseIndex != denseSize; denseIndex++) {
        unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];
        recallBuffer[sparseIndex] = recallAccumulator[denseIndex] * scale - 1.0;
    }
}

inline void VisualMemory::debug(const char *outputPngFilename) const
{
    unsigned denseSize = denseToSparsePixelIndex.size();
    const Cell *memoryBuffer = mappedMemory;
    size_t memoryBufferSize = mappedMemorySize;

    // Tiled array of camera samples, one per LED. Artificial square grid of LEDs.
    const int ledsWide = int(ceilf(sqrt(denseSize)));
    const int width = ledsWide * CameraSampler8Q::kBlocksWide;
    const int ledsHigh = (denseToSparsePixelIndex.size() + ledsWide - 1) / ledsWide;
    const int height = ledsHigh * CameraSampler8Q::kBlocksHigh;
    std::vector<uint8_t> image;
    image.resize(width * height * 3);

    // Extents, using long-term memory to set expected range
    memory_t cellMax = memoryBuffer[0].longTerm;
    for (unsigned c = 1; c < memoryBufferSize; c++) {
        cellMax = std::max(cellMax, memoryBuffer[c].longTerm);
    }
    memory_t cellScale = 1.0 / cellMax;

    fprintf(stderr, "vismem: range %f\n", cellMax);

    for (unsigned sample = 0; sample < CameraSampler8Q::kSamples; sample++) {
        for (unsigned led = 0; led < denseSize; led++) {

            int sx = CameraSampler8Q::blockX(sample);
            int sy = CameraSampler8Q::blockY(sample);

            int x = sx + (led % ledsWide) * CameraSampler8Q::kBlocksWide;
            int y = sy + (led / ledsWide) * CameraSampler8Q::kBlocksHigh;

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
