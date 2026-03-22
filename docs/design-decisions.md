# Design Decisions — CEV_MCont_V1.0

This document explains the engineering decisions behind the design of **CEV_MCont_V1.0**, a BLDC motor controller developed by Cornell Electric Vehicles for the Shell Eco-Marathon Urban Concept competition.

The V1.0 design represents a major architectural shift from earlier revisions by replacing the integrated motor driver with a fully software-controlled architecture.

---

# System Design Goals

The motor controller was designed with the following goals:

* High efficiency
* High current capability
* Deterministic switching behavior
* Minimal switching losses
* Robust operation in an automotive environment
* Simple and transparent firmware

The system operates from a **48 V battery pack** with a maximum bus voltage of approximately **56 V**, and is designed for up to **60 V absolute maximum**.

---

# Removal of Integrated Gate Driver

Earlier revisions used the **DRV8353** integrated gate driver.

Although convenient, this architecture had several limitations:

* Gate driver located far from MOSFETs
* Limited gate drive strength
* Difficult debugging when driver faults occurred
* Reduced flexibility in switching timing

In V1.0 the DRV8353 was removed entirely.

Instead, the **RP2040 directly controls external gate drivers**, which significantly improves:

* switching performance
* layout flexibility
* debugging transparency
* firmware control of switching behavior

---

# Use of External Gate Drivers

The controller uses **UCC27301 gate drivers**.

Advantages:

* strong gate drive current capability
* simple interface
* excellent switching performance

Gate resistors are asymmetric:

| MOSFET    | Gate Resistor |
| --------- | ------------- |
| Low-side  | 1 Ω           |
| High-side | 4.02 Ω        |

The larger high-side resistor intentionally slows the rising edge of the switching node.

Benefits:

* reduces dV/dt induced turn-on
* prevents accidental low-side MOSFET activation
* improves switching stability

---

# RP2040 Control Architecture

The RP2040 was chosen for several reasons:

* dual-core architecture
* programmable I/O (PIO)
* deterministic timing capabilities
* inexpensive and widely available

The PIO hardware allows precise timing for:

* PWM generation
* deadtime insertion
* gate drive signal updates

This avoids timing jitter that would occur if switching signals were generated purely in software.

---

# Switching Frequency Selection

The inverter switching frequency is **5 kHz**.

This relatively low switching frequency was chosen intentionally.

Advantages:

* significantly lower switching loss
* reduced gate drive power
* reduced MOSFET heating
* sufficient acoustic performance for the application

Because the vehicle prioritizes efficiency over acoustic noise, the lower switching frequency is acceptable.

---

# Deadtime Implementation

Deadtime is implemented using an RP2040 PIO state machine.

Deadtime duration:

```id="deadtime_v1"
32 cycles
≈256 ns
```

Deadtime is applied **only to the phase that switches between MOSFETs**.

This minimizes conduction losses while preventing shoot-through.

---

# MOSFET Selection

V1.0 uses:

```id="mosfet_v1"
NTBLS0D8N08XTXG
```

Key characteristics:

* 100 V VDS rating
* extremely low **0.79 mΩ RDS(on)**
* package optimized for high-current power stages

These devices replaced earlier MOSFETs used in previous revisions.

The selection prioritized:

* minimal conduction loss
* strong thermal performance
* compact layout

---

# Hall Sensor Interface

The motor includes embedded hall sensors.

Characteristics:

* powered from **5 V**
* **open-drain outputs**

Because the outputs are open-drain, pull-up resistors bring the logic level to **3.3 V**, allowing safe direct connection to the RP2040.

This approach avoids the need for level-shifting hardware.

---

# SPI Telemetry Interface

The motor controller communicates with the vehicle's central computer using **SPI**.

SPI signals pass through a **digital isolator** to improve system robustness and reduce electrical noise coupling between subsystems.

The SPI interface is used for telemetry and throttle reporting.

---

# Control Strategy

The controller uses **sensored six-step trapezoidal commutation**.

Reasons for choosing this control method:

* simple implementation
* robust operation
* low computational cost
* excellent efficiency for the vehicle’s operating conditions

Field-oriented control was considered but was not necessary for this application.

---

# Thermal Design

Thermal simulations indicate the controller can support:

| Condition                  | Current |
| -------------------------- | ------- |
| Continuous (with heatsink) | ~110 A  |
| Peak                       | ~160 A  |

Peak current can be sustained for approximately **2 seconds**.

These values depend strongly on airflow and heatsink performance.

---

# Summary

The V1.0 architecture prioritizes:

* switching efficiency
* deterministic timing
* layout-driven power performance
* firmware simplicity

This architecture proved significantly more robust than earlier integrated driver designs.
