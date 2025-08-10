/* PID Temperature Control Configuration */
#ifndef PID_CONFIG_H
#define PID_CONFIG_H

/* 
 * ENABLE PID TEMPERATURE CONTROL
 * 
 * Uncomment the line below to enable PID temperature control instead of
 * the default hysteresis control. PID control provides:
 * 
 * - More precise temperature control
 * - Reduced temperature oscillations  
 * - Better response to load changes
 * - Setpoint ramping to prevent overshoot
 * - Temperature rate limiting for safety
 * 
 * IMPORTANT: Before enabling PID control:
 * 1. Ensure your hardware supports PWM on the heater relay pin OR
 * 2. Be prepared to tune the PID parameters for your specific setup
 * 
 * Initial conservative tuning parameters are provided but may need adjustment.
 */

// Uncomment this line to enable PID temperature control
// PID temperature control disabled; using original time-proportional control

/* 
 * PID TUNING PARAMETERS
 * 
 * These are conservative starting values suitable for most espresso machines.
 * You may need to adjust these based on your specific machine characteristics:
 * 
 * - Kp (Proportional): How aggressively to respond to current error
 * - Ki (Integral): How aggressively to respond to accumulated error  
 * - Kd (Derivative): How aggressively to respond to error rate of change
 * 
 * Tuning guidelines:
 * - Start with these conservative values
 * - If temperature oscillates, reduce Kp and Kd
 * - If temperature is sluggish, increase Kp slightly
 * - If there's steady-state error, increase Ki slightly
 * - For faster machines, increase all gains proportionally
 * - For slower/larger boilers, decrease all gains proportionally
 */

// Brew mode PID tuning (active brewing - more aggressive for quick response)
#define PID_BREW_KP  2.5f   // Proportional gain
#define PID_BREW_KI  0.8f   // Integral gain  
#define PID_BREW_KD  1.2f   // Derivative gain

// Idle mode PID tuning (standby - conservative for stability)
#define PID_IDLE_KP  2.0f   // Proportional gain (reduced for smoother control)
#define PID_IDLE_KI  0.35f  // Integral gain (reduced to prevent overshoot)
#define PID_IDLE_KD  1.44f  // Derivative gain (increased for predictive control)

// Steam mode PID tuning (more aggressive for faster response)
#define PID_STEAM_KP 3.0f   // Proportional gain
#define PID_STEAM_KI 0.8f   // Integral gain
#define PID_STEAM_KD 0.5f   // Derivative gain

/*
 * SAFETY PARAMETERS
 * 
 * These parameters help prevent dangerous temperature swings and provide
 * smooth transitions when changing setpoints.
 */

// Maximum temperature change rate (°C/second) - safety feature
#define PID_MAX_TEMP_RATE       5.0f

// Maximum setpoint ramp rate (°C/second) - prevents overshoot
#define PID_MAX_SETPOINT_RAMP   2.0f

// Derivative filtering (0.0 = no filter, 1.0 = maximum filtering)
#define PID_DERIVATIVE_FILTER   0.15f

/*
 * OUTPUT LIMITS
 * 
 * These define the range of PWM duty cycle output from the PID controller.
 * 0% = heater completely off, 100% = heater at maximum power
 */
#define PID_OUTPUT_MIN  0.0f    // Minimum PWM duty cycle
#define PID_OUTPUT_MAX  100.0f  // Maximum PWM duty cycle

/*
 * SAMPLE TIME AND MODE TRANSITION SETTINGS
 * 
 * How frequently the PID calculation is performed (milliseconds).
 * Faster sample times provide better control but use more CPU.
 * 100-250ms is typically good for temperature control.
 * 
 * SMOOTH_TRANSITION_DURATION defines how long it takes to smoothly
 * transition between brew and idle PID parameters to prevent abrupt
 * output changes when switching modes.
 */
#define PID_SAMPLE_TIME_BREW  200   // 200ms for brew mode
#define PID_SAMPLE_TIME_STEAM 150   // 150ms for steam mode (faster)
#define PID_SAMPLE_TIME_IDLE  250   // 250ms for idle mode (slower, more stable)

// Mode transition parameters
#define PID_MODE_TRANSITION_TIME 3000  // 3 seconds for smooth mode transitions

/*
 * DEBUG OPTIONS
 * 
 * Enable debug logging to help with PID tuning and troubleshooting.
 * This will output PID parameters and calculations to the serial log.
 */
// #define PID_DEBUG_MODE  // Uncomment to enable debug logging

#endif // PID_CONFIG_H
