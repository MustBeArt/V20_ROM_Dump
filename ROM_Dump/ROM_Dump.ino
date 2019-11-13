/*****************************************************************************
 * 
 *  Dump the ROM (and RAM) from a Cadetwriter (IBM Wheelwriter 1000)
 *  using a Teensy 3.5 connected in parallel with the V20 on the
 *  typewriter's logic board.
 * 
 *  Paul Williamson KB5MU  --  paul@mustbeart.com
 *  License: GPL 3
 *
 *  Build environment:
 *  Arduino IDE 1.8.10 (https://www.arduino.cc/en/main/software).
 *  Teensyduino 1.48 (https://www.pjrc.com/teensy/td_download.html).
 *  Compile options - Teensy 3.5, USB Serial, 120 MHz, Fastest with LTO.
 * 
 *  This sketch interacts with the user over the USB serial port.
 *  For most errors, it reports the error and then freezes. The user
 *  will have to restart the program (probably by power cycling the
 *  whole system).
 * 
 *  After a successful initialization, the sketch prompts and waits
 *  for the user to hit Enter to initiate a dump of the V20's entire
 *  memory space to a file on the SD card. Once that is done, it
 *  reports success and goes back to prompting the user to hit Enter.
 * 
 *  The user is free to do as many memory dumps as required, within
 *  two limitations: the free space on the SD card and the limit of
 *  1000 files imposed by the file naming convention: CWMEMxxx.BIN
 *  where xxx is a decimal number from 000 to 999.
 * 
 *  Hardware:
 *    GPIO pins on the Teensy 3.5 (which has 5V tolerant pins) are
 *    connected in parallel with most of the pins on the NEC V20
 *    microprocessor on the Wheelwriter's logic board, typically
 *    using a DIP clip. Wire according to this table:
 * 
 * Signal Name    V20 pin   Teensy pin
 *  A14             2         0
 *  A13             3         1
 *  A12             4         2
 *  A11             5         3
 *  A10             6         4
 *  A9              7         5
 *  A8              8         6
 *  A7              9         7
 *  A6             10         8
 *  A5             11         9
 *  A4             12        10
 *  A3             13        11
 *  A2             14        12
 *  A1             15        24
 *  A0             16        25
 *  unused                 26-32
 *  GND            20       GND
 *
 *  ASTB           25        37
 *  BUFEN/         26        38
 *  BUFR/W         27        39
 *  IO/M/          28        14
 *  WR/            29        15
 *  HLDAK          30        16
 *  HLDRQ          31        17
 *  RD/            32        18
 *
 *  A19            35        19
 *  A18            36        20
 *  A17            37        21
 *  A16            38        22
 *  A15            39        23
 *  VDD            40       Vin
 *
 *  unused                 33-36
 *
 *  Note that we've skipped Teensy pin 13, which is used for the built-in LED
 * 
 * 
 *  Operating steps:
 *
 *  Wire up the Teensy to the DIP clip
 *  Insert a formatted SD card into the Teensy
 *  Turn off the Cadetwriter
 *  Open up the Cadetwriter to reveal the logic board (see adaptation guide)
 *  Clip the DIP clip onto the V20 chip on the logic board
 *  Turn on the Cadetwriter
 *  Connect the Teensy to a host computer via USB
 *  Program the Teensy with this sketch (if not already programmed)
 *  Run the serial monitor or a terminal program to interact with Teensy
 *  If interested in RAM, operate the Cadetwriter to get into the state of interest
 *  Hit Enter on the host computer to initiate a dump
 *  Wait for a completion message on the serial monitor
 *  Repeat last X steps if more captures are desired
 *  Turn off the Cadetwriter
 *  Remove the SD card from the Teensy
 *  Copy the resulting files off the SD card
 * 
 *  Notes:
 * 
 *  This procedure is not dependent upon the serial interface board
 *  that turns a stock Wheelwriter 1000 into a Cadetwriter. It will work on
 *  an unmodified Wheelwriter 1000, or on a Cadetwriter, or a Cadetwriter
 *  from which the Teensy 3.5 has been removed (so you can re-use the same
 *  Teensy for this operation if you don't have a spare handy).
 *  
 *  In fact, it should work on just about any small-scale V20-based system.
 *  The wiring table depends on the V20 being in a DIP package and the
 *  BUSRQ signal not being actively driven or tied to ground.
 * 
 *  Because this procedure dumps the RAM as well as the ROM, it can be
 *  useful for understanding the stored variables in RAM in particular
 *  operational states. Depending on what state you're trying to grok,
 *  you might need to have the Cadetwriter serial interface board
 *  connected and populated with its own Teensy 3.5.
 *
 *********************************************************************/

