ASM = nasm
CC = gcc
LD = ld
VM = qemu-system-i386 -kernel
ASM_FLAGS = -f elf32
CC_FLAGS = -m32 -ffreestanding -fno-pie -c
LD_FLAGS = -m elf_i386 -T link.ld

all: kernel

kasm.o: kernel.asm
	$(ASM) $(ASM_FLAGS) kernel.asm -o kasm.o

kc.o: kernel.c
	$(CC) $(CC_FLAGS) kernel.c -o kc.o

kernel: kasm.o kc.o
	$(LD) $(LD_FLAGS) -o kernel kasm.o kc.o

clean:
	rm -rf *.o kernel
run: kernel
	$(VM) kernel