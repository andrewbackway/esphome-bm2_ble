# Copilot & Developer Instructions

This file contains instructions a developer (or GitHub Copilot) should follow when working on the `esphome-bm2_ble` project. These instructions focus on the ESPhome version, board, framework, and build/flash steps for the Lolin S3 Pro board.

## Overview ✅
- ESPhome minimum version: **2025.11.0** (this is required for the YAML examples in this repo)
- Target board: **Lolin S3 Pro** (`board: lolin_s3_pro`) — an ESP32-S3 board
- Framework: **ESP-IDF** (specified in the YAML as `framework: esp-idf`)

## Local Setup 💡
1. Install Python (3.10+ recommended). On Windows you can use either the Python.org installer or `pyenv`/`Chocolatey`.
2. Install ESPhome CLI pinned to the version used by this repo:

```powershell
python -m pip install --upgrade pip
pip install esphome==2025.11.0
```

3. For frameworks using ESP-IDF, ESPhome will handle the toolchain automatically in most cases. If you need custom ESP-IDF versions or you have local custom patching requirements, follow the official ESP-IDF install instructions: https://docs.espressif.com/projects/esp-idf/en/latest/esp-idf/get-started/

4. Connect your Lolin S3 Pro board to your machine using a USB cable.

## Building and Flashing 🔧
- To validate, use the test YAML (no OTA) for local builds and testing:

```powershell
esphome run bm2_example_test.yaml --device COM3
```

- To build and upload the example config (replace `COM3` with your device port):

```powershell
esphome run bm2_example.yaml --device COM3
```

Notes:
- If you rely on `external_components` to import from GitHub, ensure you have internet access when building. For local testing, `bm2_example_test.yaml` has the external_components section disabled.
- For ESP-IDF frameworks, if the esphome build suggests missing tools, follow the prompts to install toolchains.

## Additional Tips ⚠️
- If using Windows and esphome isn't recognizing a COM port, ensure correct USB-UART drivers are installed (e.g., CP210x, CH340). Additionally, try using `powershell` running as Administrator.
- You can change logs to INFO level in YAML for quieter logs in normal use and DEBUG level for troubleshooting.
- If your board is not `lolin_s3_pro` then update the `board:` entry in YAML accordingly.

## YAML Specifics to Review 📝
- `min_version: 2025.11.0` ensures compatibility with ESP-IDF and recent ESPhome features
- `framework: esp-idf` is required for using the ESP32-S3 features available in the Lolin S3 Pro board
- Keep `external_components` configured for development to point at the local path or GitHub. The default example points at GitHub.

## Questions or Troubleshooting
If something fails during the build or the board does not boot, collect the build logs and console output. File issues or PRs with reproduction steps and tag them for review.

---
Updated for ESPhome 2025.11 and Lolin S3 Pro (ESP-IDF), (c) repository maintainers.
