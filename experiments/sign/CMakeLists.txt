cmake_minimum_required(VERSION 3.14)

## P_sign protocol implementation
add_executable(sign.x
  main.cc 
  presig.cc 
  signer.cc 
  manager.cc)
target_link_libraries(sign.x ${SCL} ${GMP})
