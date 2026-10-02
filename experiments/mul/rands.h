#ifndef KS_MUL_RANDS_H
#define KS_MUL_RANDS_H

#include <vector>

#include "util.h"

/**
 * @brief A collection of random secret-shared elements.
 */
struct RandBundle {
  /**
   * @brief Create a rand bundle.
   * @param number_of_parties the number of parties.
   * @param number_of_muls the number of multiplications.
   */
  static RandBundle create(std::size_t number_of_parties,
                           std::size_t number_of_muls);

  // A list of t = n/3 random elements.
  std::vector<Field> as_clear;

  // Another list of t = n/3 random elements.
  std::vector<Field> bs_clear;

  // Pedersen secret-shares of as_clear
  std::vector<std::vector<PVSS>> as;

  // Pedersen secret-shares of bs_clear
  std::vector<std::vector<PVSS>> bs;
};

/**
 * @brief A collection of double-shared random elements.
 */
struct DoubleRandBundle {
  /**
   * @brief Create a double-shared bundle.
   * @param number_of_parties the number of parties.
   * @param number_of_muls the number of multiplications.
   */
  static DoubleRandBundle create(std::size_t number_of_parties,
                                 std::size_t number_of_muls);

  // shares of degree t
  std::vector<std::vector<PVSS>> r1s;

  // shares of degree 2t
  std::vector<std::vector<PVSS>> r2s;
};

#endif  // KS_MUL_RANDS_H
