# Firmware Architecture

This document describes the firmware used in **CEV_MCont_V1.0**.

The firmware implements sensored trapezoidal motor control using the RP2040.

Major subsystems:

* commutation logic
* PIO gate drive state machine
* PIO PWM generator
* velocity estimation
* V/Hz speed controller

---

# Control Architecture

```
Hall sensors → commutation logic
Throttle → V/Hz controller → PWM duty
PWM + commutation → gate drive signals
```

The RP2040 performs the control computations while the **PIO state machines handle precise timing**.

---

# Commutation Logic

The hall sensors determine rotor position.

A three-bit state value is constructed:

```
state = ((a << 2) | (b << 1) | c) - 1
```

This state indexes a lookup table that determines which phase is:

* high
* low
* PWM driven

At each commutation step:

| Phase role     | Behavior            |
| -------------- | ------------------- |
| PWM phase      | PWM applied         |
| Low phase      | connected to ground |
| Floating phase | high-impedance      |

---

# Gate Drive Encoding

Gate states are encoded into a **6-bit control word**:

```
[C_HIGH C_LOW B_HIGH B_LOW A_HIGH A_LOW]
```

Example control output:

```
0b0001PWM!PWM
```

This word is generated using bit-shift masks and lookup tables.

---

# Deadtime Generation

Deadtime is applied only to the phase that switches between MOSFETs.

Deadtime word example:

```
0b000100
```

Deadtime and control values are concatenated into a **12-bit value** sent to the PIO gate driver.

---

# Gate Drive PIO State Machine

The gate drive PIO performs the following sequence:

1. wait for FIFO value
2. output deadtime state
3. wait deadtime delay
4. output control state

Deadtime duration:

```
32 PIO cycles
≈256 ns
```

This ensures the control signal never updates without first inserting deadtime.

---

# PWM PIO State Machine

PWM generation is handled by a separate PIO state machine.

Parameters:

```
wrap = 255
switching frequency ≈ 5 kHz
```

Each cycle:

1. read duty cycle from FIFO
2. count down from wrap
3. toggle output when threshold reached

PIO interrupts trigger updates on **both rising and falling PWM edges**.

---

# Velocity Estimation

Velocity is determined from the time between hall sensor transitions.

Each transition represents:

```
1/24 mechanical revolution
```

because the motor has:

```
4 pole pairs
6 electrical states
```

Velocity equation:

```
raw_rpm = 2.5e6 / step_period
```

---

# Velocity Filtering

Velocity is filtered using a first-order low-pass filter:

```
alpha = timer_period / (TAU + timer_period)

motor_rpm = alpha * raw_rpm + (1 - alpha) * motor_rpm
```

This reduces measurement noise.

---

# V/Hz Speed Controller

Motor voltage is controlled using a **V/Hz strategy**.

The controller calculates a maximum duty cycle proportional to speed.

Throttle input selects a percentage of this maximum.

Benefits:

* constant magnetic flux
* stable torque production
* simple implementation

---

# Interrupt Architecture

The firmware uses both RP2040 cores.

```
Core 0
    PWM interrupt (highest priority)

Core 1
    Hall sensor interrupt

Main loop
    velocity update
```

This architecture reduced PWM jitter from **250 ns to 18 ns**.

---

# Debugging Lessons

Several issues were encountered during development:

### PWM Jitter

Cause:

interrupt contention.

Solution:

restructured interrupt priorities and moved velocity update to main loop.

---

### Shoot-Through

Cause:

incorrect deadtime logic.

Solution:

deadtime applied only to phases that switch.

---

### Hall Sensor Jitter

Observed hardware jitter was filtered in software by preventing reverse commutation.

---

# Summary

Using RP2040 PIO allows:

* deterministic switching
* precise deadtime control
* minimal CPU load

This architecture enables a simple and reliable BLDC controller.
