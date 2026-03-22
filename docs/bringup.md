# Bringup Procedure — CEV_MCont_V1.0

This document describes the recommended procedure for bringing up the motor controller safely.

---

# 1. Visual Inspection

Before powering the board:

* inspect for solder bridges
* verify MOSFET orientation
* verify gate driver orientation
* inspect power connectors

---

# 2. Resistance Checks

Using a multimeter:

Check resistance between:

* VBUS and GND
* phase outputs and ground

There should be **no short circuits**.

---

# 3. Power Logic Rails

Power the logic system first.

Verify:

| Rail  | Expected Voltage   |
| ----- | ------------------ |
| 3.3 V | RP2040 supply      |
| 5 V   | hall sensor supply |
| 15 V  | gate driver supply |

---

# 4. Flash Firmware

Connect USB to the RP2040.

Flash firmware using the RP2040 UF2 bootloader.

Confirm the firmware boots successfully.

---

# 5. Verify Hall Inputs

Rotate the motor by hand and verify that hall signals toggle.

Confirm that rotor state changes are detected by firmware.

---

# 6. Power the Bus

Apply bus voltage from the battery.

Ensure the battery voltage is within the safe operating range:

```id="bus_range"
0–60 V
```

---

# 7. Verify Gate Signals

Using an oscilloscope:

Check gate drive outputs.

Verify:

* PWM switching
* proper deadtime
* no overlapping gate signals

---

# 8. Initial Motor Test

Connect the motor and slowly apply throttle.

Verify:

* correct rotation direction
* smooth commutation
* stable PWM behavior

---

# 9. Monitor Temperature

During early testing:

* monitor MOSFET temperature
* ensure adequate cooling

High currents should not be applied until thermal performance is verified.

---

# Summary

Following this procedure helps ensure safe first operation of the motor controller and reduces the risk of damaging components.
