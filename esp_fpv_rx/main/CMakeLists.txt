set(fpv_srcs
    "fpv_main.c"
    "display_osd.cpp"
    "image_decoder.c"    
    "memory_model/memory_model.c"
    "tjpg_decoder/tjpgd.c"
    )
    
set(wireless_srcs
    "wireless/wireless_encryption.c"
    "wireless/wireless_main.c"
    "wireless/wireless_scanner.c"
    )

idf_component_register(SRCS ${fpv_srcs} ${wireless_srcs}
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
