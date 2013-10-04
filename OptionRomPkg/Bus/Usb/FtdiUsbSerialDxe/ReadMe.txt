
=== FTDI USB SERIAL OVERVIEW ===

This is a bus driver that enables the EfiSerialIoProtocol interface
for FTDI8U232AM based USB-to-Serial adapters.

=== STATUS ===

Serial Input: Functional on real hardware.
Serial Output: Functional on real hardware.

Operating Modes: Currently the user is able to change all operating modes
except timeout, FIFO depth and certain Data Bits, Baudrate and Flow Control values.
The default operating mode is:
	Baudrate:     115200
	Parity:       None
	Flow Control: None
	Data Bits:    8
	Stop Bits:    1
Currently Unsupported Values:
        Baudrate: Anything other than 115200
	Flow Control: RTS/CTS and DSR/DTR (these are not supported by the FTDI device)
	Data Bits: Anything other than 8
Note: 
	Data Bits setting of 6,7,8 can not be combined with a Stop Bits setting of 1.5

=== COMPATIBILITY ===

Tested with:
An FTDI8U232AM based USB-To-Serial adapter and the UEFI Shell 
using a PuTTY Terminal
