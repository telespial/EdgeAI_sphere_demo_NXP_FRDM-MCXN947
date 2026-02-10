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
            src/text5x7.c
            src/accel_proc.c
            src/sim_world.c
            src/render_world.c
            src/npu_api.c
            src/npu_backend_stub.c
            src/npu_backend_neutron.cpp
            src/sand_sim.c
            src/water_sim.c
            src/fxls8974cf.c
            src/par_lcd_s035.c
            src/sw_render.c
            src/npu/model.cpp
            src/npu/model_ops_npu.cpp
)

mcux_add_include(
    BASE_PATH ${EDGEAI_ROOT}
    INCLUDES src
)

# Default linker script stack is 0x800 (2 KB), which is enough for the render loop
# but not for TFLM/Neutron init/invoke when NPU inference is enabled (STKOF/HardFault).
if(DEFINED EDGEAI_ENABLE_NPU_INFERENCE AND EDGEAI_ENABLE_NPU_INFERENCE)
  mcux_add_linker_symbol(SYMBOLS "__stack_size__=0x2000")
endif()

include(${SdkRootDirPath}/${board_root}/${board}/demo_apps/edgeai_sand_demo/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
