cmake_minimum_required(VERSION 3.5)

find_library(PBCTS libpbcts.so HINTS ${PROJECT_SOURCE_DIR}/libs/ REQUIRED)

add_executable(pbcts.x
  main.cc
  manager.cc
  protocol.cc
  ${DUMMY_BC})
target_link_libraries(pbcts.x ${SCL} ${GMP} ${PBCTS})
