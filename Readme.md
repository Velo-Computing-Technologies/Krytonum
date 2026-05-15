# Krytonum OS

A low-overhead, bare-metal system architecture engineered for direct hardware resource coordination and execution velocity.

---

## 1. Project Status & Migration Notice

This project is undergoing a structural namespace and branding migration from its legacy working designation, `OrionOS`, to **Krytonum**. 

* **Maintainer:** Velo Computing Technologies
* **Status:** Active Core Development / Refactoring Phase
* **Upstream Namespace Changes:** Legacy identifiers inside system configurations and header files are incrementally transitioning to the `krytonum` namespace.

---

## 2. Technical Profile

### Architecture and Kernel Strategy
* **Design Philosophy:** Minimal hardware abstraction. Direct execution control without high-level wrapper frameworks.
* **Target Platforms:** Bare-metal execution stacks, specialized low-latency computing contexts, and custom virtualized hypervisors.
* **Boot Environment:** Multiboot compliant entry routines via the `Limine` engine infrastructure.

### Component Map
* **Core Runtime (`app.cpp`, `main.cpp`):** Handles baseline execution loops and system setup handoffs.
* **Network Components (`e1000.cpp`, `http.cpp`):** Raw controller interaction loops for network interfaces paired with a structural `mbedtls` cryptographic integration sub-dependency.
* **Subsystems (`pci.cpp`, `ps2kbd.cpp`):** Direct hardware interrogation routines via standard bus protocols and basic legacy peripheral drivers.

---

## 3. Toolchain & Build Environment

Compilation relies strictly on explicit file linkage scripts and native compiler toolchains.

### Dependencies
* **Compiler:** `gcc` setup configurations tuned for freestanding environment development (`-ffreestanding`).
* **Assembler:** NASM compiler utility for early-stage processor preparation targets (`boot.asm`, `kernel_entry.asm`).
* **Automation:** GNU Make (configured inside localized `Makefile`).
* **Submodules:** `mbedtls` library (pinned configuration context).

### Source Build Instructions
Ensure your local system path includes an active ELF cross-compiler toolset before execution.

```bash
# Clone the repository logic including sub-allocations
git clone --recursive https://github.com
cd Krytonum

# Trigger compilation pass via the Makefile infrastructure
make
```

---

## 4. Operational Ecosystem

* **Infrastructure Domain:** [velocomputing.tech](https://velocomputing.tech)
* **Licensing Model:** MIT License (refer to individual `LICENSE` source files inside the repository root).
