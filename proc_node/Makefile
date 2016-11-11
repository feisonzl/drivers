KVERS =$(shell uname -r)
PWD = $(shell pwd)
obj-m +=proc_node.o
obj-m +=proc_seq.o
build:
	make -C /lib/modules/$(KVERS)/build M=$(PWD) modules
clean:	
	make -C /lib/modules/$(KVERS)/build M=$(PWD) clean
