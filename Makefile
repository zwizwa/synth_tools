.PHONY: all
all: all_products

-include rules.mk
.PHONY: all_products
all_products: $(ALL_PRODUCTS)

.PHONY: signalsmith
signalsmith:
	make -C signalsmith

.PHONY: clean
clean:
	cd stm32f103 ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build *.hex *.fw.enc *.map
	cd linux ; rm -f *.o *.d *.a *.elf *.bin *.fw *.data *.build
	rm -f $(ALL_PRODUCTS)
	rm -rf rs/target
	rm -f zig/*.o zig/*.a

.PHONY: rs_linux
rs_linux:
	cd rs_linux ; cargo build

.PHONY: pd
pd:
	rm -f pd ; ln -sf $$PD pd ; ls -l pd

.PHONY: jack
jack:
	rm -f jack ; ln -sf $$JACK jack ; ls -l jack


# Keep this /etc/net build.  Does not depend on ARM tools.
studio_elf: $(STUDIO_ELF)

rs_a: $(RS_A_HOST)

