#include <scl/math/matrix.h>

#include "util.h"

struct GlobalState {
  scl::math::Matrix<Field> Van_shrs;
  scl::math::Matrix<Curve> Van_comm;
};

void initializeGlobalState(std::size_t number_of_parties);

const extern GlobalState* STATE;
