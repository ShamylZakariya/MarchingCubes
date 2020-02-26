#ifndef xorshift_hpp
#define xorshift_hpp

#include <stdint.h>

// https://en.wikipedia.org/wiki/Xorshift

struct xorshift32_state {
    uint32_t a;
};

/* The state word must be initialized to non-zero */
uint32_t xorshift32(struct xorshift32_state* state)
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    uint32_t x = state->a;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state->a = x;
}

struct xorshift64_state {
    uint64_t a;
};

uint64_t xorshift64(struct xorshift64_state* state)
{
    uint64_t x = state->a;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return state->a = x;
}

struct xorshift128_state {
    uint32_t a, b, c, d;
};

/* The state array must be initialized to not be all zero */
uint32_t xorshift128(struct xorshift128_state* state)
{
    /* Algorithm "xor128" from p. 5 of Marsaglia, "Xorshift RNGs" */
    uint32_t t = state->d;

    uint32_t const s = state->a;
    state->d = state->c;
    state->c = state->b;
    state->b = s;

    t ^= t << 11;
    t ^= t >> 8;
    return state->a = t ^ s ^ (s >> 19);
}

/*
rng_xorshift64
Helper which wraps xorshift64_state and xorshift64() into a single struct
*/
struct rng_xorshift64 {
private:
    xorshift64_state _state;

public:
    rng_xorshift64()
    {
        _state.a = 12345;
    }
    rng_xorshift64(uint64_t seed)
    {
        _state.a = seed;
    }

    int nextInt(int min, int max)
    {
        const auto r = xorshift64(&_state);
		constexpr auto range = 99999;
        const int v = r % range;
		const float t = static_cast<float>(v) / static_cast<float>(range);
		return min + static_cast<int>(t * (max - min));
    }

    int nextInt(int max)
    {
        const auto r = xorshift64(&_state);
		constexpr auto range = 99999;
        const int v = r % range;
		const float t = static_cast<float>(v) / static_cast<float>(range);
		return static_cast<int>(t * max);
    }

    float nextFloat(float min, float max)
    {
        const auto r = xorshift64(&_state);
		constexpr auto range = 99999;
        const int v = r % range;
		const float t = static_cast<float>(v) / static_cast<float>(range);
        auto f = min + (t * (max - min));
        return f;
    }

    float nextFloat(int max)
    {
        const auto r = xorshift64(&_state);
		constexpr auto range = 99999;
        const int v = r % range;
		const float t = static_cast<float>(v) / static_cast<float>(range);
        auto f = t * max;
        return f;
    }
};

#endif