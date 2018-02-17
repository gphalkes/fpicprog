Introduction
============

fpicprog is a software driver for FTDI USB chips like the FT232RL to program
Microchip PIC™ chips. Contrary to other such software, fpicprog allows fast
programming and verification, by using the so-called Synchronous Bitbang mode
of the FT232RL and later chips. Programming several kilobytes of program data
takes seconds, where other FT232-based programming methods take minutes for
the same data.

USB-to-serial converters with the FT232RL are readily available over the
internet, often costing no more than a handful of dollars. These are typically
not in a housing, but instead provide a circuit board with a USB connector on
one end and a bare PCB header on the other. These are easily used with a
breadboard to connect the PCB header to the appropriate pins of the PIC chip.

Note that fpicprog is designed to work with chips which provide Low-Voltage
Programming, or Single-Supply programming. It is possible to use one of the
pins of the FT232 chip to drive a high voltage, and thereby also use fpicprog
for High-Voltage programming, but this requires extra circuitry which is not
described in this document.

Compiling fpicprog
==================

To compile fpicprog from the github repository, clone the gphalkes/fpicprog
and the gphalkes/makesys repositories in the same directory. Also make sure
you have the development package for libftdi (commonly called libftdi-dev or
libftdi-devel) and the google-gflags development package installed. Then
run `make -C fpicprog/src COMPILER=gcc` to build fpicprog. Note that if clang++
is installed, the COMPILER=gcc can be left out to build with clang++ instead of
g++.

Connecting the programmer
=========================

Each PIC chip has a different pin layout, and there are different versions of
Low-Voltage programming, thus wiring up the chip will require checking the
datasheet for the chip you wish to program. However, in general you will need
to at least make the following connections: VCC -> VDD, GND -> VSS,
TxD -> !MCLR, DTR -> PGC, and RxD -> PGD. If the PIC chip you wish to program
has a separate PGM pin, rather than activating the Low-Voltage Programming mode
through a handshake on the !MCLR pin, you need to connect CTS -> PGM as well.
(Note that this is for the default settings. You can instruct fpicprog to use
different pins instead, if that make wiring easier.)

Although not strictly necessary, it is recommended to connect the !MCLR pin to
VCC using a pull-up resistor. You may also want to add capacitors between the
VDD and VSS pins as recommended by Microchip, although I have been able to
reliably program chips without any of these additional components.

An example of wiring up a PIC18F45K50 is in the picture below.

![Example](https://github.com/gphalkes/fpicprog/raw/master/example.jpg)
