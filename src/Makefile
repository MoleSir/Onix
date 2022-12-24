%.bin: %.asm
	nasm -f bin $< -o $@

master.img: boot.bin loader.bin
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat master.img
# 将 boot.bin 写入 master.img 磁盘的第 0 个 512 字节空间
	dd if=boot.bin of=master.img bs=512 count=1 conv=notrunc
# 将 loader.bin 写入 master.img 磁盘的第 2 个 4 * 512 字节空间
	dd if=loader.bin of=master.img bs=512 count=4 seek=2 conv=notrunc


.PHONY: clean
clear:
	rm -rf *.bin
	rm -rf *.img


.PHNOY: bochs
bochs: master.img
	bochs -q