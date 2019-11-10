# V20_ROM_Dump
Teensy 3.5 Arduino sketch for dumping the memory of a small-scale NEC V20-based system,
such as the IBM Wheelwriter 1000 typewriter upon which the Cadetwriter is based.

For detailed design notes, see Cadetwriter_ROM_dump_notes.txt

Requires the Arduino IDE, Teensyduino, and the Arduino SdFat library, and a special
hardware adapter to connect the Teensy 3.5 to most of the pins of the NEC V20 in
the target system.

Dumps the entire address space of the V20 (RAM, ROM, and anything that happens to be
memory-mapped. This assumes there is no memory mapping going on in the target system.

User interaction is via the Teensy's USB serial terminal.

Outputs to a raw binary file (one megabyte exactly) on the SD card in the Teensy 3.5.
