# CEV Motor Controller V1.0

Open-source BLDC motor controller developed by **Cornell Electric Vehicles (CEV)** for the **Shell Eco-Marathon Urban Concept** competition.

This controller drives a **48 V battery electric drivetrain** using **sensored trapezoidal commutation** implemented on an **RP2040 microcontroller**. The design emphasizes:

* high efficiency
* high current capability
* deterministic switching behavior
* minimal firmware complexity

The controller supports:

* **48 V nominal battery systems**
* **60 V maximum bus voltage**
* **110 A continuous current (with heatsink)**
* **160 A peak current (~2 s)**

The firmware uses **RP2040 PIO state machines** to generate precise PWM and gate drive signals with nanosecond-scale deadtime control.

---

# Repository Structure

```
hardware/
    CEV_MCont_V1.0/
        PCB design files here

firmware/
    MCont_V1.0-main/
        pico project here

docs/
    design-decisions.md
    power-stage-design.md
    layout-lessons.md
    bringup.md
    firmware.md
```

---

# System Architecture

```
Throttle (analog pedal)
        │
        ▼
RP2040 Microcontroller
        │
        ├── PIO PWM generator
        ├── PIO gate-drive timing
        ├── Hall sensor processing
        └── V/Hz control algorithm
        │
        ▼
External Gate Drivers (UCC27301)
        │
        ▼
3-Phase MOSFET Inverter
        │
        ▼
BLDC Motor
```

Telemetry and throttle reporting are transmitted to the vehicle control system through **SPI** using a **digital isolator**.

---

# Key Specifications

| Parameter           | Value        |
| ------------------- | ------------ |
| Bus voltage         | 48 V nominal |
| Maximum bus voltage | 60 V         |
| Continuous current  | ~110 A       |
| Peak current        | ~160 A       |
| Switching frequency | 5 kHz        |
| Deadtime            | 256 ns       |
| Gate drive voltage  | 15 V         |

---

# Major Hardware Components

### Microcontroller

**RP2040**

Responsibilities:

* hall sensor decoding
* commutation logic
* speed estimation
* PWM duty control
* gate drive timing coordination

---

### Gate Drivers

**UCC27301**

Features:

* high peak drive current
* fast switching capability
* 15 V gate drive

Gate resistors:

| MOSFET    | Gate resistor |
| --------- | ------------- |
| Low side  | 1 Ω           |
| High side | 4.02 Ω        |

High-side resistors are intentionally larger to reduce dV/dt-induced shoot-through.

---

### MOSFETs

**NTBLS0D8N08XTXG**

Characteristics:

* 100 V VDS
* **0.79 mΩ RDS(on)**
* power package optimized for high current

These devices replaced earlier MOSFETs used in prior revisions.

---

# Motor Commutation

The controller uses **sensored six-step trapezoidal commutation**.

Three hall sensors embedded in the motor determine rotor position.

At any time:

* one phase is **PWM driven**
* one phase is **pulled to ground**
* one phase is **tristated**

This minimizes switching events while maintaining torque production.

---

# Firmware Highlights

The firmware leverages **RP2040 PIO** to ensure precise switching timing:

* PIO generates the PWM signal
* PIO generates gate drive outputs
* CPU performs commutation logic

This architecture allows deterministic switching and very low jitter.

---

# Deadtime Strategy

Deadtime is inserted only on the phase that switches between high-side and low-side conduction.

Deadtime is:

```
32 PIO cycles
≈256 ns
```

This prevents shoot-through while minimizing switching loss.

---

# Hall Sensor Interface

The hall sensors:

* operate at **5 V supply**
* use **open-drain outputs**

Pull-up resistors bring the logic level to **3.3 V** for the RP2040.

This allows safe level translation without additional hardware.

---

# Telemetry Interface

The controller reports throttle and telemetry over **SPI** to the central vehicle computer.

The SPI interface passes through a **digital isolator** for noise immunity and system safety.

---

# License

This project is released under the **MIT License**.
