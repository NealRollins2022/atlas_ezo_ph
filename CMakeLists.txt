# CMakeLists.txt for Arducam module (C:\ncs\v2.7.0\modules\arducam2\CMakeLists.txt)

if(CONFIG_PH_SENSOR)




  # Add include directory for drivers/video/arducam_mega.h
  target_include_directories(zephyr_interface BEFORE INTERFACE
    ${ZEPHYR_CURRENT_MODULE_DIR}/include
  )

  # Include driver source file (updated path)
  zephyr_library_sources(
    ${ZEPHYR_CURRENT_MODULE_DIR}/drivers/sensor/ph_sensor/ph_sensor.c
  )
endif()
