cmake_minimum_required(VERSION 3.28)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)

# Enable aggressive optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
set(CMAKE_BUILD_TYPE Release)
add_executable(${CMAKE_PROJECT_NAME}_run main.cpp)

target_include_directories(${CMAKE_PROJECT_NAME}_run PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(${CMAKE_PROJECT_NAME}_run PRIVATE src)

include_directories(src)
add_subdirectory(src)
# add_subdirectory(tests)
add_subdirectory(demo)

target_link_libraries(${CMAKE_PROJECT_NAME}_run ${CMAKE_PROJECT_NAME}_lib demo_lib)

# cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
# cmake --build . --parallel