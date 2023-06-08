# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.0)

# Set the project name
project (redis)

# Add an executable
add_executable(server server.cpp)
add_executable(client client.cpp)
