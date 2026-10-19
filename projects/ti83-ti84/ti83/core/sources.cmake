get_filename_component(TI83_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(TI83_CORE_SOURCES ${TI83_ROOT}/core/adapter.c ${TI83_ROOT}/core/allocator.c ${TI83_ROOT}/core/program.c)
list(APPEND TI83_CORE_SOURCES ${TI83_ROOT}/core/fast_memory.c)
list(APPEND TI83_CORE_SOURCES ${TI83_ROOT}/core/cpu_profile.c)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(TI83_CPU_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/ti83_cpu/z80_split.c")
set(TI83_KEYPAD_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/ti83_cpu/keypad_fast.c")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${TI83_ROOT}/scripts/split-cpu.py"
    "${TI83_ROOT}/upstream/emu" "${CMAKE_CURRENT_BINARY_DIR}/ti83_cpu"
    RESULT_VARIABLE TI83_SPLIT_RESULT)
if(NOT TI83_SPLIT_RESULT EQUAL 0)
    message(FATAL_ERROR "TI83 CPU split generation failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${TI83_ROOT}/scripts/split-cpu.py" "${TI83_ROOT}/upstream/emu/z80.c"
    "${TI83_ROOT}/upstream/emu/z80main.h" "${TI83_ROOT}/upstream/emu/z80cmds.h"
    "${TI83_ROOT}/upstream/emu/z80cb.h" "${TI83_ROOT}/upstream/emu/z80ed.h"
    "${TI83_ROOT}/upstream/emu/z80ddfd.h" "${TI83_ROOT}/upstream/emu/keypad.c")
list(APPEND TI83_CORE_SOURCES "${TI83_CPU_SOURCE}")
list(APPEND TI83_CORE_SOURCES "${TI83_KEYPAD_SOURCE}")
foreach(unit calcs flash link lcd cert md5 timers)
    list(APPEND TI83_CORE_SOURCES ${TI83_ROOT}/upstream/emu/${unit}.c)
endforeach()
foreach(model x1 x2 x3 x4 x5 x6 x7 xn xp xs xz)
    foreach(unit init io memory subcore)
        list(APPEND TI83_CORE_SOURCES ${TI83_ROOT}/upstream/emu/${model}/${model}_${unit}.c)
    endforeach()
endforeach()
