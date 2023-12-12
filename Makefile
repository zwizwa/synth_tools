.PHONY: all clean host_elf all_products

ALL ?= host_elf # all_products

all: $(ALL)

-include rules.mk

all_products: $(ALL_PRODUCTS)

host_elf: $(HOST_ELF)

clean:
	cd stm32f103 ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build *.hex *.fw.enc *.map
	cd tools ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build
	rm -f $(ALL_PRODUCTS)
	rm -rf rs/target
	rm -f zig/*.o zig/*.a








