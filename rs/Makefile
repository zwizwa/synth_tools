.PHONY: all clean

all: test.elf
	./test.elf

RS_A:= ./target/release/libsynth_tools.a

$(RS_A): src/lib.rs
	cargo build --release
	find -name '*.a'

%.o: %.c
	gcc \
	-I../../uc_tools \
	-I../generic \
	-g -o $@ -c $<

test.elf: test.o $(RS_A)
	gcc -o test.elf test.o -L $$(dirname $(RS_A)) -lsynth_tools -Wl,--gc-sections

clean:
	rm -rf target

