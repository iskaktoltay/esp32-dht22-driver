idf_component_register(SRCS "dht22_self.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_gpio esp_timer)
