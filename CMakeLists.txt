# CMakeLists.txt for Arducam module (C:\ncs\v3.2.3\modules\atlas_ezo_ph\)

if(CONFIG_PH_SENSOR)


# Ensure include directory is always added
target_include_directories(zephyr_interface BEFORE INTERFACE
    ${ZEPHYR_CURRENT_MODULE_DIR}/include
)

# Only compile if the Kconfig symbol is enabled
zephyr_library_sources_ifdef(
    CONFIG_PH_SENSOR
    drivers/sensor/ph_sensor/ph_sensor.c
)

# Add subdirectory if needed
if(EXISTS ${ZEPHYR_CURRENT_MODULE_DIR}/drivers/sensor/ph_sensor/CMakeLists.txt)
    add_subdirectory(drivers/sensor/ph_sensor)
endif()
endif()
