#include "GameRand.h"

#include <cstdlib>
#include <random>

namespace {
constexpr float kRandMaxF = static_cast<float>(RAND_MAX);
} // namespace

float RandF(float lo, float hi) {
    return lo + static_cast<float>(rand()) / kRandMaxF * (hi - lo);
}

float SeededUnitFloat(unsigned seed, unsigned salt) {
    std::mt19937 rng(seed ^ salt);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}
