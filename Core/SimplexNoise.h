#pragma once

#include <cstddef>  // size_t

#include "Math/Random.h"

static Math::RandomNumberGenerator random;
/**
 * @brief A Perlin Simplex Noise C++ Implementation (1D, 2D, 3D, 4D).
 */
class SimplexNoise {
public:

    int32_t fastfloor(float fp);
    int32_t fastfloor(float fp) const;
    static int seed;
    static void setSeed(int num);
    uint8_t hash(int32_t i) const;
    uint8_t hash(int32_t i);
    float grad(int32_t hash, float x) const;
    float grad(int32_t hash, float x, float y) const;
    float grad(int32_t hash, float x, float y, float z) const;
    // 1D Perlin simplex noise
    float noise(float x) const;
    // 2D Perlin simplex noise
    float noise(float x, float y) const;
    // 3D Perlin simplex noise
    float noise(float x, float y, float z) const;

    // Fractal/Fractional Brownian Motion (fBm) noise summation
    float fractal(size_t octaves, float x) const;
    float fractal(size_t octaves, float x, float y) const;
    float fractal(size_t octaves, float x, float y, float z) const;

    /**
     * Constructor of to initialize a fractal noise summation
     *
     * @param[in] frequency    Frequency ("width") of the first octave of noise (default to 1.0)
     * @param[in] amplitude    Amplitude ("height") of the first octave of noise (default to 1.0)
     * @param[in] lacunarity   Lacunarity specifies the frequency multiplier between successive octaves (default to 2.0).
     * @param[in] persistence  Persistence is the loss of amplitude between successive octaves (usually 1/lacunarity)
     */
    SimplexNoise(float frequency = 1.0f,
                          float amplitude = 1.0f,
                          float lacunarity = 2.0f,
                          float persistence = 0.5f) :
        mFrequency(frequency),
        mAmplitude(amplitude),
        mLacunarity(lacunarity),
        mPersistence(persistence) {
        for (auto& m_perm : perm)
        {
            m_perm = random.NextInt(255);
        }
    }

private:
    // Parameters of Fractional Brownian Motion (fBm) : sum of N "octaves" of noise
    float mFrequency;   ///< Frequency ("width") of the first octave of noise (default to 1.0)
    float mAmplitude;   ///< Amplitude ("height") of the first octave of noise (default to 1.0)
    float mLacunarity;  ///< Lacunarity specifies the frequency multiplier between successive octaves (default to 2.0).
    float mPersistence; ///< Persistence is the loss of amplitude between successive octaves (usually 1/lacunarity)
    uint8_t perm[256];
};
