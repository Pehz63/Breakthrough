# run-tests

Build and run the Breakthrough test suite.

## How to invoke

This skill is triggered when the user asks to run tests, check tests pass, verify
the test suite, or similar.

## Steps

1. Run the test suite using the project's wrapper script. Use the Bash tool:

```
cd "c:/Users/Ricar/Downloads/Zeph/Board Games/Breakthrough" && powershell.exe -NonInteractive -File "tools/run_tests.ps1" -Build 2>&1
```

This script:
- Calls `build_tests.bat`, which uses `vswhere.exe` to locate Visual Studio and
  loads `vcvars64.bat` before running `cl`. The VS path is detected automatically,
  so do not hardcode it.
- Compiles all test sources into `tests.exe`.
- Runs `tests.exe` and streams the Catch2 output.

2. Report the result:
- Quote the final Catch2 summary line (e.g. "All 81 assertions passed.").
- If the build or tests fail, quote the first error line so the developer can act on it.

## Notes

- Do NOT use `cl` directly from PowerShell or Bash without first loading
  `vcvars64.bat`. `cl` is not on the default PATH and the call will silently fail.
  The `build_tests.bat` / `run_tests.ps1` pair handles this correctly.
- Do NOT use PowerShell's pipe (`|`) to feed answers to `breakthrough.exe`; the
  encoding corrupts stdin. Use the Bash tool with `printf '...' | ./breakthrough.exe`
  if you need to drive the console app non-interactively.
- Always run from the project root so relative paths (`boards\`, `build\`) resolve.
