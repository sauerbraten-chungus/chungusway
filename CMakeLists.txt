cmake_minimum_required(VERSION 3.10)

project(chungustrator-enet)
set(CMAKE_CXX_STANDARD 20)
set(CMAKAE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(unofficial-enet CONFIG REQUIRED)
find_package(grpc CONFIG REQUIRED)

add_executable(chungustrator-enet src/main.cpp proto/chungustrator_enet_streaming.grpc.pb.cc proto/chungustrator_enet_streaming.pb.cc src/enet_client.cpp src/chungus_service.cpp)

target_include_directories(chungustrator-enet PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(chungustrator-enet PRIVATE unofficial::enet::enet fmt::fmt gRPC::grpc++)
