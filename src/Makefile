# Top level Makefile that would use the functionality of existing makefiles
# inside the lib and vfd directory. This file would build the libraries and the 
# vfd source. 
all:
	(cd lib; make jsmn && make libvfd.a)
	(cd vfd; make clean && make)
	(cd system; make clean && make)

clean:
	(cd vfd; make clean)
	(cd lib; make nuke)
	(cd system; make nuke)
	(cd lib/jsmn; make clean)
