# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Gaggiuino is a coffee machine modification project that transforms a Gaggia Classic into a smart espresso machine with pressure profiling, temperature control, and predictive weight algorithms. The project consists of three main components:

1. **STM32 Firmware** (`src/`): Main control firmware written in C++ using Arduino framework
2. **ESP32 Webserver** (`webserver/`): WiFi-enabled web interface and API server
3. **React Web Interface** (`webserver/web-interface/`): Frontend dashboard for machine control and monitoring

## Architecture

### STM32 Firmware (Primary Controller)
- **Main entry point**: `src/gaggiuino.ino`
- **Core systems**: Located in `lib/Common/` with shared state management
- **Peripherals**: Hardware control modules in `src/peripherals/`
- **Functional modules**: Coffee brewing logic in `src/functional/`
- **EEPROM data**: Configuration and profile storage in `src/eeprom_data/`

### ESP32 Webserver
- **Main**: `webserver/src/main.cpp`
- **STM32 Communication**: `webserver/src/stm_comms/`
- **Web API**: REST endpoints in `webserver/src/server/api/`
- **WebSocket**: Real-time data streaming in `webserver/src/server/websocket/`

### React Web Interface
- **Entry point**: `webserver/web-interface/src/index.tsx`
- **Components**: Reusable UI components in `webserver/web-interface/src/components/`
- **Pages**: Main application views in `webserver/web-interface/src/pages/`
- **Charts**: Data visualization components using Chart.js

## Common Development Commands

### STM32 Firmware
```bash
# Build firmware for all-PCB configuration
pio run -e all-pcb-stlink

# Build firmware for LEGO valve configuration
pio run -e lego-stlink

# Upload firmware via DFU
pio run -e all-pcb-stlink -t upload

# Run tests
pio test -e test

# Check code quality
pio check
```

### ESP32 Webserver
```bash
# Build ESP32 firmware
cd webserver
pio run

# Upload to ESP32
pio run -t upload

# Monitor serial output
pio device monitor
```

### React Web Interface
```bash
cd webserver/web-interface

# Install dependencies
npm install

# Start development server
npm start

# Build for production
npm run build

# Run tests
npm test

# Lint code
npm run lint
```

## Key Configuration Files

- `platformio.ini`: PlatformIO build configuration with multiple environments
- `extra_defines.ini`: User-specific build flags (create this file for custom settings)
- `webserver/web-interface/package.json`: Node.js dependencies and scripts

## Hardware Configurations

The firmware supports multiple hardware configurations:
- `all-pcb-stlink`: Single PCB design with integrated components
- `lego-stlink`: LEGO valve relay configuration
- `scales-calibration-stlink`: Special build for scale calibration

## Flashing Instructions

1. Put STM32 into DFU mode (hold NRST+BOOT0, release NRST, wait, release BOOT0)
2. Flash using: `dfu-util -d 0x0483:0xDF11 -a 0 -s 0x08000000:leave -D firmware.bin`

## Testing

- STM32 tests: Located in `test/tests/` using PlatformIO test framework
- Web interface tests: Jest-based tests in React components
- Mock hardware: Test doubles in `test-lib/mocks/` for hardware abstraction

## Key State Management

- `SystemState`: Global system state in `lib/Common/system_state.h`
- `SensorState`: Hardware sensor readings in `lib/Common/sensors_state.h`
- `eepromValues_t`: Persistent configuration storage
- `Profile`: Coffee brewing profiles and phases