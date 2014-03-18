/*
 * Convolution kernel operating on real-time camera data.
 * We use integer math, maintaining a buffer with the current state
 * of the filter, updated on every new camera data packet.
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


class CameraKernel
{
public:
    void process(unsigned index, int delta);
    const int *results() const;

protected:
    CameraKernel(int size);

    std::vector<int> coefficients;
    std::vector<int> positions;
    int buffer[Camera::kPixels];
};


class CameraKernelGabor : public CameraKernel
{
public:
    CameraKernelGabor(int size, float angle, float wavelength, float sigma = 1.0, float phase = 0.0);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline CameraKernel::CameraKernel(int size)
{
        printf("zzz\n");

    int s2 = size * size;
    coefficients.resize(s2);
    positions.resize(s2);
    memset(buffer, 0, sizeof buffer);

    // Calculate kernel sampling positions
    for (int i = 0; i < s2; i++) {
        int x = (i % size) - size/2;
        int y = (i / size) - size/2;
        positions[i] = x + y * Camera::kPixelsPerLine;
    }
}

inline const int* CameraKernel::results() const
{
    return buffer;
}

inline void CameraKernel::process(unsigned index, int delta)
{
    if (delta) {
        for (int i = 0; i < positions.size(); i++) {
            unsigned p = positions[i] + index;
            int c = coefficients[i];
            if (p < Camera::kPixels && c) {
                buffer[p] += delta * c;
            }
        }
    }
}

inline CameraKernelGabor::CameraKernelGabor(int size, float angle, float wavelength, float sigma, float phase)
    : CameraKernel(size)
{
    int sum = 0;
    for (int i = 0; i < positions.size(); i++) {
        float x = ((i % size) - size/2) / ((size - 1) / 2.0f);
        float y = ((i / size) - size/2) / ((size - 1) / 2.0f);

        float xr = x * cos(angle) + y * sin(angle);
        float yr = -x * sin(angle) + y * cos(angle);

        float v = exp(-(xr*xr + yr*yr) / (2 * sigma * sigma)) * cos(2*M_PI * xr / wavelength + phase);

        coefficients[i] = floor(v * 8192.0f);
        sum += coefficients[i];
        printf("[%d] (%f, %f) -> %d\n", i, x, y, coefficients[i]);
    }
    printf("sum = %d\n", sum);
}
