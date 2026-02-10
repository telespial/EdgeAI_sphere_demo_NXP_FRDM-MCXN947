# Build / Flash (FRDM-MCXN947)

This repo is self-contained. It does not require any external build system repo.

## Current Build Profile (2026-02-10)
Golden restore tag: `GOLDEN_2026-02-10_v27_npu_glint`

Last verified firmware artifact:
- `mcuxsdk_ws/build_v26_npu_glint/edgeai_sand_demo_cm33_core0.elf`

This build enables NPU inference (Neutron backend) and uses the NPU output as a 0..255 `glint` modulation input for the reflective ball shader.

## 1) Bootstrap Tooling (No sudo)

```bash
./tools/bootstrap_ubuntu_user.sh
```

This installs user-local:
- `pip`, `west`, `cmake`, `ninja`
- `git-lfs`
- `arm-none-eabi-gcc` (xPack)

## 2) Create/Update MCUX SDK Workspace

```bash
./tools/setup_mcuxsdk_ws.sh
```

This creates `./mcuxsdk_ws/`, runs `west init` + `west update`, then installs the
`edgeai_sand_demo` wrapper into `mcuxsdk/examples/`.

## 3) Build

```bash
./tools/build_frdmmcxn947.sh
```

Build output:
- `mcuxsdk_ws/build/edgeai_sand_demo_cm33_core0.bin`

Notes:
- Set `BUILD_DIR` to use a non-default build directory (for example, `BUILD_DIR=./mcuxsdk_ws/build_alt ./tools/build_frdmmcxn947.sh`).
- For a clean rebuild, use a fresh `BUILD_DIR` or remove the existing build directory contents.

### Build (Golden Profile: NPU Glint Enabled)
Build the same configuration as the last verified flash by passing CMake cache defines to `west build`:
```bash
cd mcuxsdk_ws
west build -d build_v26_npu_glint mcuxsdk/examples/demo_apps/edgeai_sand_demo \
  --toolchain armgcc \
  --config debug \
  -b frdmmcxn947 \
  -Dcore_id=cm33_core0 \
  -DEDGEAI_ENABLE_NPU_INFERENCE=1 \
  -DEDGEAI_NPU_BACKEND=1
```

## 4) Flash

Flashing uses NXP LinkServer (x86_64) via `west flash -r linkserver`.

Install LinkServer (user-local, no sudo):

```bash
ACCEPT_NXP_LINKSERVER_LICENSE=1 ./tools/install_linkserver_user.sh
```

If flashing fails with `No probes detected`, install udev rules (requires sudo), then unplug/replug the board:

```bash
./tools/install_mculink_udev_rules.sh
```

Then flash:

```bash
./tools/flash_frdmmcxn947.sh
```

### Flash (Golden Profile: NPU Glint Enabled)
Flash the `build_v26_npu_glint` artifact using the standard script by overriding `BUILD_DIR`:
```bash
BUILD_DIR="$PWD/mcuxsdk_ws/build_v26_npu_glint" ./tools/flash_frdmmcxn947.sh
```

Verify the device flash matches the ELF (optional):
```bash
LinkServer flash auto verify mcuxsdk_ws/build_v26_npu_glint/edgeai_sand_demo_cm33_core0.elf
```

## Failsafe Flash (Last Resort)
The active failsafe artifact is recorded in `docs/failsafe.md`.

Flashing requires explicit confirmation via the exact filename listed in line 1 of `docs/failsafe.md`:
```bash
FAILSAFE_CONFIRM="$(sed -n '1p' docs/failsafe.md)" ./tools/flash_failsafe.sh
```

## Serial (Optional)
The MCU-Link VCOM interface typically enumerates as `/dev/ttyACM0`:
```bash
timeout 10 cat /dev/ttyACM0
```
