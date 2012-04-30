all: clean build makejar


build:
	ant

clean:
	rm -rf bin
	
makejar:
	cd bin; jar -cf ../../bin/kremlin.jar .
