
// Adapted from: https://github.com/Reputeless/PerlinNoise/

//----------------------------------------------------------------------------------------
//
//	siv::PerlinNoise
//	Perlin noise library for modern C++
//
//	Copyright (C) 2013-2019 Ryo Suzuki <reputeless@gmail.com>
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files(the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions :
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.
//
//----------------------------------------------------------------------------------------

#ifndef perlin_noise_hpp
#define perlin_noise_hpp

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <random>
#include <type_traits>

class PerlinNoise {
private:
    std::uint8_t p[512];

    static float Fade(float t) noexcept
    {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    static float Lerp(float t, float a, float b) noexcept
    {
        return a + t * (b - a);
    }

    static float Grad(std::uint8_t hash, float x, float y, float z) noexcept
    {
        const std::uint8_t h = hash & 15;
        const float u = h < 8 ? x : y;
        const float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

public:
    explicit PerlinNoise(std::uint32_t seed = std::default_random_engine::default_seed)
    {
        reseed(seed);
    }

    template <class URNG, std::enable_if_t<!std::is_arithmetic_v<URNG>>* = nullptr>
    explicit PerlinNoise(URNG& urng)
    {
        reseed(urng);
    }

    void reseed(std::uint32_t seed)
    {
        for (size_t i = 0; i < 256; ++i) {
            p[i] = static_cast<std::uint8_t>(i);
        }

        std::shuffle(std::begin(p), std::begin(p) + 256, std::default_random_engine(seed));

        for (size_t i = 0; i < 256; ++i) {
            p[256 + i] = p[i];
        }
    }

    template <class URNG, std::enable_if_t<!std::is_arithmetic_v<URNG>>* = nullptr>
    void reseed(URNG& urng)
    {
        for (size_t i = 0; i < 256; ++i) {
            p[i] = static_cast<std::uint8_t>(i);
        }

        std::shuffle(std::begin(p), std::begin(p) + 256, urng);

        for (size_t i = 0; i < 256; ++i) {
            p[256 + i] = p[i];
        }
    }

    float noise(float x) const
    {
        return noise(x, 0.0, 0.0);
    }

    float noise(float x, float y) const
    {
        return noise(x, y, 0.0);
    }

    float noise(float x, float y, float z) const
    {
        const std::int32_t X = static_cast<std::int32_t>(std::floor(x)) & 255;
        const std::int32_t Y = static_cast<std::int32_t>(std::floor(y)) & 255;
        const std::int32_t Z = static_cast<std::int32_t>(std::floor(z)) & 255;

        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);

        const float u = Fade(x);
        const float v = Fade(y);
        const float w = Fade(z);

        const std::int32_t A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
        const std::int32_t B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

        return Lerp(w, Lerp(v, Lerp(u, Grad(p[AA], x, y, z), Grad(p[BA], x - 1, y, z)), Lerp(u, Grad(p[AB], x, y - 1, z), Grad(p[BB], x - 1, y - 1, z))),
            Lerp(v, Lerp(u, Grad(p[AA + 1], x, y, z - 1), Grad(p[BA + 1], x - 1, y, z - 1)),
                Lerp(u, Grad(p[AB + 1], x, y - 1, z - 1),
                    Grad(p[BB + 1], x - 1, y - 1, z - 1))));
    }

    float octaveNoise(float x, std::int32_t octaves) const
    {
        float result = 0.0;
        float amp = 1.0;

        for (std::int32_t i = 0; i < octaves; ++i) {
            result += noise(x) * amp;
            x *= 2.0;
            amp *= 0.5;
        }

        return result;
    }

    float octaveNoise(float x, float y, std::int32_t octaves) const
    {
        float result = 0.0;
        float amp = 1.0;

        for (std::int32_t i = 0; i < octaves; ++i) {
            result += noise(x, y) * amp;
            x *= 2.0;
            y *= 2.0;
            amp *= 0.5;
        }

        return result;
    }

    float octaveNoise(float x, float y, float z, std::int32_t octaves) const
    {
        float result = 0.0;
        float amp = 1.0;

        for (std::int32_t i = 0; i < octaves; ++i) {
            result += noise(x, y, z) * amp;
            x *= 2.0;
            y *= 2.0;
            z *= 2.0;
            amp *= 0.5;
        }

        return result;
    }

    float noise0_1(float x) const
    {
        return noise(x) * 0.5 + 0.5;
    }

    float noise0_1(float x, float y) const
    {
        return noise(x, y) * 0.5 + 0.5;
    }

    float noise0_1(float x, float y, float z) const
    {
        return noise(x, y, z) * 0.5 + 0.5;
    }

    float octaveNoise0_1(float x, std::int32_t octaves) const
    {
        return octaveNoise(x, octaves) * 0.5 + 0.5;
    }

    float octaveNoise0_1(float x, float y, std::int32_t octaves) const
    {
        return octaveNoise(x, y, octaves) * 0.5 + 0.5;
    }

    float octaveNoise0_1(float x, float y, float z, std::int32_t octaves) const
    {
        return octaveNoise(x, y, z, octaves) * 0.5 + 0.5;
    }
};

#endif