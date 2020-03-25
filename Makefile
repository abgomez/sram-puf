main: GetStrongBit.cpp PUF/sram.cpp
	gcc -o getStrongBit GetStrongBit.cpp PUF/sram.cpp -I. -l bcm2835