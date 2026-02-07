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

## 4) Flash

Flashing uses NXP LinkServer (x86_64) via `west flash -r linkserver`.

After installing LinkServer and ensuring `LinkServer` is on `PATH`:

```bash
./tools/flash_frdmmcxn947.sh
```

