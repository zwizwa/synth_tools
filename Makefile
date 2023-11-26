.PHONY: all clean host stm all_products

ALL ?= all_products

all: $(ALL)

-include rules.mk

all_products: $(ALL_PRODUCTS)

studio_elf: $(STUDIO_ELF)

clean:
	cd stm32f103 ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build *.hex *.fw.enc *.map
	cd tools ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build
	rm -f $(ALL_PRODUCTS)




