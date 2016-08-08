KVERS =$(shell uname -r)
PWD = $(shell pwd)
obj-m +=scullc.o
build:
	make -C /lib/modules/$(KVERS)/build M=$(PWD) modules -lgcc -lm
clean:	
	make -C /lib/modules/$(KVERS)/build M=$(PWD) clean
