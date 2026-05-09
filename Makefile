# -------------------------------------------------------------------------
# Krytonium OS — x86_64 UEFI build system
#
# Targets:
#   all        Build kernel.elf
#   iso        Build build/krytonium.iso (bootable in QEMU / USB)
#   run        Launch in QEMU with UEFI firmware (requires OVMF)
#   clean      Remove build artefacts
# -------------------------------------------------------------------------

# Toolchain (system cross-compiler; works with freestanding flags)
CC   = x86_64-linux-gnu-g++
AS   = nasm
LD   = x86_64-linux-gnu-g++

# Compiler flags for a freestanding x86_64 higher-half kernel
CFLAGS  = -std=gnu++17                  \
           -ffreestanding               \
           -fno-exceptions              \
           -fno-rtti                    \
           -fno-stack-protector         \
           -fno-pic                     \
           -fno-pie                     \
           -mcmodel=kernel              \
           -mno-red-zone                \
           -mno-mmx                     \
           -mno-sse                     \
           -mno-sse2                    \
           -O2                          \
           -Wall                        \
           -Wextra                      \
           -I.

# Assembler flags (NASM, ELF64)
ASFLAGS = -f elf64

# Linker flags
LDFLAGS = -ffreestanding               \
           -nostdlib                   \
           -static                     \
           -mcmodel=kernel             \
           -mno-red-zone               \
           -mno-mmx                    \
           -mno-sse                    \
           -mno-sse2                   \
           -z max-page-size=0x1000     \
           -T linker.ld                \
           -Wl,--build-id=none

# Source files
C_SRCS  = main.cpp login.cpp app.cpp terminal.cpp default.cpp \
           system.cpp settings.cpp                             \
           ps2kbd.cpp pci.cpp e1000.cpp net.cpp http.cpp

ASM_SRCS = kernel_entry.asm

OBJS = $(C_SRCS:.cpp=.o) $(ASM_SRCS:.asm=.o)

KERNEL = kernel.elf

# Limine binaries (downloaded to /tmp/limine/limine-binary by the setup step)
LIMINE_DIR ?= /tmp/limine/limine-binary

# -------------------------------------------------------------------------
all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# -------------------------------------------------------------------------
# ISO image (bootable via QEMU UEFI / USB stick)
#
# Uses the standard Limine hybrid ISO layout:
#   - BIOS CD El Torito boot (limine-bios-cd.bin) so Limine can resolve
#     the boot volume correctly under UEFI (avoids the "ambiguous volume"
#     warning in Limine v8+)
#   - UEFI El Torito boot (limine-uefi-cd.bin)
#   - EFI/BOOT/BOOTX64.EFI fallback path for UEFI removable-media boot
# -------------------------------------------------------------------------
iso: $(KERNEL)
	mkdir -p build/iso_root/boot/limine
	mkdir -p build/iso_root/EFI/BOOT
	cp $(KERNEL)                              build/iso_root/boot/kernel.elf
	cp limine.conf                            build/iso_root/boot/limine/limine.conf
	cp $(LIMINE_DIR)/limine-bios-cd.bin       build/iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-bios.sys          build/iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin       build/iso_root/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI             build/iso_root/EFI/BOOT/BOOTX64.EFI
	cp $(LIMINE_DIR)/BOOTX64.EFI             build/iso_root/boot/limine/BOOTX64.EFI
	xorriso -as mkisofs                                           \
	  -b boot/limine/limine-bios-cd.bin                          \
	  -no-emul-boot -boot-load-size 4 -boot-info-table           \
	  --protective-msdos-label                                    \
	  -eltorito-alt-boot                                          \
	  -e boot/limine/limine-uefi-cd.bin                          \
	  -no-emul-boot --efi-boot-part --efi-boot-image             \
	  -R -J                                                       \
	  -o build/krytonium.iso                                      \
	  build/iso_root 2>/dev/null
	@echo "ISO: build/krytonium.iso"

# -------------------------------------------------------------------------
# Run in QEMU with UEFI firmware
# Requires: qemu-system-x86_64, OVMF (sudo apt install ovmf)
# -------------------------------------------------------------------------
OVMF ?= /usr/share/OVMF/OVMF_CODE.fd

run: iso
	qemu-system-x86_64                              \
	  -M q35                                        \
	  -m 256M                                       \
	  -bios $(OVMF)                                 \
	  -cdrom build/krytonium.iso                    \
	  -netdev user,id=n0,net=192.168.1.0/24,host=192.168.1.1 \
	  -device e1000,netdev=n0                       \
	  -serial stdio

# -------------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(KERNEL)
	rm -rf build

.PHONY: all iso run clean
