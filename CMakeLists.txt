# NotEnoughIDsPE for the iOS MCPE (v0.16.2) Emulator.
# Builds an arm64 dylib (NotEnoughIDsPE.dylib) and copies it to the emulator's mods folder.
cmake_minimum_required(VERSION 3.20)
project(NotEnoughIDsPE C CXX)

set(CMAKE_CXX_STANDARD 20)

add_library(NotEnoughIDsPE SHARED
    src/NotEnoughIDsPE.cpp   # generated from the tweak's Tweak.xm by tools/port_tweak.py
    src/neid_port.cpp
)
target_include_directories(NotEnoughIDsPE PRIVATE include src)
set_target_properties(NotEnoughIDsPE PROPERTIES PREFIX "" SUFFIX ".dylib")
# as the tweak (Makefile: -fno-rtti -fno-exceptions); the tweak's code has unused
# variables and functions that only some builds use
target_compile_options(NotEnoughIDsPE PRIVATE -fno-rtti -fno-exceptions -Wno-unused-function -Wno-unused-variable)

# Emulator data folder (the mod goes to <data>/mods). Change it if you start the
# emulator with --data.
set(MCPE_DATA_DIR "$ENV{LOCALAPPDATA}/iOS-MCPE-0.16.2" CACHE PATH "iOS MCPE emulator data folder")
add_custom_command(TARGET NotEnoughIDsPE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MCPE_DATA_DIR}/mods"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:NotEnoughIDsPE>" "${MCPE_DATA_DIR}/mods/"
    COMMENT "Installing the mod into ${MCPE_DATA_DIR}/mods")
