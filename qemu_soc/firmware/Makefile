CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

CFLAGS  := -mcpu=cortex-m3 -mthumb -O2 -ffreestanding -fno-builtin \
           -Wall -Wextra -I.
LDFLAGS := -T linker.ld -nostdlib -Wl,--gc-sections

OBJS := startup.o main.o

.PHONY: all clean

all: timer_fw.elf timer_fw.bin
	$(SIZE) timer_fw.elf

timer_fw.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

timer_fw.bin: timer_fw.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) timer_fw.elf timer_fw.bin
