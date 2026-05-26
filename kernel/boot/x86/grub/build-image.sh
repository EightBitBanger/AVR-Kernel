mkdir -p iso_root/boot/grub

cp ../../../../bin/x86-kernel.bin iso_root/boot/kernel.bin
cp grub.cfg iso_root/boot/grub/

grub-mkrescue -o boot-image.iso iso_root
