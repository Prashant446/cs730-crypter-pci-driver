obj-m = cryptocard_mod.o
cryptocard_mod-objs+= cryptocard.o chardev.o core.o

KVERSION = $(shell uname -r)

all:
	cp cryptocard_user.h ../include
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
