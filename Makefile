CC=avr-gcc
CFLAGS=-g -Os -Wall -mcall-prologues -mmcu=attiny85 -DF_CPU=8000000UL -Wno-missing-braces
OBJ2HEX=avr-objcopy 
TARGET=gizmulp

program: $(TARGET).hex 
	sudo avrdude -p t85 -P usb -c avrispmkII -Uflash:w:$(TARGET).hex -B 1.0

fuses:
	sudo avrdude -p t85 -P usb -c avrispmkII -U lfuse:w:0xe2:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m -B 8

dispenser: fuses program
	./pump.sh

$(TARGET).hex: $(TARGET).obj
	$(OBJ2HEX) -R .eeprom -O ihex $< $@

$(TARGET).obj: $(TARGET).o  
	$(CC) $(CFLAGS) -o $@ -Wl,-Map,$(TARGET).map $(TARGET).o 

clean:
	rm -f *.hex *.obj *.o *.map
