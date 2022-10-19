# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDART 20)
set(CMAKE_C_STANDART 17)

set(COMPONENT_ADD_INCLUDEDIRS
    main
    )

set(EXTRA_COMPONENT_DIRS 
    ../libs
    )

if (IDF_VERSION_MAJOR GREATER_EQUAL 5)
    set(COMPONENT_REQUIRES esp_timer driver wpa_supplicant mbedtls)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_fpv_rx)
