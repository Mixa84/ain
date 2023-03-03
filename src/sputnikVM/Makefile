CARGO ?= cargo
TARGET ?=

all:
	CRATE_CC_NO_DEFAULTS=1 $(CARGO) build --release $(if $(TARGET),--target $(TARGET),)
	mkdir -p include
	cp target/$(if $(TARGET),$(TARGET)/,)release/libevm.a  libevm.a
	cp target/libevm.h include/
	cp target/libevm.cpp libevm.cpp

clean:
	$(CARGO) clean
	rm -rf include
	rm libevm.a libevm.cpp
