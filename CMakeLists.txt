# IDF6: esp_private/gpio.h lives in esp_driver_gpio and the I2S LL in esp_hal_i2s, no longer reached through driver.
set(requires driver)
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND requires esp_driver_gpio esp_hal_i2s)
endif()
idf_component_register(
    SRCS "src/I2SClocklessLedDriver.cpp"
    INCLUDE_DIRS "src"
    REQUIRES ${requires}
)
