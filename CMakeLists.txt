# ESP32 Blackbox 项目根 CMake 构建配置文件
# 设置 CMake 最低版本要求，引入 ESP-IDF 项目构建系统

# 设置 CMake 最低版本为 3.16（ESP-IDF v4.4+ 要求）
cmake_minimum_required(VERSION 3.16)

# 引入 ESP-IDF 项目构建系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称为 "esp32-blackbox"
project(esp32-blackbox)
