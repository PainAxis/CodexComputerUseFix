# Community hardening: v0.1.0-community.1

Base: f5ad8d0af21db17f6ab50255f37926ee871ade4a (upstream v0.1.0).
Upstream documentation was synchronized with commit 1a56b55. This is a community-modified build, not an official OpenAI or upstream release. The DLL is unsigned.

## Changes

- Stage the installer DLL and recovery record in the target directory, then publish the recovery record before the DLL using no-overwrite renames. Failed record writes leave no loadable DLL or temporary files. Repeated installation validates the record contents.
- Restore memory-page protection with at most three attempts. If all attempts fail, fail fast in the capture process instead of continuing with a writable IAT/vtable. Tests inject a non-terminating recorder for this exceptional path; the production process-termination path has not been deliberately triggered.
- Publish pending subscription records before calling native COM registration. A concurrent close cancels pending records; the registration thread cleans up late tokens. Reference accounting avoids releasing a record while registration is in flight. A closing-pool marker prevents registration during the native close call.
- Query the optional Session3 interface explicitly in the baseline probe, avoiding the SDK projection's null optional-interface path on Windows 10.
- Add regression tests and wire them into the build/package/CI paths. build.ps1 -UseEnvironment supports an already-configured MSVC/SDK environment; normal Visual Studio discovery remains available.

## Validation on Windows 10 build 19044.4529

- Compiled using MSVC toolset 14.44.35207 and Windows SDK 10.0.22621.0.
- Four native test executables pass, including 100 deterministic registration/close races.
- Installer baseline and injected record-write failure tests pass using inert EXE fixtures.
- All eight validate.ps1 scenarios pass (native baseline, unrelated process, and six patched capture scenarios).
- A real 520 x 300 test-window capture was saved and visually checked; SoftwareBitmap conversion succeeds, with zero live handlers and zero hook/dispatch errors after cleanup.

The actual Codex helper and an RDP window have not been tested with this DLL. No DLL has been installed into the actual runtime. Runtime updates may change compatibility. Installation must target the actual helper directory, never the Windows system directory. Use the included installer/uninstaller and restart the helper to load or unload it.
