KVERS =$(shell uname -r)
PWD = $(shell pwd)
obj-m +=scull1.o
build:
	make -C /lib/modules/$(KVERS)/build M=$(PWD) modules -lgcc -lm
clean:	
	make -C /lib/modules/$(KVERS)/build M=$(PWD) clean
