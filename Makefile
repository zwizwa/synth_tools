.PHONY: all clean all_products

all: all_products

-include rules.mk
all_products: $(ALL_PRODUCTS)

clean:
	cd stm32f103 ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build *.hex *.fw.enc *.map
	cd linux ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build
	rm -f $(ALL_PRODUCTS)
	rm -rf rs/target
	rm -f zig/*.o zig/*.a








