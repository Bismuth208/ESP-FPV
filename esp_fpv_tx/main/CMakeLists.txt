set(FPV_SRCS
    "fpv_main.c"
    "camera.c"
    )

set(WIRELESS_MODULE_SRCS
    "wireless/wireless_encryption.c"
    "wireless/wireless_main.c"
    )

set(MISC_SRCS
    "debug_assist.c"
    )

idf_component_register(SRCS
    ${FPV_SRCS}
    ${WIRELESS_MODULE_SRCS}
    ${MISC_SRCS}
    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")