# AquaOS build - needs: nasm, gcc, binutils, xorriso (python3 only for nothing; no Linux kernel/bootloader used)
CFLAGS64 = -m64 -ffreestanding -fno-pie -fno-stack-protector -fwrapv -O2 -mno-red-zone -mgeneral-regs-only -Wall -Wno-array-bounds -Wno-misleading-indentation -fno-delete-null-pointer-checks -Wno-unused-function -nostdlib -fno-asynchronous-unwind-tables
CFLAGS32 = -m32 -march=i486 -ffreestanding -fno-pie -fno-stack-protector -fwrapv -O2 -mgeneral-regs-only -Wall -Wno-array-bounds -Wno-misleading-indentation -fno-delete-null-pointer-checks -Wno-unused-function -nostdlib -fno-asynchronous-unwind-tables
SRC = kernel/main.c kernel/lib.c kernel/hw.c kernel/gfx.c kernel/fs.c kernel/wm.c kernel/apps.c kernel/apps2.c
HDR = kernel/aqua.h

all: AquaOS.iso

build/k64.elf: $(SRC) $(HDR) kernel/start64.asm kernel/link.ld
	mkdir -p build
	nasm -f elf64 kernel/start64.asm -o build/start64.o
	gcc $(CFLAGS64) -c kernel/main.c -o build/m64_main.o
	for f in lib hw gfx fs wm apps apps2; do gcc $(CFLAGS64) -c kernel/$$f.c -o build/m64_$$f.o || exit 1; done
	ld -m elf_x86_64 -T kernel/link.ld -o $@ build/start64.o build/m64_*.o
build/k32.elf: $(SRC) $(HDR) kernel/start32.asm kernel/link.ld
	mkdir -p build
	nasm -f elf32 kernel/start32.asm -o build/start32.o
	gcc $(CFLAGS32) -c kernel/main.c -o build/m32_main.o
	for f in lib hw gfx fs wm apps apps2; do gcc $(CFLAGS32) -c kernel/$$f.c -o build/m32_$$f.o || exit 1; done
	ld -m elf_i386 -T kernel/link.ld -o $@ build/start32.o build/m32_*.o
build/k64.bin: build/k64.elf
	objcopy -O binary $< $@
build/k32.bin: build/k32.elf
	objcopy -O binary $< $@
build/boot.img: boot/boot.asm build/k32.bin build/k64.bin
	nasm -f bin -I build/ boot/boot.asm -o $@
AquaOS.iso: build/boot.img
	rm -rf build/iso && mkdir -p build/iso/boot
	cp build/boot.img build/iso/boot/aquaos.img
	cp README.md build/iso/README.txt
	xorriso -as mkisofs -quiet -V AQUAOS -o $@ -b boot/aquaos.img -no-emul-boot -boot-load-size $$(( $$(stat -c %s build/boot.img) / 512 )) build/iso
clean:
	rm -rf build AquaOS.iso
