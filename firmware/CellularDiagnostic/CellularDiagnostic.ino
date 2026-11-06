// Temporary USB-to-modem diagnostic. No SMS, calls, radio or synthetic readings.
// USB 115200; modem D10 RX / D11 TX at 9600. Send AT commands ending CR.
// A single ^ re-requests 9600 using the modem's possible 115200 boot baud.
#include <SoftwareSerial.h>
#include <avr/wdt.h>
SoftwareSerial modem(10, 11);
void disableWatchdog() __attribute__((naked, section(".init3")));
void disableWatchdog() { MCUSR = 0; wdt_disable(); }
void setup() {
  Serial.begin(115200);
  modem.begin(9600);
  Serial.println(F("FMS modem diagnostic: USB 115200 / modem 9600; ^ sets boot baud"));
}
char command[160];
uint8_t used = 0;
unsigned int rawRemaining = 0;
void loop() {
  while (modem.available()) Serial.write(modem.read());
  while (Serial.available()) {
    char c = Serial.read();
    if (rawRemaining) {
      modem.write(c);
      if (--rawRemaining == 0) Serial.println(F("RAW transfer complete"));
      continue;
    }
    if (c == '^') {
      modem.begin(115200);
      modem.print(F("AT+IPR=9600\r"));
      delay(250);
      modem.begin(9600);
      Serial.println(F("modem baud requested: 9600"));
    } else if (c == '\r' || c == '\n') {
      if (used) {
        command[used] = 0;
        if (strncmp(command, "@RAW ", 5) == 0) {
          rawRemaining = atoi(command + 5);
          Serial.println(F("RAW transfer ready"));
        } else modem.println(command);
        used = 0;
      }
    } else if (used < sizeof(command) - 1) command[used++] = c;
  }
}
