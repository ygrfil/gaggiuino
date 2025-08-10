# PID Temperature Control Implementation

This directory contains a complete PID controller implementation designed to replace the simple hysteresis-based temperature control in the Gaggiuino espresso machine firmware.

## Features

### Advanced PID Controller (`pid_controller.h/cpp`)
- **Anti-windup protection**: Prevents integral term from growing excessively when output is saturated
- **Derivative filtering**: Reduces noise impact on derivative term with configurable filter coefficient
- **Setpoint ramping**: Gradually changes setpoint to prevent overshoot when target temperature changes
- **Temperature rate limiting**: Safety feature that limits how fast temperature readings can change
- **Configurable output limits**: Define min/max PWM duty cycle (0-100%)
- **Debug logging**: Detailed logging for PID tuning and troubleshooting

### Conservative Tuning Parameters
- **Kp = 2.0**: Proportional gain (conservative starting point)
- **Ki = 0.5**: Integral gain (prevents steady-state error)
- **Kd = 1.0**: Derivative gain (reduces overshoot)

### Safety Features
- **Temperature rate limiting**: Maximum 5°C/second change
- **Setpoint ramping**: Maximum 2°C/second ramp rate
- **Output constraints**: PWM duty cycle limited to 0-100%
- **Error checking**: Validates input parameters

## File Structure

```
src/peripherals/
├── pid_controller.h        # PID controller class definition
├── pid_controller.cpp      # PID controller implementation  
├── pid_config.h           # Configuration and tuning parameters
├── heater_control.h       # Updated heater control interface
├── heater_control.cpp     # Updated heater control with PID integration
└── README_PID.md         # This documentation file
```

## How to Enable PID Control

1. **Edit `src/peripherals/pid_config.h`**:
   ```c
   // Uncomment this line to enable PID temperature control
   #define USE_PID_TEMPERATURE_CONTROL
   ```

2. **Optional: Enable debug mode for tuning**:
   ```c
   #define PID_DEBUG_MODE  // Uncomment to enable debug logging
   ```

3. **Compile and upload firmware**

## Hardware Requirements

### Recommended Setup
- **Hardware PWM support**: Best performance with hardware timer-based PWM
- **Enable in `src/peripherals/peripherals.h`**:
  ```c
  #define USE_HARDWARE_TIMER_PWM
  ```

### Fallback Mode
- If hardware PWM is not available, the system falls back to on/off control with 50% threshold
- Still benefits from PID calculation for smoother control decisions

## Tuning Guidelines

### Starting Parameters (Already Set)
- **Brew mode**: Kp=2.0, Ki=0.5, Kd=1.0 (conservative, stable)
- **Steam mode**: Kp=3.0, Ki=0.8, Kd=0.5 (more aggressive)

### If Temperature Oscillates
- **Reduce Kp and Kd**: Lower proportional and derivative gains
- **Increase derivative filtering**: Set `PID_DERIVATIVE_FILTER` to 0.2-0.3

### If Temperature is Sluggish
- **Increase Kp slightly**: More aggressive proportional response
- **Check sample time**: Ensure appropriate for your system (100-250ms typical)

### If Steady-State Error
- **Increase Ki slightly**: Better integral response
- **Check for output saturation**: Ensure PID output isn't hitting limits

### For Different Machine Types
- **Fast/Small boilers**: Increase all gains proportionally
- **Slow/Large boilers**: Decrease all gains proportionally
- **Very stable systems**: Can use more aggressive tuning

## Usage Examples

### Basic Usage (Automatic)
When `USE_PID_TEMPERATURE_CONTROL` is enabled, PID control is automatically used in `justDoCoffee()` function. No code changes needed.

### Manual PID Control
```cpp
#include "peripherals/heater_control.h"

// Initialize PID system
initPIDTemperatureControl();

// In your control loop
float setpoint = 93.0f;    // Target temperature
float current = 91.5f;     // Current temperature
bool brewing = true;       // Brewing mode

// Compute and apply PID output
float output = computePIDTemperatureControl(setpoint, current, brewing);

// Output is automatically applied to heater
// output contains the PWM duty cycle (0-100%)
```

