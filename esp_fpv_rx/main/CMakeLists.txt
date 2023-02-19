set(FPV_SRCS
    "fpv_main.c"
    "button_poller.c"
    "display_osd.cpp"
    "image_decoder.c"
    )

set(WIRELESS_MODULE_SRCS
    "wireless/wireless_encryption.c"
    "wireless/wireless_main.c"
    "wireless/wireless_scanner.c"
    )

set(MISC_SRCS
    "memory_model/memory_model.c"
    "tjpg_decoder/tjpgd.c"
    )

idf_component_register(SRCS 
    ${FPV_SRCS}
    ${WIRELESS_MODULE_SRCS}
    ${MISC_SRCS}
    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
