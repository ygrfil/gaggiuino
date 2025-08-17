/* Centralized heater tuning constants */
#ifndef CONTROL_TUNING_H
#define CONTROL_TUNING_H

#include <Arduino.h>

// Cooldown lockout when above setpoint
inline constexpr float HEAT_LOCKOUT_DELTA_C = 0.5f; // °C
inline constexpr uint32_t HEAT_LOCKOUT_MS = 8000UL; // ms

// Slope thresholds (temperature change rate) for braking near setpoint
inline constexpr float SLOPE_BRAKE_THRESH_A = 0.10f; // °C/s
inline constexpr float SLOPE_BRAKE_THRESH_B = 0.15f; // °C/s
inline constexpr float SLOPE_BRAKE_THRESH_C = 0.20f; // °C/s
inline constexpr float SLOPE_BRAKE_THRESH_D = 0.25f; // °C/s

// Time-proportional heating periods (ms)
inline constexpr uint32_t HEAT_PERIOD_FAR   = 1000UL;
inline constexpr uint32_t HEAT_PERIOD_HIGH  = 2000UL;
inline constexpr uint32_t HEAT_PERIOD_MID   = 3000UL;
inline constexpr uint32_t HEAT_PERIOD_NEAR1 = 4000UL;
inline constexpr uint32_t HEAT_PERIOD_NEAR2 = 5000UL;
inline constexpr uint32_t HEAT_PERIOD_NEAR3 = 6000UL;
inline constexpr uint32_t HEAT_PERIOD_NEAR4 = 7000UL;

// Duty suggestions (%) for the above bands
inline constexpr uint8_t HEAT_DUTY_FAR   = 100;
inline constexpr uint8_t HEAT_DUTY_HIGH  = 60;
inline constexpr uint8_t HEAT_DUTY_MID   = 35;
inline constexpr uint8_t HEAT_DUTY_NEAR1 = 18;
inline constexpr uint8_t HEAT_DUTY_NEAR2 = 10;
inline constexpr uint8_t HEAT_DUTY_NEAR3 = 5;
inline constexpr uint8_t HEAT_DUTY_NEAR4 = 3;

#endif // CONTROL_TUNING_H




