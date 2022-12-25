BUILD := ../build
SRC := .

# 指定入口地址
ENTRYPOINT := 0x10000

$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@
	
$(BUILD)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $< -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel/start.o
	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static $^ -o $@ -Ttext $(ENTRYPOINT)

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
	$(BUILD)/system.bin \
	$(BUILD)/system.map \
	
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
# 将 boot.bin 写入 master.img 磁盘的第 0 个 512 字节空间
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
# 将 loader.bin 写入 master.img 磁盘的第 2 个 4 * 512 字节空间
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc
# 将内核代码写入到磁盘的第 10 个扇起始的 200 个扇区
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc

test: $(BUILD)/master.img

.PHONY: clean
clear:
	rm -rf $(BUILD)

.PHNOY: bochs
bochs:$(BUILD)/master.img
	bochs -q