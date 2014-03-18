/*
 * Camera sampler layer: Ways of breaking down camera data into
 * precursors used by our other algorithms.
 *
 * Copyright (c) 2014 Micah Elizabeth Scott <micah@scanlime.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "camera.h"


/*
 * This picks out a relatively small number
 * of individual luminance pixels to sample. We use the symmetric
 * solution to the 8 queens problem, in order to use 1/8 the pixels
 * without losing any entire rows or columns.
 *
 * This class acts as a VideoChunk processor that emits callbacks
 * for each new sample it encounters. Video is arriving nearly in
 * real-time, each sample is processed as soon as its Isoc buffer
 * is available.
 */
class CameraSampler8Q
{
public:
    static const int kSamplesPerBlock = 8;
    static const int kBlocksWide = Camera::kPixelsPerLine / kSamplesPerBlock;
    static const int kBlocksHigh = Camera::kLinesPerFrame / kSamplesPerBlock;
    static const int kBlocks = kBlocksWide * kBlocksHigh;
    static const int kSamples = kBlocks * kSamplesPerBlock;

    CameraSampler8Q(const Camera::VideoChunk &chunk);

    /*
     * Look for a sample in the current chunk. Returns 'true' if the sample
     * was found, and sets 'index' and 'luminance'. Returns 'false' if no more
     * samples are in this chunk.
     */
    bool next(unsigned &index, uint8_t &luminance);

    // Calculate the position of a particular sample
    static int sampleX(unsigned index);
    static int sampleY(unsigned index);

    // Calculate the block position of a particular sample
    static int blockX(unsigned index);
    static int blockY(unsigned index);

private:
    Camera::VideoChunk iter;
    unsigned y;
    unsigned yIndex;

    static unsigned x8q(unsigned y);
};


/*
 * Sample every luminance pixel, but look at deltas between them.
 * This is a short-lived object, existing on the stack only during video
 * processing. The "Buffer" object holds persistent information.
 */
class CameraSamplerDelta
{
public:
    struct Buffer {
        uint8_t luminance[Camera::kPixels];
    };

    CameraSamplerDelta(const Camera::VideoChunk &chunk, Buffer &buffer);

    bool next(unsigned &index, int &delta);

private:
    Camera::VideoChunk iter;
    Buffer &buffer;
    unsigned y;
    unsigned yIndex;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline CameraSampler8Q::CameraSampler8Q(const Camera::VideoChunk &chunk)
    : iter(chunk),
      y(iter.line * Camera::kFields + iter.field),
      yIndex(y * kBlocksWide)
{}

inline unsigned CameraSampler8Q::x8q(unsigned y)
{
    // Tiny lookup table for the X offset of the Eight Queens
    // solution on row (Y % 8).
    return (0x24170635 >> ((y & 7) << 2)) & 7;
}   

inline int CameraSampler8Q::sampleX(unsigned index)
{
    return (blockX(index) << 3) + x8q(sampleY(index));
}

inline int CameraSampler8Q::blockX(unsigned index)
{
    return index % kBlocksWide;
}

inline int CameraSampler8Q::sampleY(unsigned index)
{
    return index / kBlocksWide;
}

inline int CameraSampler8Q::blockY(unsigned index)
{
    return sampleY(index) / kSamplesPerBlock;
}

inline bool CameraSampler8Q::next(unsigned &index, uint8_t &luminance)
{
    // Low four bits of the byteOffset we want (X coord + luminance byte)
    unsigned mask = x8q(y) * 2 + 1;

    while (iter.byteCount) {

        if ((iter.byteOffset & 0xF) == mask) {
            index = yIndex + (iter.byteOffset >> 4);
            luminance = *iter.data;

            iter.byteOffset++;
            iter.byteCount--;
            iter.data++;

            // Ready for more
            return true;
        }

        iter.byteOffset++;
        iter.byteCount--;
        iter.data++;
    }

    // Out of data
    return false;
}

inline CameraSamplerDelta::CameraSamplerDelta(const Camera::VideoChunk &chunk, Buffer &buffer)
    : iter(chunk),
      buffer(buffer),
      y(iter.line * Camera::kFields + iter.field),
      yIndex(y * Camera::kPixelsPerLine)
{}

inline bool CameraSamplerDelta::next(unsigned &index, int &delta)
{
    while (iter.byteCount) {

        if (iter.byteOffset & 1) {
            // Luminance

            index = yIndex + (iter.byteOffset >> 1);
            int prev = buffer.luminance[index];
            int next = *iter.data;

            buffer.luminance[index] = next;
            delta = next - prev;

            iter.byteOffset++;
            iter.byteCount--;
            iter.data++;

            // Ready for more
            return true;
        }

        iter.byteOffset++;
        iter.byteCount--;
        iter.data++;
    }

    // Out of data
    return false;
}
