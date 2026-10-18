# AquaOS 1.0

A small operating system written from scratch for x86 PCs - its own boot loader,
its own kernels (32-bit and 64-bit), drivers, window manager and apps.
No Linux, no GRUB, no borrowed kernel code.

## What it does
* **Boot loader** (`boot/boot.asm`, BIOS / El Torito): enables A20, reads the BIOS
  memory map, tests the CPU for 64-bit long mode, copies the VGA ROM font, sets a
  VBE 1024x768x32 framebuffer (falls back to 800x600, 640x480), then starts the
  **64-bit kernel on 64-bit CPUs** (paging + long mode) or the **32-bit kernel on
  32-bit CPUs**.
* **Boot terminal**: a verbose, timestamped boot log (real memory map, CPU
  vendor/model/flags, IDT/PIC/PIT/RTC, PS/2 keyboard + mouse detection, filesystem,
  resource loading, progress bars). It takes about 17 seconds; press any key to
  speed it up.
* **Desktop**: wallpaper, desktop icons (double-click), draggable/minimizable/
  maximizable windows, taskbar with window buttons and clock, Start ("Aqua") menu,
  Alt+Tab, Alt+F4, Windows key. Mouse and keyboard supported.
* **Apps**: Terminal (30+ commands), Notepad, Files, Calculator, Paint, Snake,
  Clock & Calendar, System Info, Settings (wallpapers, accent colours).
* **Kernel**: IDT with exception "blue screen", PIC + 1000 Hz PIT timer (idle uses
  `hlt`), CMOS RTC, PS/2 i8042 keyboard and mouse, PC speaker, software renderer
  with double buffering, RAM filesystem, reboot/shutdown.

## Build
Requires `nasm`, `gcc` (with -m32 support), `binutils`, `xorriso`, `make`:

    make          # produces AquaOS.iso
    qemu-system-x86_64 -cdrom AquaOS.iso -m 128
    qemu-system-i386   -cdrom AquaOS.iso -m 128 -cpu qemu32   # 32-bit path

## Requirements / limits (honest list)
* BIOS (legacy / CSM) boot only - no UEFI. At least 32 MiB RAM.
* Needs a VBE linear-framebuffer mode (all emulators and nearly all BIOS PCs).
* PS/2 keyboard and mouse (emulated PS/2 on most PCs and all VMs). USB-only
  devices work only if the firmware offers legacy PS/2 emulation.
* The filesystem lives in RAM: files are cleared on reboot. No disk or network
  drivers yet. Shutdown powers off in QEMU/VirtualBox; on real hardware it shows
  "safe to turn off".
* The ISO boots as a CD/DVD. It is not a hybrid image for USB sticks.

## Layout
    boot/boot.asm        boot loader (real mode -> protected mode -> long mode)
    kernel/start32.asm   32-bit entry + interrupt stubs
    kernel/start64.asm   64-bit entry + interrupt stubs
    kernel/main.c        boot terminal + main loop
    kernel/hw.c          IDT, PIC, PIT, RTC, CPUID, PS/2, speaker, power
    kernel/gfx.c         framebuffer renderer, text, wallpaper
    kernel/wm.c          window manager, taskbar, start menu, icons
    kernel/apps.c        Terminal, Notepad, Files (+ app registry)
    kernel/apps2.c       Calculator, Paint, Snake, Clock, System Info, Settings
    kernel/fs.c, lib.c   RAM filesystem, mini C library
