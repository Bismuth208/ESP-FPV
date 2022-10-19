# CMakeLists for ESP-IDF

set(COMPONENT_ADD_INCLUDEDIRS
    src
    )
file(GLOB SRCS
     src/*.c
     )

set(COMPONENT_SRCS ${SRCS})

if (IDF_VERSION_MAJOR GREATER_EQUAL 5)
    set(COMPONENT_REQUIRES esp_timer)
endif()

register_component()
