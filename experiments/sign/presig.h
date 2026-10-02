#ifndef KS_SIGN_PRESIG_H
#define KS_SIGN_PRESIG_H

#include "util.h"

/**
 * @brief A dummy presignature bundle.
 */
struct PreSigBundle {
  /**
   * @brief Create a new dummy pre-signature bundle for n parties.
   * @param n the number of parties.
   */
  static PreSigBundle create(std::size_t n);

  /**
   * @brief Pre-signatures.
   */
  std::vector<PreSig> presigs;

  /**
   * @brief Public key.
   */
  Curve pk;
};

#endif  // KS_SIGN_PRESIG_H
