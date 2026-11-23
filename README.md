# CtrlHz180

PSP controller plugin (ARK/PRO).

- Default sampling cycle: ~180 Hz (5555 µs). Explicit game cycles are respected.
- Buttons: immediate press, **2-sample release filter** (single-item Read/Peek only).
- Analog stick (Lx/Ly): **unchanged**. No filtering or dead zone.
- ReadBuffer waits, timestamps, multi-item reads and latch APIs: unchanged.

## Build

```sh
cd source
make clean && make
```

Output: `source/ctrlhz180.prx` (PSPDEV + CFW systemctrl libraries required).

## Test

```sh
bash tests/run_tests.sh
```

Copy the PRX to `ms0:/seplugins/` and enable it for GAME. Do not run another controller-sampling PRX at the same time.

Note: a 1-sample OFF between two presses can be hidden. Hardware testing is required.
