# Power Stage Design — CEV_MCont_V1.0

This document describes the design and analysis of the inverter power stage.

The controller implements a **three-phase MOSFET inverter** designed for high efficiency and high current capability.

---

# Inverter Topology

The motor controller uses a standard **three-phase half-bridge inverter**.

Each phase consists of:

* one high-side MOSFET
* one low-side MOSFET

Total MOSFET count:

```id="fet_count"
6
```

---

# MOSFET Selection

The selected device:

```id="fet_type"
NTBLS0D8N08XTXG
```

Important parameters:

| Parameter  | Value                      |
| ---------- | -------------------------- |
| VDS rating | 100 V                      |
| RDS(on)    | 0.79 mΩ                    |
| Package    | High-current power package |

Low RDS(on) was the primary selection criterion to minimize conduction losses.

---

# Loss Model

MOSFET losses were modeled using the equation:

```id="loss_equation"
P_loss =
(I_rms^2 * R_DS_ON)
+ (V_bus * I_out / 2 * (t_on + t_off) * f_sw)
+ (V_bus^2 * C_oss * f_sw)
+ (V_g^2 * C_gs * f_sw)
```

Components:

| Term                    | Description                          |
| ----------------------- | ------------------------------------ |
| Conduction loss         | resistive MOSFET loss                |
| Switching loss          | energy dissipated during transitions |
| Output capacitance loss | Coss charge/discharge                |
| Gate drive loss         | energy required to charge gate       |

---

# Switching Performance

Measured switching characteristics:

| Parameter           | Value       |
| ------------------- | ----------- |
| VGS plateau         | ~100-150 ns |
| VDS rise/fall       | ~200-300 ns |
| Switching frequency | 5 kHz       |

Low switching frequency significantly reduces switching losses.

---

# Gate Drive Voltage

Gate drivers operate at:

```id="gate_voltage"
15 V
```

Higher gate voltage reduces MOSFET RDS(on), lowering conduction loss.

---

# Gate Drive Strategy

Gate resistors are asymmetric:

| MOSFET    | Gate resistor |
| --------- | ------------- |
| Low side  | 1 Ω           |
| High side | 4.02 Ω        |

The high-side resistor slows the switching edge slightly to prevent dV/dt-induced turn-on of the low-side MOSFET.

---

# Deadtime

Deadtime prevents shoot-through in each half bridge.

Deadtime duration:

```id="deadtime_value"
256 ns
```

Deadtime is inserted only when a phase transitions between high-side and low-side conduction.

---

# Bus Voltage

System bus parameters:

| Parameter       | Value |
| --------------- | ----- |
| Nominal battery | 48 V  |
| Maximum battery | ~56 V |
| Design limit    | 60 V  |

MOSFETs were selected with sufficient voltage margin for this operating range.

---

# Thermal Performance

Thermal simulations predict:

| Condition  | Current |
| ---------- | ------- |
| Continuous | ~110 A  |
| Peak       | ~160 A  |

These values assume the use of a heatsink.

Without a heatsink, current capability is significantly reduced.

---

# Switching Loss Optimization

Several design decisions were made to reduce switching losses:

* low switching frequency
* strong gate drive current
* minimized gate loop inductance
* high gate drive voltage

These improvements significantly reduce MOSFET heating.

---

# Summary

The power stage was optimized for:

* extremely low conduction losses
* controlled switching transitions
* high current capability

The resulting design provides efficient operation within the Eco-Marathon vehicle power system.
