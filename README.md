# SREES 2026 - dTwin WLS State Estimation Plugin

This project implements a natID/dTwin plugin for generating a WLS state estimation model from three-phase MATPOWER data.

The converter reads MATPOWER `.m` files, primarily `mpc.bus3p` data, and generates:

- a dTwin digital model (`.dmodl`) with WLS equations,
- a dTwin visual model (`.vmodl`) with plots for solver precision, estimated phase voltages, and estimated phase angles.

Gaussian noise is added to the measurement values in the generated model in order to simulate measurement uncertainty. The WLS problem is then solved inside dTwin.

## Project Structure

```text
docs/          LyX report, documentation, and OpenDraw figures
src/           C++ source code for the dTwin plugin and converter
presentation/  Final presentation and ReadMe.txt with presentation duration
```

Main implementation files:

- `src/ConverterCore.cpp` - MATPOWER parsing, natID dense/sparse matrix usage, full bus-phase WLS `.dmodl` generation, `.vmodl` generation
- `src/TSEPlugin.cpp` - dTwin plugin entry point
- `src/ViewConv.h` - plugin GUI for selecting files and running conversion

## Requirements

- natID SDK
- dTwin
- CMake
- C++17 compatible compiler

The project uses natID GUI classes and natID dense/sparse matrix classes. No external matrix library is used.

## Build on Windows

```powershell
cmake -S . -B build-vs18
cmake --build build-vs18 --config Debug
```

The main plugin target is:

```text
tse_complex_converter
```

After a successful Windows build, the plugin DLL is copied to the dTwin plugin folder:

```text
C:\Users\mirza\ba.natID\dTwin\plugins\tse_complex_converter.dll
```

## Cross-Platform Note

The CMake configuration uses a shared library target. On Windows this produces a `.dll`; on Linux or macOS it produces the corresponding platform-specific shared library (`.so` or `.dylib`), provided that the matching natID SDK is installed on that system.
