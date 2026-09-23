cmake_minimum_required(VERSION 3.20)

project(recommendation-engine
    VERSION 0.3.0
    DESCRIPTION "gRPC recommendation engine for the Marketly platform"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Find dependencies ----
# Protobuf ships a CMake config on Debian, so find_package works.
find_package(Protobuf REQUIRED)

# gRPC on Debian (libgrpc++-dev) does NOT ship a CMake config — it
# ships pkg-config files instead. Use pkg-config to locate the libs and
# find_program to locate the codegen plugin.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found (install protobuf-compiler-grpc)")
endif()
if(NOT Protobuf_PROTOC_EXECUTABLE)
    find_program(Protobuf_PROTOC_EXECUTABLE protoc)
endif()

# ---- Protobuf / gRPC code generation ----
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/protos/recommendation.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS
    ${GENERATED_DIR}/recommendation.pb.cc
    ${GENERATED_DIR}/recommendation.grpc.pb.cc)
set(PROTO_HDRS
    ${GENERATED_DIR}/recommendation.pb.h
    ${GENERATED_DIR}/recommendation.grpc.pb.h)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/src/protos
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf + gRPC sources for recommendation.proto"
    VERBATIM)

# Library for the generated proto+grpc code. The generated headers are
# flat (recommendation.pb.h), so consumers include them without a
# "protos/" prefix; the include dir below makes that resolve.
add_library(recommendation_proto STATIC ${PROTO_SRCS})
target_include_directories(recommendation_proto PUBLIC ${GENERATED_DIR})
target_link_libraries(recommendation_proto
    PUBLIC ${GRPC_LIBRARIES} protobuf::libprotobuf)
target_include_directories(recommendation_proto
    PRIVATE ${GRPC_INCLUDE_DIRS})

# ---- Engine library (cache + engine + models) ----
add_library(recommendation_core STATIC
    src/models.cpp
    src/cache.cpp
    src/engine.cpp)
target_include_directories(recommendation_core PUBLIC src)
target_link_libraries(recommendation_core PUBLIC recommendation_proto)

# ---- gRPC server ----
add_executable(recommendation-engine
    src/main.cpp
    src/server.cpp)
target_link_libraries(recommendation-engine
    PRIVATE recommendation_core ${GRPC_LIBRARIES} protobuf::libprotobuf)

# ---- Tests ----
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_executable(test_engine tests/test_engine.cpp)
    target_link_libraries(test_engine PRIVATE recommendation_core)
    add_test(NAME test_engine COMMAND test_engine)
endif()

# ---- Install ----
install(TARGETS recommendation-engine RUNTIME DESTINATION bin)
