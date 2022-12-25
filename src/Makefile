BUILD := ../build
SRC := .

$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin $(BUILD)/boot/loader.bin
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
# 将 boot.bin 写入 master.img 磁盘的第 0 个 512 字节空间
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
# 将 loader.bin 写入 master.img 磁盘的第 2 个 4 * 512 字节空间
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc


.PHONY: clean
clear:
	rm -rf $(BUILD)

.PHNOY: bochs
bochs:$(BUILD)/master.img
	bochs -q