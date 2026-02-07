cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(edgeai_sand_demo LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/demo_apps/edgeai_sand_demo)

include(${SdkRootDirPath}/CMakeLists.txt)

get_filename_component(EDGEAI_ROOT "${SdkRootDirPath}/../../../../projects/nxp/frdm-mcxn947/EdgeAI_sand_demo" ABSOLUTE)

mcux_add_source(
    BASE_PATH ${EDGEAI_ROOT}
    SOURCES src/edgeai_sand_demo.c
            src/fxls8974cf.c
            src/sim.c
)

mcux_add_include(
    BASE_PATH ${EDGEAI_ROOT}
    INCLUDES src
)

include(${SdkRootDirPath}/${board_root}/${board}/demo_apps/edgeai_sand_demo/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

