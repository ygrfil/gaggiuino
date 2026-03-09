<div align="center">
<b>BLACKPILL PCB FORK - MODERNIZED FOR A NO-SCALES MACHINE</b>

[![Gaggiuino](/images/GAGGIUINO_LOGO_transp.png)](https://gaggiuino.github.io/#/)
  
[![Compile Sketch](https://github.com/Zer0-bit/gaggiuino/actions/workflows/compile-sketch.yml/badge.svg)](https://github.com/Zer0-bit/gaggiuino/actions/workflows/compile-sketch.yml)
[![Discord Chat](https://img.shields.io/discord/890339612441063494)](https://discord.gg/eJTDJA3xfh "Join Discord Help Chat")
</div>

## Fork Summary
This fork starts from the original `release/stm32-blackpill` Gaggiuino branch and is narrowed to one hardware target only:

- Blackpill PCB build
- `SINGLE_BOARD`
- `MAX31855` thermocouple interface
- 50 Hz baseline
- no scales hardware

It is intentionally a clean-break firmware fork for this machine, not a compatibility-preserving upstream patch set.

## What Changed
- Removed LEGO build targets and legacy non-Blackpill hardware paths.
- Removed scales from the runtime path and from the main firmware build.
- Replaced the old brew/steam heater heuristics with explicit temperature PID control.
- Replaced the pressure-target pump heuristic with a PI-based pressure controller.
- Kept flow profiling support, but tuned it for a no-scales machine using pumped-water estimation.
- Added 25-minute inactivity heater standby:
  - heater off
  - pump off
  - steam relays off
  - MCU and display stay alive for wake/resume
- Updated default brew profiles with `Rao Best Practice` first.
- Bumped EEPROM version, so old stored settings are reset to the new defaults.
- Updated the Blackpill dependency baseline in PlatformIO.

## Default Profiles
The shipped profile order is:

1. `Rao Best Practice`
2. `Classic 9 Bar`
3. `Light Roast Flow`
4. `Blooming Espresso`
5. `Dark Roast Comfort`

## Build
Primary firmware target:

```bash
pio run -e all-pcb-stlink
```

Native tests:

```bash
pio test -e test
```

## USB DFU Upload
With the Blackpill in DFU mode:

```bash
dfu-util -a 0 -s 0x08000000:leave -D .pio/build/all-pcb-stlink/firmware.bin
```

Or build and upload:

```bash
pio run -e all-pcb-stlink && dfu-util -a 0 -s 0x08000000:leave -D .pio/build/all-pcb-stlink/firmware.bin
```

## Notes
- This fork assumes no hardware scales are connected.
- Shot stopping still uses the existing shot target settings, but the runtime estimate is based on pumped-water flow rather than real scale data.
- If you flash this over an older install, expect settings to reset because the EEPROM version was intentionally bumped.
