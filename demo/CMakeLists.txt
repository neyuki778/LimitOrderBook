add_library(demo_lib demo.cpp)
target_include_directories(demo_lib PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(demo_lib ${CMAKE_PROJECT_NAME}_lib)