/* Heater Control Interface */
#ifndef HEATER_CONTROL_H
#define HEATER_CONTROL_H

#include "pid_controller.h"

// Forward declarations for heater control functions
void setBoilerOn(void);
void setBoilerOff(void);
void setPumpOff(void);

// PID-based heater control functions
void initPIDTemperatureControl();
void setPIDHeaterOutput(float dutyCycle);
float computePIDTemperatureControl(float setpoint, float currentTemp, bool isBrewMode = false);
void resetPIDController();
void setPIDTunings(float kp, float ki, float kd);
void enablePIDDebugMode(bool enabled);

// Get current PID controller status
float getPIDOutput();
float getPIDError();
void getPIDTunings(float &kp, float &ki, float &kd);

// Temperature safety and diagnostics integration
void enablePIDDiagnosticMode(bool enabled);
void logPIDDiagnostics(float setpoint, float currentTemp, float output, bool heaterState);
bool checkTemperatureSafetyLimits(float currentTemp, bool heaterState);

// PWM heater control functions (map to simple on/off when hardware PWM not available)
void setHeaterDutyCycle(uint8_t dutyCycle);
void heaterHardwareOff(void);

#endif // HEATER_CONTROL_H
