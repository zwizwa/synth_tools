.PHONY: all clean

all: test.elf
	./test.elf

LIB:=synth_tools_rs
RS_A:= ./target/release/lib$(LIB).a

$(RS_A): src/lib.rs
	cargo build --release
	find -name '*.a'

%.o: %.c
	gcc \
	-I../../uc_tools \
	-I../generic \
	-g -o $@ -c $<

# The --gc-sections is necessary to avoid linker errors.
test.elf: test.o $(RS_A)
	gcc -o test.elf test.o -L $$(dirname $(RS_A)) -l$(LIB) -Wl,--gc-sections

clean:
	rm -rf target

