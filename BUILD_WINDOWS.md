# Build on Windows

The project is expected to be placed in the user's HOME GitHub project folder:

```text
%USERPROFILE%\GitHub\SREES_2026_Vuk_TSE
```

For this machine that is:

```text
C:\Users\mirza\GitHub\SREES_2026_Vuk_TSE
```

Do not configure the same build folder from a Desktop or OneDrive copy of the source. If the source path changes, delete the whole build folder first, because CMake stores the original source path in `CMakeCache.txt`.

Expected natID paths:

```text
%USERPROFILE%\natID.SDK
%USERPROFILE%\ba.natID\dTwin\plugins
```

Configure and build:

```powershell
cmake -S "%USERPROFILE%\GitHub\SREES_2026_Vuk_TSE" -B "%USERPROFILE%\GitHub\SREES_2026_Vuk_TSE_build" -G "Visual Studio 18 2026" -A x64
cmake --build "%USERPROFILE%\GitHub\SREES_2026_Vuk_TSE_build" --config Release --target tse_complex_converter
```

The DLL target is:

```text
tse_complex_converter
```

After a successful Release build, CMake copies the plugin DLL to:

```text
%USERPROFILE%\ba.natID\dTwin\plugins\tse_complex_converter.dll
```

Restart dTwin after building if it was already open. Build Release, not Debug.

If CMake reports that the source path does not match the cached source path, delete the build folder and configure again.