#include "SdFat.h"

// Values for BUSRQ (bus hold request)
#define BUS_HOLD    HIGH
#define BUS_RELEASE LOW

// Values for BUSAK (bus hold acknowledge)
#define BUS_ACK     HIGH
#define BUS_NAK     LOW

#define ADDRESS_BUS_WIDTH   20
#define DATA_BUS_WIDTH       8

// Teensy pin definitions
#define ASTB      37
#define BUFEN     38
#define BUFRW     39
#define IOMEM     14
#define WR        15
#define HLDAK     16
#define HLDRQ     17
#define RD        18

// Teensy pin mapping of address/data bus starting from the LSB
int bus_pins[] = { 25,24,12,11,10,9,8,7,6,5,4,3,2,1,0,23,22,21,20,19 };

SdFatSdioEX sd;
File dumpfile;

// Keep track of the next filename to be used. At setup, we start at 000 and
// skip over any files that already exist. Each time we complete a file, we
// move on to the next filename.
int file_number = 0;
char filename[] = "CWMEM000.BIN";    // filename will be incremented in place by next_file()

// Go the next file number and name in sequence
void next_file(void) {
  file_number++;
  if (file_number > 999) {
    Serial.println("Too many files for our naming convention. Clean off the SD card!");
    while (1);  // panic
  }
  sprintf(filename, "CWMEM%03d.BIN", file_number);
}


// Set the entire address bus to tristate (undriven)
void tristate_address_pins(void) {
  for (int i=0; i < ADDRESS_BUS_WIDTH; i++) {
    pinMode(bus_pins[i], INPUT);
  }
}


// Drive the data bus with a specified value
void drive_address(unsigned long addr) {
  for (int i=0; i < 20; i++) {
    pinMode(bus_pins[i], OUTPUT);
    digitalWrite(bus_pins[i], !!(addr & (1UL << i)));
  }
}


// Set the data portion of the bus to tristate (undriven)
void tristate_data_pins(void) {
  for (int i=0; i < DATA_BUS_WIDTH; i++) {
    pinMode(bus_pins[i], INPUT);
  }
}


// Return the 8-bit value on the data bus
// Of course, this returns garbage if no memory device is driving the bus.
byte read_data(void) {
  byte val = 0;

  for (int i=0; i < 8; i++) {
    if (digitalRead(bus_pins[i])) {
      val |= (1 << i);
    }
  }
  
  return val;
}


// Set all the bus control signals to driven with the idle values
void bus_state_driven_idle(void) {  
  digitalWrite(BUFEN, HIGH);    pinMode(BUFEN, OUTPUT);
  digitalWrite(BUFRW, LOW );    pinMode(BUFRW, OUTPUT);
  digitalWrite(ASTB,  LOW );    pinMode(ASTB, OUTPUT);
  digitalWrite(RD,    HIGH);    pinMode(RD, OUTPUT);
  digitalWrite(WR,    HIGH);    pinMode(WR, OUTPUT);
  digitalWrite(IOMEM, LOW );    pinMode(IOMEM, OUTPUT);
}


// Set all the bus control signals and the address bus to tristate (undriven)
void bus_state_not_driven(void) {
  pinMode(BUFEN, INPUT);
  pinMode(BUFRW, INPUT);
  pinMode(ASTB, INPUT);
  pinMode(RD, INPUT);
  pinMode(WR, INPUT);
  pinMode(IOMEM, INPUT);
  tristate_address_pins();
}


