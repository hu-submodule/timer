cmake_minimum_required(VERSION 3.10)

# 定义静态库
add_library(hs_timer STATIC hs_timer.c)

# 指定需要链接的库
target_link_libraries(hs_timer PUBLIC rt)

# 添加头文件搜索路径
target_include_directories(hs_timer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
