# ServiceManager

Standalone FreeMiNT application, maintained for the Foenix A2560Me.

## Build

Requires Python 3, `m68k-atari-mint-gcc`, a built MiNTlib tree with
`include/` and `build/m68020/`, and GEMlib with `.lib68020-60/`.
These dependencies are supplied explicitly; no system-workspace checkout is required.

```sh
python3 build.py --mintlib /path/to/mintlib --gemlib /path/to/gemlib
```

Outputs are written to `build/`. Use `--output` to change the location and
`--cc` to select a compiler or compiler wrapper. Builds target 68060 without
FPU instructions. 

## Tests

Host regression tests need Python 3 and a host C compiler; ServiceManager's
account transaction test also requires libcrypt development files.

```sh
for test in tests/*.py; do python3 "$test" || exit; done
```

## Installation and integration

See [USAGE.md](USAGE.md) for runtime instructions and known limitations.
The [system workspace](https://github.com/FoenixLabs/freemint-agent-workspace)
handles drive installation, startup scripts, hardware dependencies, and desktop shortcuts.
This repository owns application sources, builds, and application unit tests.

## License and provenance

GPL-2.0-or-later; see COPYING.

## Original project notes

ServiceManager: modeless GEM controls for A2560Me networking and sharing.
GPL-2.0-or-later. app.c is the UI; service.c validates settings and safely
updates the dedicated FTP account and service configuration; control.c provides
non-interactive validation and hostname application. No password argv API.
Build and stage through the system integration workspace: scripts/services.py.
Application tests are in tests/ here; startup integration tests remain in the system repo.
Runtime instructions: docs/SERVICES.md in the system repo.
