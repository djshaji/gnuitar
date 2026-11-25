cmake_minimum_required(VERSION 3.19)

execute_process(COMMAND "${LV2LS}" OUTPUT_VARIABLE available OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" discovered "${available}")
list(LENGTH discovered count)
if(NOT count EQUAL 16)
    message(FATAL_ERROR "Expected exactly sixteen plugins, found: ${discovered}")
endif()

function(check_ports effect layout)
    set(uri "https://github.com/djshaji/gnuitar#${effect}-${layout}")
    if(NOT uri IN_LIST discovered)
        message(FATAL_ERROR "Plugin discovery failed for ${uri}")
    endif()
    execute_process(COMMAND "${LV2INFO}" "${uri}" OUTPUT_VARIABLE info
        COMMAND_ERROR_IS_FATAL ANY)
    string(REGEX MATCHALL "Symbol:[ \t]+[a-z_][a-z_0-9]+" symbol_lines "${info}")
    string(REGEX REPLACE "Symbol:[ \t]+" "" symbols "${symbol_lines}")
    if(NOT "${symbols}" STREQUAL "${ARGN}")
        message(FATAL_ERROR "${uri}: expected ports ${ARGN}, got ${symbols}")
    endif()
endfunction()

check_ports(digital-amp mono in out gain_db)
check_ports(digital-amp stereo in_l in_r out_l out_r gain_db)
check_ports(tremolo mono in out period_ms depth_pct)
check_ports(tremolo stereo in_l in_r out_l out_r period_ms depth_pct)
check_ports(tremolo-bar mono in out period_ms depth_hz base_hz)
check_ports(tremolo-bar stereo in_l in_r out_l out_r period_ms depth_hz base_hz)
check_ports(noise-gate mono in out threshold_pct hysteresis_pct attack_ms hold_ms release_ms)
check_ports(noise-gate stereo in_l in_r out_l out_r threshold_pct hysteresis_pct attack_ms hold_ms release_ms)
check_ports(sustain mono in out sustain_pct noise_pct gate_pct)
check_ports(sustain stereo in_l in_r out_l out_r sustain_pct noise_pct gate_pct)
check_ports(distort mono in out drive_pct saturation_pct level_pct lowpass_hz)
check_ports(distort stereo in_l in_r out_l out_r drive_pct saturation_pct level_pct lowpass_hz)
check_ports(overdrive mono in out drive_pct level_pct treble_db)
check_ports(overdrive stereo in_l in_r out_l out_r drive_pct level_pct treble_db)

set(eq_controls
    band_40_db band_100_db band_200_db band_320_db band_640_db band_1000_db
    band_1600_db band_2200_db band_3000_db band_4000_db band_6000_db
    band_8000_db band_12000_db band_16000_db gain_db)
check_ports(eq-bank mono in out ${eq_controls})
check_ports(eq-bank stereo in_l in_r out_l out_r ${eq_controls})
