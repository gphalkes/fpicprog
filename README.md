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
Make sure however, that the converter uses the appropriate voltage for your
PIC chip. There are versions at 5V and at 3.3V.

Note that fpicprog is designed to work with chips which provide Low-Voltage
Programming, or Single-Supply programming. It is possible to use one of the
pins of the FT232 chip to drive a high voltage, and thereby also use fpicprog
for High-Voltage programming, but this requires extra circuitry which is not
described in this document.

Supported chips
===============

fpicprog supports chips in the following families: PIC10, PIC12, PIC16, PIC18
and PIC24. Most chips supporting Low-Voltage Programming from these families
can simply be added to the device database by copying the relevant parameters
from the datasheet. See the documentation in man/fpicprod-devlist.5.txt and the
device\_db folder for more details.

Compiling fpicprog
==================
To compile fpicprog from the github repository, clone the gphalkes/fpicprog
inlcuding the makesys submodule. Also make sure that you have the development 
package for libftdi (commonly called libftdi1-dev or libftdi-devel) and the 
google-gflags development package installed. 

Then build fpicprog. Note that if clang++ is installed, the COMPILER=gcc can 
be left out to build with clang++ instead of g++.

```bash
git clone https://github.com/gphalkes/fpicprog
cd fpicprog/
git submodule update --init --recursive
cd ../
make -C fpicprog/src COMPILER=gcc
```

Connecting the programmer
=========================

Each PIC chip has a different pin layout, and there are different versions of
Low-Voltage programming, thus wiring up the chip will require checking the
datasheet for the chip you wish to program. However, in general you will need
to at least make the following connections:

| Programmer  | Target |
| :---------: | :----: |
| VCC         | VDD    |
| GND         | VSS    |
| TxD         | !MCLR  |
| DTR         | PGC    |
| RxD         | PGD    |

(Note that this is for the default settings. You can instruct fpicprog to use
different pins instead, if that makes wiring easier.)

Although not strictly necessary, it is recommended to connect the !MCLR pin to
VCC using a pull-up resistor. You may also want to add capacitors between the
VDD and VSS pins as recommended by Microchip, although I have been able to
reliably program chips without any of these additional components.

An example of wiring up a PIC18F45K50 is in the picture below. Note that the
order of the pins on the USB-to-serial converter is model dependent, so you will
have to check your specific board for the exact pin layout.

![Example](https://github.com/gphalkes/fpicprog/raw/master/example.jpg)

Chips using a PGM pin
---------------------

Some PIC chips use a PGM pin to enter programming mode, instead of a handshake
using the !MCLR pin. If this is the case, you need to connect CTS -> PGM as
well.

Split PGD pin operation
-----------------------

The default wiring can cause relatively high current to flow on the PGD pin, as
it will be driven from both the FTDI chip and the PIC chip at the same time.
Also, it has been found that using 3.3V for programming some of the F-series
PIC chips that should allow operation at 3.3V (not all can, check the datasheet
for the chip you are using), attempting to do so results in too low signal
voltages and reading invalid values.

To overcome both these issues, it is possible to split the PGD pin use in an
input pin and an output pin. To enable this, the --ftdi\_PGD\_in flag can be
used the specify a different pin to use for reading the PGD pin. In this case,
a series resistor of approximately 470 ohms must be connected in series with
the driving pin of the programmer module, i.e. the pin specified by the
--ftdi\_PGD flag.

An example of wiring up a PIC18F25K42 with the seperated PGD signal to a
FT4232H Mini Module on Channel A can be seen in
[this example](https://github.com/gphalkes/fpicprog/raw/master/FT4232H_Mini_Module_Example.jpg).
The example uses the RTS pin, which is not available on most FT232 modules.
However, if the PIC chip doesn't need have a PGM pin, the CTS pin on the FT232
module can be used. This required the following flag combination:
--ftdi\_PGM=NC --ftdi\_PGD\_in=CTS.
