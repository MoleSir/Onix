BUILD := ../build
SRC := .

# 指定入口地址
MULTIBOOT2 := 0x10000
ENTRYPOINT := $(shell python -c "print(f'0x{$(MULTIBOOT2) + 64:x}')")

CFLAGS := -m32 # 32 位的程序
CFLAGS += -fno-builtin # 不需要 gcc 内置函数
CFLAGS += -nostdinc # 不需要标准头文件
CFLAGS += -fno-pic # 不需要位置无关的代码 	position independent code
CFLAGS += -fno-pie # 不需要位置无关的可执行程序 	position independent executable
CFLAGS += -nostdlib # 不需要标准库
CFLAGS += -fno-stack-protector # 不需要栈保护
CFLAGS := $(strip ${CFLAGS})

DEBUG := -g

INCLUDE := -I$(SRC)/include

$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@
	
$(BUILD)/%.o: $(SRC)/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

$(BUILD)/%.o: $(SRC)/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

LDFLAGS := -m elf_i386 \
	-static \
	-Ttext $(ENTRYPOINT) \
	--section-start=.multiboot2=$(MULTIBOOT2)
LDFLAGS := $(strip ${LDFLAGS})

$(BUILD)/kernel.bin: \
	$(BUILD)/kernel/start.o \
	$(BUILD)/kernel/main.o \
	$(BUILD)/kernel/io.o \
	$(BUILD)/kernel/console.o \
	$(BUILD)/kernel/printk.o \
	$(BUILD)/kernel/assert.o \
	$(BUILD)/kernel/debug.o \
	$(BUILD)/kernel/global.o \
	$(BUILD)/kernel/schedule.o \
	$(BUILD)/kernel/task.o \
	$(BUILD)/kernel/gate.o \
	$(BUILD)/kernel/interrupt.o \
	$(BUILD)/kernel/handler.o \
	$(BUILD)/kernel/clock.o \
	$(BUILD)/kernel/time.o \
	$(BUILD)/kernel/memory.o \
	$(BUILD)/kernel/rtc.o \
	$(BUILD)/kernel/syscall.o \
	$(BUILD)/kernel/mutex.o \
	$(BUILD)/kernel/thread.o \
	$(BUILD)/kernel/keyboard.o \
	$(BUILD)/kernel/arena.o \
	$(BUILD)/kernel/ide.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/printf.o \
	$(BUILD)/ds/bitmap.o \
	$(BUILD)/ds/list.o \
	$(BUILD)/ds/fifo.o \

	$(shell mkdir -p $(dir $@))
	ld ${LDFLAGS} $^ -o $@

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
	$(BUILD)/system.bin \
	$(BUILD)/system.map \
	$(SRC)/utils/master.sfdisk \

# 创建一个 16M 的硬盘镜像
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@

# 将 boot.bin 写入 master.img 磁盘的第 0 个 512 字节空间
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc

# 将 loader.bin 写入 master.img 磁盘的第 2 个 4 * 512 字节空间
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc

# 测试 system.bin 的大小
	test -n "$$(find $(BUILD)/system.bin -size -100k)"

# 将内核代码写入到磁盘的第 10 个扇起始的 200 个扇区
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc

# 分区
	sfdisk $@ < $(SRC)/utils/master.sfdisk

$(BUILD)/slave.img:

# 创建一个 32M 的硬盘镜像
	yes | bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@

IMAGES := $(BUILD)/master.img $(BUILD)/slave.img

image : $(IMAGES)

$(BUILD)/kernel.iso : $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg

# 检测内核文件是否合法
	grub-file --is-x86-multiboot2 $<
# 创建 iso 目录
	mkdir -p $(BUILD)/iso/boot/grub
# 拷贝内核文件
	cp $< $(BUILD)/iso/boot
# 拷贝 grub 配置文件
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
# 生成 iso 文件
	grub-mkrescue -o $@ $(BUILD)/iso


.PHONY: clean
clean:
	rm -rf $(BUILD)

.PHONY: bochs
bochs: $(IMAGES)
	bochs -q -f ../bochs/bochsrc -unlock

.PHONY: bochsg
bochsg: $(IMAGES)
	bochs-gdb -q -f ../bochs/bochsrc.gdb -unlock

.PHONY: bochsb
bochsb:	$(BUILD)/kernel.iso
	bochs -q -f ../bochs/bochsrc.grub -unlock

QEMU := qemu-system-i386
QEMU += -m 32M
QEMU += -audiodev pa,id=hda
QEMU += -machine pcspk-audiodev=hda
QEMU += -rtc base=localtime
QEMU += -drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw
QEMU += -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw

QEMU_DISK := -boot c

QEMU_CDROM := -boot d \
	-drive file=$(BUILD)/kernel.iso,media=cdrom \

QEMU_DEBUG := -s -S

.PHONY: qemu
qemu: $(IMAGES)
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug
qemug: $(IMAGES)
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

.PHONY: qemub
qemub: $(BUILD)/kernel.iso
	$(QEMU) $(QEMU_CDROM) \
	# $(QEMU_DEBUG)

$(BUILD)/master.vmdk: $(BUILD)/master.img
	qemu-img convert -O vmdk $< $@

.PHONY:vmdk
vmdk: $(BUILD)/master.vmdk