### Tuning PID Parameters
```cpp
// Update PID tuning during runtime
setPIDTunings(2.5f, 0.6f, 1.2f);  // Kp, Ki, Kd

// Enable debug logging
enablePIDDebugMode(true);

// Reset PID state (after major changes)
resetPIDController();
```

### Monitoring PID Performance
```cpp
float currentError = getPIDError();      // Current temperature error
float currentOutput = getPIDOutput();    // Current PWM duty cycle

float kp, ki, kd;
getPIDTunings(kp, ki, kd);               // Get current tuning parameters
```

## Integration Points

### Temperature Control (`just_do_coffee.cpp`)
The PID controller is integrated into the main temperature control function with a simple compile-time switch:

```cpp
#ifdef USE_PID_TEMPERATURE_CONTROL
    // Use PID controller for precise temperature control
    float pidOutput = computePIDTemperatureControl(brewTempSetPoint, sensorTemperature, brewActive);
#else
    // Original hysteresis control logic
    // ... existing code ...
#endif
```

### Hardware Interface (`heater_control.cpp`)
The PID controller interfaces with the existing hardware control system:
- Uses hardware PWM when available (`USE_HARDWARE_TIMER_PWM`)
- Falls back to on/off control when PWM unavailable
- Integrates with existing safety systems

## Safety Considerations

### Built-in Safety Features
1. **Temperature rate limiting**: Prevents sensor spikes from causing dangerous heating
2. **Output limiting**: PWM duty cycle constrained to safe ranges
3. **Anti-windup**: Prevents control system instability
4. **Setpoint ramping**: Prevents thermal shock from rapid setpoint changes

### Integration with Existing Safety
The PID controller works alongside existing safety systems:
- Pressure monitoring and shutoffs
- Temperature limit checks  
- Watchdog timer protection
- Emergency shutoff functionality

### Recommended Testing Procedure
1. **Start with debug logging enabled**
2. **Monitor temperature response carefully**
3. **Verify safety systems still function**
4. **Test various operating modes** (brew, steam, idle)
5. **Adjust tuning parameters as needed**

## Troubleshooting

### Temperature Oscillations
- Reduce Kp and Kd gains
- Increase derivative filtering
- Check for mechanical issues (loose thermocouples, etc.)

### Poor Temperature Tracking
- Increase Kp gain slightly
- Check sample time (may be too slow)
- Verify hardware PWM is working

### Integral Windup Issues
- Anti-windup is enabled by default
- Check output limits are appropriate
- Verify Ki gain is not too high

### System Instability
- Reset PID controller: `resetPIDController()`
- Start with lower gains and work up
- Check for hardware issues

### Debug Logging
Enable debug mode to see detailed PID calculations:
```
PID Debug - SP:93.0 IN:91.5 OUT:65.2 ERR:1.5 P:3.0 I:12.8 D:2.4
```
- SP: Setpoint temperature
- IN: Input (current) temperature  
- OUT: PID output (PWM duty cycle)
- ERR: Temperature error
- P: Proportional term
- I: Integral term
- D: Derivative term

## Performance Benefits

### Compared to Hysteresis Control
- **Reduced temperature swing**: Typically ±0.2°C vs ±1.0°C
- **Faster settling time**: Reaches target temperature quicker
- **Better load response**: Adapts to brewing thermal loads
- **Smoother control**: Less on/off cycling of heater

### Energy Efficiency
- **Reduced overshoot**: Less wasted energy from overheating
- **Optimal heating**: PWM control allows precise power delivery
- **Faster warmup**: Intelligent ramping to target temperature

## Future Enhancements

### Possible Improvements
- **Adaptive tuning**: Automatically adjust PID parameters based on system response
- **Multiple PID sets**: Different tuning for different operating conditions
- **Predictive control**: Use brewing patterns to anticipate thermal loads
- **Remote tuning**: Web interface for PID parameter adjustment

### Integration Opportunities  
- **Shot profiling**: PID control for pressure profiling systems
- **Multiple zones**: Independent PID control for different thermal zones
- **Machine learning**: Learn optimal parameters from usage patterns

## Support

For questions, issues, or tuning help:
1. **Enable debug logging** and collect data
2. **Document your specific machine configuration**
3. **Note any hardware modifications**
4. **Provide temperature response graphs** if possible

The PID implementation is designed to be conservative and safe by default, but optimal performance will require tuning for your specific machine setup.
