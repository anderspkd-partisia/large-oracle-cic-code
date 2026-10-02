#ifndef KS_MUL_DELAY_SAMPLER_H
#define KS_MUL_DELAY_SAMPLER_H

#include <random>

#include <scl/util/measurement.h>
#include <scl/util/time.h>

/**
 * @brief Sampler used by the fake prover/verifier.
 *
 * A DelaySampler simply samples from a normal distribution with some mean and
 * standard deviation.
 */
struct DelaySampler {
  /**
   * @brief Create a new sampler.
   * @param m a measurement to create the sampler from.
   */
  DelaySampler(const scl::util::TimeMeasurement& m)
      : rand(std::default_random_engine(0)),
        dist(std::normal_distribution<long double>(
            scl::util::timeToMillis(m.mean()),
            scl::util::timeToMillis(m.stddev()))) {}

  std::default_random_engine rand;
  std::normal_distribution<long double> dist;

  /**
   * @brief Re-seed the sampler.
   * @param seed the seed.
   */
  void reSeed(unsigned seed) {
    rand = std::default_random_engine(seed);
  }

  /**
   * @brief Get another sample.
   */
  scl::util::Time::Duration get() {
    const auto d = std::chrono::duration<long double, std::milli>(dist(rand));
    const auto dd = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    return dd;
  }
};

#endif  // KS_MUL_DELAY_SAMPLER_H
