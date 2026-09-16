# Legacy environment-variable configuration (disabled)

This file preserves the original machine-specific configuration for reference.
VS Code does not load this Markdown file.

The active configuration now resolves CMake, Ninja, and GNU Arm Embedded tools
from `PATH`, so the same tracked files can be used on multiple computers.
Build output uses the repository's existing `build/` directory. Configure runs
with `--fresh` so caches containing another computer's absolute paths are reset.

## Original `.vscode/tasks.json` command

All `Configure`, `Build`, and `Clean` tasks originally used:

```json
"command": "${env:CMAKE_ROOT}/bin/cmake.exe"
```

The original file also contained damaged text encoding in several `statusbar.detail`
strings, which made the JSON invalid. The active `tasks.json` keeps the same tasks
but replaces those display-only strings with valid UTF-8 text.

## Original `.vscode/settings.json` tool paths

```json
"cmake.cmakePath": "${env:CMAKE_ROOT}/bin/cmake.exe",
"C_Cpp.default.compilerPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-gcc.exe",
"cortex-debug.armToolchainPath": "${env:ARM_GCC_ROOT}/bin",
"cortex-debug.gdbPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-gdb.exe",
"cortex-debug.objdumpPath": "${env:ARM_GCC_ROOT}/bin/arm-none-eabi-objdump.exe"
```

## Original `CMakePresets.json` additions

```json
"environment": {
    "PATH": "$env{ARM_GCC_ROOT}/bin;$penv{PATH}"
},
"cacheVariables": {
    "CMAKE_MAKE_PROGRAM": "$env{NINJA_ROOT}/ninja.exe",
    "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
}
```

If a computer cannot put the tools on `PATH`, these values can be restored and
the three variables `CMAKE_ROOT`, `ARM_GCC_ROOT`, and `NINJA_ROOT` can be set on
that computer without committing its absolute installation paths.
