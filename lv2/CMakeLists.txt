cmake_minimum_required(VERSION 3.19)
project(GNUitarLV2 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)

find_path(LV2_INCLUDE_DIR NAMES lv2/core/lv2.h REQUIRED)

set(GNUITAR_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bundles/gnuitar.lv2")
set(GNUITAR_LV2_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/lv2"
    CACHE STRING "Directory where LV2 bundles are installed")

add_library(gnuitar_lv2 MODULE pilot.c ../src/biquad.c ../src/rcfilter.c)
target_compile_features(gnuitar_lv2 PRIVATE c_std_99)
target_compile_definitions(gnuitar_lv2 PRIVATE FLOAT_DSP _DEFAULT_SOURCE)
target_include_directories(gnuitar_lv2 PRIVATE "${LV2_INCLUDE_DIR}" ../src)
target_link_libraries(gnuitar_lv2 PRIVATE m)
set_target_properties(gnuitar_lv2 PROPERTIES
    C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${GNUITAR_BUNDLE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${GNUITAR_BUNDLE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${GNUITAR_BUNDLE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${GNUITAR_BUNDLE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${GNUITAR_BUNDLE_DIR}"
    OUTPUT_NAME gnuitar
    PREFIX "")

foreach(file IN ITEMS manifest.ttl gnuitar.ttl)
    configure_file("${file}" "${GNUITAR_BUNDLE_DIR}/${file}" COPYONLY)
endforeach()
configure_file(../COPYING "${GNUITAR_BUNDLE_DIR}/COPYING" COPYONLY)

install(TARGETS gnuitar_lv2 LIBRARY DESTINATION "${GNUITAR_LV2_INSTALL_DIR}/gnuitar.lv2")
install(FILES manifest.ttl gnuitar.ttl ../COPYING
    DESTINATION "${GNUITAR_LV2_INSTALL_DIR}/gnuitar.lv2")

if(BUILD_TESTING)
    add_executable(test_pilot tests/test_pilot.c ../src/biquad.c)
    target_compile_features(test_pilot PRIVATE c_std_99)
    target_compile_definitions(test_pilot PRIVATE FLOAT_DSP _DEFAULT_SOURCE)
    target_include_directories(test_pilot PRIVATE "${LV2_INCLUDE_DIR}" ../src)
    target_link_libraries(test_pilot PRIVATE ${CMAKE_DL_LIBS} m)
    add_test(NAME pilot_dsp COMMAND test_pilot "$<TARGET_FILE:gnuitar_lv2>")

    find_program(LV2LS lv2ls)
    find_program(LV2INFO lv2info)
    if(LV2LS AND LV2INFO)
        add_test(NAME pilot_discovery
            COMMAND "${CMAKE_COMMAND}"
                "-DLV2LS=${LV2LS}" "-DLV2INFO=${LV2INFO}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/discovery.cmake")
        set_tests_properties(pilot_discovery PROPERTIES
            ENVIRONMENT "LV2_PATH=${CMAKE_CURRENT_BINARY_DIR}/bundles")
    endif()
endif()
