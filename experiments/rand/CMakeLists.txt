cmake_minimum_required(VERSION 3.5)

## P_rand implementation
add_executable(rand.x
  main.cc
  gf16.cc
  ctx.cc
  enc.cc
  manager.cc
  rand.cc
  rand_agree.cc
  ${DUMMY_BC})
target_link_libraries(rand.x ${SCL} ${GMP})

## Test for the error correcting codes used in P_rand
add_executable(test_enc.x
  test_enc.cc
  enc.cc
  gf16.cc)
target_link_libraries(test_enc.x ${SCL} ${GMP})
