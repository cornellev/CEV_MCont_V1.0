# PCB Layout Lessons — CEV_MCont_V1.0

Power electronics performance is heavily influenced by PCB layout.

Many of the improvements in V1.0 were driven by lessons learned from earlier controller revisions.

---

# Minimize Switching Loop Area

The highest current loop in the system is:

```id="switch_loop"
DC link capacitor
→ high-side MOSFET
→ phase node
→ low-side MOSFET
→ ground
```

This loop must be made as small as possible.

Reducing this loop area:

* lowers switching inductance
* reduces voltage spikes
* improves EMI performance

---

# Gate Driver Placement

Gate drivers should be placed **as close as possible to the MOSFET gates**.

Long gate traces introduce:

* inductance
* ringing
* slower switching transitions

Using external drivers allowed them to be positioned directly next to the MOSFETs.

---

# Gate Drive Return Path

Gate signals must have a tight return path.

Key rule:

* gate trace and return path should run directly above a solid ground plane.

Interruptions in the ground plane increase loop inductance and degrade switching performance.

---

# Kelvin Source Routing

For accurate gate drive reference, the source connection of each MOSFET should be routed using **Kelvin connections** where possible.

This reduces the effect of source inductance during switching.

---

# Ground Plane Strategy

A continuous ground plane should be maintained under:

* gate drivers
* microcontroller
* signal traces

This provides a low-impedance return path for switching currents.

---

# Separate Power and Logic Domains

High current power paths should be separated from sensitive logic circuitry.

Key strategies:

* dedicated ground returns
* short current paths
* careful routing of high di/dt nodes

---

# DC Link Capacitor Placement

The DC link capacitors should be placed directly across the power stage.

Poor capacitor placement increases:

* switching loop inductance
* voltage overshoot
* capacitor heating

---

# Thermal Copper

High-current regions require large copper pours to distribute heat.

Additional copper layers were added in V1.0 to improve thermal performance.

---

# Summary

Careful PCB layout is essential for reliable high-current power electronics.

Many improvements in V1.0 resulted directly from optimizing the current loops and gate drive layout.
