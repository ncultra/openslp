all: _slp.so slp.py

_slp.so: _slp.pyx
	python setup.py build

clean:
	rm -rf build _slp.so _slp.c *~

.PHONY: all clean