// Perform a single read cycle on the bus and return the result.
// This implementation runs at Teensy bus speed, except for a
// single delay to allow for the memory device's access time.
// That's not a realistic V20 bus cycle, though, so more delays
// might need to be inserted to make the timing more realistic.
byte bus_read_cycle(unsigned long address) {
  byte value;

  drive_address(address);
  digitalWrite(ASTB, HIGH);   // ASTB pulses high
  digitalWrite(ASTB, LOW);    // external address latch on this edge
  tristate_data_pins();       // free up the data bus for the memory read
  digitalWrite(RD, LOW);      // start reading the memory device
  digitalWrite(BUFEN, LOW);   // enable any external data buffers
  delayMicroseconds(1);        // wait for the slow memory devices
  value = read_data();        // get the result
  digitalWrite(BUFEN, HIGH);  // return the bus to idle state
  digitalWrite(RD, HIGH);

  return value;
}


// Standard Arduino setup routine, runs once on powerup
void setup() {

  delay(1000);              // This seems necessary, but why?
  
  Serial.begin(9600);       // We will interact with the user on the USB serial port
  Serial.println("Cadetwriter ROM dumper 0.2");
  
  pinMode(HLDRQ, OUTPUT);   // We will take permanent charge of the bus hold request line
  digitalWrite(HLDRQ, BUS_RELEASE);     // Let the V20 keep the bus for now

  pinMode(HLDAK, INPUT);    // We will need to check that the V20 gives up the bus
  
  pinMode(LED_BUILTIN, OUTPUT); // We'll also be lighting up the Teensy's LED

  // Set up the SD card to accept our dumped data
  if (!sd.begin()) {
    Serial.println("SD card not working!");
    sd.initErrorHalt("SdFatSdioEX begin() failed");
  }

  // Find the next filename in order that isn't already used
  while (sd.exists(filename)) {
    next_file();    // skip over all the existing files
  }

  // Let the user know if there were already files on the card
  if (file_number == 1) {
    Serial.println("One dump file already exists on this SD card.");
  } else if (file_number > 1) {
    Serial.print(file_number);
    Serial.println(" dump files already exist on this SD card.");
  }

}


// Standard Arduino loop function. Called repeatedly, forever.
void loop() {
  File dumpfile;

  Serial.println("\n\nHit Enter to dump the entire address space to a file.");
  while (!Serial.available() || Serial.read() != '\n');

  Serial.print("Seizing the bus ... ");
  digitalWrite(HLDRQ, BUS_HOLD);
  while (digitalRead(HLDAK) != BUS_ACK);
  Serial.println("ok");

  digitalWrite(LED_BUILTIN, HIGH);    // a little feedback on the hardware

  bus_state_driven_idle();  // take over the control signals on the bus

  // Create a new empty data dump file on the SD card
  dumpfile = sd.open(filename, FILE_WRITE);
  if (!dumpfile) {
    Serial.print("Failed to open ");
    Serial.println(filename);
    while (1);      // hang forever, don't try to recover.
  }

  Serial.println("Dumping ...");

  // Do the actual data dump, writing the results in raw binary to the file
  for (unsigned long addr=0; addr <= 0xFFFFFul; addr++) {
    dumpfile.write(bus_read_cycle(addr));   // ignoring any errors here.
  }

  // Try to close out the dump file, checking for errors.
  if (!dumpfile.close()) {
    Serial.print("Failed to close ");
    Serial.println(filename);
    // don't panic. Maybe it's safe to try again?
  }

  // Release the bus to the V20
  Serial.print("Releasing the bus ... ");
  digitalWrite(HLDRQ, BUS_RELEASE);
  while (digitalRead(HLDAK) == BUS_ACK);
  Serial.println("ok");

  digitalWrite(LED_BUILTIN, LOW);    // a little feedback on the hardware

  // Inform the user of completion and provide the filename
  Serial.print("Memory dumped to file ");
  Serial.println(filename);

  next_file();
  // Loop back and prompt the user for another dump
}
