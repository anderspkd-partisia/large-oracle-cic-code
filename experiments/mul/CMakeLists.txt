cmake_minimum_required(VERSION 3.5)

## P_mul implementation
add_executable(mul.x
  main.cc
  apocm.cc
  prover.cc
  verifier.cc
  rands.cc
  manager.cc
  protocol.cc
  ctx.cc)
target_link_libraries(mul.x ${SCL} ${GMP})

## Test for the APOCM ZK proof
add_executable(test_apocm.x
  test_apocm.cc
  apocm.cc
  rands.cc
  ctx.cc
  prover.cc
  verifier.cc)
target_link_libraries(test_apocm.x ${SCL} ${GMP})

## Benchmarks for the APOCM ZK proof
add_executable(benchmark_apocm.x
  benchmark_apocm.cc
  apocm.cc
  rands.cc
  ctx.cc
  prover.cc
  verifier.cc)
target_link_libraries(benchmark_apocm.x ${SCL} ${GMP})
