KVERS =$(shell uname -r)
PWD = $(shell pwd)
obj-m +=hello_world.o
build:
	make -C /lib/modules/$(KVERS)/build M=$(PWD) modules
clean:	
	make -C /lib/modules/$(KVERS)/build M=$(PWD) clean
