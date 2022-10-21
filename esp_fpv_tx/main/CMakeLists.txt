set(fpv_srcs
    "fpv_main.c"
    "camera.c"
    "debug_assist.c"
    )

set(wireless_srcs
    "wireless/wireless_encryption.c"
    "wireless/wireless_main.c"
    )

idf_component_register(SRCS ${fpv_srcs} ${wireless_srcs}
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")