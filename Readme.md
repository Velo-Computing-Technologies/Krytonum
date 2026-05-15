# Krytonum OS

Krytonum is a long-term, multi-year R&D system engineering project focused on building an independent, minimal-overhead bare-metal execution architecture. 

---

## Project Paradigm & Horizon

This repository functions as an ongoing architectural workshop managed by **Velo Computing Technologies**. 

* **Timeline Expectation:** This is a generational, foundational project with an estimated 5–10 year architectural evolution horizon. 
* **Current Utility:** **Non-functional / Pre-alpha Structural Setup.** The code assets currently tracking in this repository represent initial bootstrapping tests, driver stubs (PCI, network links, keyboard), and runtime layout configurations. It is not currently runnable as a functional general-purpose computing platform.
* **Migration Note:** Source configurations are gradually shifting namespaces from its initial baseline iteration identity, `OrionOS`, over to `Krytonum`.

---

## Long-Term Research Steps

The development track is explicitly decoupled into long-range milestones:

1. **Bootstrap Stability (Active Target):** Hardening early x86_64 target configuration profiles, testing interface layouts, and stabilizing compiler configurations inside the building matrix (`Makefile`, `linker.ld`, `asm` modules).
2. **Subsystem Primitives:** Constructing memory allocation layout maps, validating hardware interface loops, and defining deterministic scheduler mechanics.
3. **Hardware Direct IO:** Isolating low-latency driver stacks (leveraging early network mapping setups like `e1000.cpp` and basic packet handling frameworks).

---

## Layout Reference

* `/` — Project build coordination tools (`Makefile`, configurations)
* `*.asm` — Processor stage entry procedures and low-level stack handling.
* `*.cpp` / `*.h` — Freestanding driver abstractions, network configuration skeletons (`http`, `net`, `mbedtls` configurations), and basic video framing buffers (`orion.h`).

---

## Environment Setup

Because the kernel compiles in a freestanding environment without native OS library ecosystems, changes require explicitly configured toolchains.

### Required Utilities
* **Assembler:** `nasm`
* **Compiler Build Array:** `gcc` or `clang` targets configured for custom standalone compilation output.
* **Automation Handler:** `make`

```bash
# Clone repository blocks
git clone --recursive https://github.com
cd Krytonum

# Perform structural syntax parsing build checks
make
```

---

## Legals & Scope
Distributed under the open MIT License models. All development targets focus entirely on exploratory architecture primitives.
