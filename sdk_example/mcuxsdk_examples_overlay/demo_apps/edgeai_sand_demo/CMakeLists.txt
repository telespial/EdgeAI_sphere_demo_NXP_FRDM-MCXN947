cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(edgeai_sand_demo LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/demo_apps/edgeai_sand_demo)

include(${SdkRootDirPath}/CMakeLists.txt)

if(DEFINED ENV{EDGEAI_SAND_DEMO_ROOT})
    set(EDGEAI_ROOT "$ENV{EDGEAI_SAND_DEMO_ROOT}")
else()
    # Default layout: this repo contains ./mcuxsdk_ws and the workspace contains mcuxsdk/.
    # SdkRootDirPath points at <repo>/mcuxsdk_ws/mcuxsdk, so ../.. is the repo root.
    get_filename_component(EDGEAI_ROOT "${SdkRootDirPath}/../.." ABSOLUTE)
endif()

mcux_add_source(
    BASE_PATH ${EDGEAI_ROOT}
    SOURCES src/edgeai_sand_demo.c
            src/fxls8974cf.c
            src/par_lcd_s035.c
            src/postfx.c
            src/npu_postfx_stub.c
            src/sim.c
)

mcux_add_include(
    BASE_PATH ${EDGEAI_ROOT}
    INCLUDES src
)

include(${SdkRootDirPath}/${board_root}/${board}/demo_apps/edgeai_sand_demo/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
