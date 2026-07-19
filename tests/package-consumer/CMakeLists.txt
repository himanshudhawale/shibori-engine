cmake_minimum_required(VERSION 3.25)

project(ShiboriEnginePackageConsumer LANGUAGES CXX)

find_package(ShiboriEngine 0.1 REQUIRED)

add_executable(shibori_engine_package_consumer main.cpp)
target_link_libraries(shibori_engine_package_consumer PRIVATE Shibori::engine)
