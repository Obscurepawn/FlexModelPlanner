cmake_minimum_required(VERSION 3.20)
project(FlexModelPlanner VERSION 1.0.0 LANGUAGES CXX)

set(FLEXMODEL_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(FLEXMODEL_TOP_LEVEL ON)
endif()
option(FLEXMODEL_BUILD_TESTS "Build tests" ${FLEXMODEL_TOP_LEVEL})
option(FLEXMODEL_BUILD_EXAMPLES "Build examples" ${FLEXMODEL_TOP_LEVEL})
option(FLEXMODEL_BUILD_BENCHMARKS "Build the optional comparison benchmark" OFF)
option(FLEXMODEL_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(flexmodel INTERFACE)
add_library(flexmodel::flexmodel ALIAS flexmodel)
target_compile_features(flexmodel INTERFACE cxx_std_20)
target_include_directories(flexmodel INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

function(flexmodel_executable name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE flexmodel::flexmodel)
  set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(FLEXMODEL_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
      target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    else()
      message(FATAL_ERROR "FLEXMODEL_SANITIZERS requires GCC or Clang with ASan/UBSan")
    endif()
  endif()
endfunction()

if(FLEXMODEL_BUILD_EXAMPLES)
  flexmodel_executable(recent_trade_vs_mid examples/recent_trade_vs_mid.cpp)
  flexmodel_executable(csv_replay examples/csv_replay.cpp)
endif()
if(FLEXMODEL_BUILD_BENCHMARKS)
  flexmodel_executable(dispatch_benchmark benchmarks/dispatch_benchmark.cpp)
  target_include_directories(dispatch_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif()

if(FLEXMODEL_BUILD_TESTS)
  enable_testing()
  file(GLOB test_sources CONFIGURE_DEPENDS tests/test_*.cpp)
  foreach(source IN LISTS test_sources)
    get_filename_component(name "${source}" NAME_WE)
    flexmodel_executable(${name} "${source}")
    add_test(NAME ${name} COMMAND ${name})
  endforeach()
  file(GLOB negative_sources CONFIGURE_DEPENDS tests/compile_fail/*.cpp)
  foreach(source IN LISTS negative_sources)
    get_filename_component(name "${source}" NAME_WE)
    file(STRINGS "${source}" expected_line REGEX "^// EXPECT_ERROR: " LIMIT_COUNT 1)
    if(NOT expected_line)
      message(FATAL_ERROR "Missing EXPECT_ERROR marker: ${source}")
    endif()
    string(REPLACE "// EXPECT_ERROR: " "" expected "${expected_line}")
    add_test(NAME compile_fail_${name}
      COMMAND ${CMAKE_COMMAND}
        "-DCASE_SOURCE=${source}" "-DEXPECTED=${expected}"
        "-DINCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include"
        "-DPROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/compile-fail/${name}"
        "-DPROBE_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileFailProject"
        "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DTOOLSET=${CMAKE_GENERATOR_TOOLSET}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}" "-DTOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}"
        "-DCONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCompileFail.cmake")
  endforeach()
  if(FLEXMODEL_BUILD_EXAMPLES)
    add_test(NAME example_recent_trade COMMAND recent_trade_vs_mid)
    set_tests_properties(example_recent_trade PROPERTIES PASS_REGULAR_EXPRESSION "1.66667")
    add_test(NAME replay_sample COMMAND csv_replay "${CMAKE_CURRENT_SOURCE_DIR}/examples/sample.csv")
    set_tests_properties(replay_sample PROPERTIES PASS_REGULAR_EXPRESSION "1.66667,1.66667")
    add_test(NAME replay_bad_data COMMAND csv_replay "${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid.csv")
    set_tests_properties(replay_bad_data PROPERTIES WILL_FAIL TRUE)
    add_test(NAME replay_input_validation
      COMMAND ${CMAKE_COMMAND} "-DPROGRAM=$<TARGET_FILE:csv_replay>"
        "-DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/csv-cases"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCsvReplay.cmake")
  endif()
  add_test(NAME install_consumer
    COMMAND ${CMAKE_COMMAND}
      "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DSMOKE_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallSmokeProject"
      "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DTOOLSET=${CMAKE_GENERATOR_TOOLSET}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DTOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}" "-DCONFIG=$<CONFIG>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckInstall.cmake")
endif()

install(TARGETS flexmodel EXPORT FlexModelPlannerTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT FlexModelPlannerTargets FILE FlexModelPlannerTargets.cmake
  NAMESPACE flexmodel:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlexModelPlanner)
configure_package_config_file(cmake/FlexModelPlannerConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/FlexModelPlannerConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlexModelPlanner)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/FlexModelPlannerConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FlexModelPlannerConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/FlexModelPlannerConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlexModelPlanner)
