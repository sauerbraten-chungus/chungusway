cmake_minimum_required(VERSION 3.10)

project(chungustrator-enet)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# nixpkgs' enet ships only a pkg-config file (libenet.pc), no CMake config package --
# unlike vcpkg's "unofficial-enet" wrapper. pkg-config works under both nix and vcpkg
# (vcpkg's toolchain file also populates PKG_CONFIG_PATH for most ports).
find_package(PkgConfig REQUIRED)
pkg_check_modules(ENET REQUIRED IMPORTED_TARGET libenet)

add_executable(chungustrator-enet
    src/main.cpp
    src/enet_client.cpp
    src/chungus_service.cpp
    src/match_helper.cpp
    proto/chungustrator_enet_streaming.grpc.pb.cc
    proto/chungustrator_enet_streaming.pb.cc
    proto/chungusway_chungusdb.grpc.pb.cc
    proto/chungusway_chungusdb.pb.cc
)

target_include_directories(chungustrator-enet PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(chungustrator-enet PRIVATE PkgConfig::ENET fmt::fmt gRPC::grpc++)
