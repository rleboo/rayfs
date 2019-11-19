obj-m += rayfs.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
	gcc mkfs.c -g -o mkfs

do: all
	sudo ./mkfs /dev/mmcblk0p3
	sudo insmod rayfs.ko
	sudo mount -t rayfs /dev/mmcblk0p3 mount_pt/
	ls -lh mount_pt/
	cat mount_pt/readme.txt

undo:
	sudo umount /home/pi/Desktop/module/mount_pt/
	sudo rmmod rayfs