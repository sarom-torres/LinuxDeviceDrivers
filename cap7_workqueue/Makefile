# KERNELRELEASE define a versão atual do kernel
# é possível usar outras como: VERSION, PATHLEVEL, SUBLEVEL E EXTRAVERSION
#
# Se KERNELRELEASE está definida então será chamado a partir do kernel build system
ifneq ($(KERNELRELEASE),)
	obj-m = scull_fifo.o
# senão será chamado direto pela linha de comando 
else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
