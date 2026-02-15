# Build / Flash (FRDM-MCXN947)

This repo is self-contained. It does not require any external build system repo.

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

## 4) Flash (Default: Known-Good Failsafe)

Default flashing uses the pinned failsafe artifact in `docs/failsafe.md`.
This is the recommended path for fresh pulls and recovery.

Install LinkServer (user-local, no sudo):

```bash
ACCEPT_NXP_LINKSERVER_LICENSE=1 ./tools/install_linkserver_user.sh
```

If flashing fails with `No probes detected`, install udev rules (requires sudo), then unplug/replug the board:

```bash
./tools/install_mculink_udev_rules.sh
```

Then flash default (recommended):

```bash
./tools/flash_default.sh
```

## 5) Flash Latest Local Source Build (Advanced)
Use this only when intentionally testing the current local source build output.

```bash
./tools/flash_frdmmcxn947.sh
```

## Failsafe Flash (Explicit Confirmation Mode)
The active failsafe artifact is recorded in `docs/failsafe.md`. This explicit mode is still available:

```bash
FAILSAFE_CONFIRM="$(sed -n '1p' docs/failsafe.md)" ./tools/flash_failsafe.sh
```

## Serial (Optional)
The MCU-Link VCOM interface typically enumerates as `/dev/ttyACM0`:
```bash
timeout 10 cat /dev/ttyACM0
```
