idf_component_register(SRCS "esp_ota_server.c" "file_server.c" "mount.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "favicon.ico" "upload_script.html")
