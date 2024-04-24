/* IR Decoder

  Code that decodes IR signals from the Adafruit IR Remote (389) using a TSOP38238.
 
  Adapted from Raw IR commander by Limor Fried, Adafruit Industries
*/

// Use the 'raw' pin reading methods because timing
// is very important here and the digitalRead()
// procedure is slower!
// uint8_t IRpin = 2;
// Digital pin #2 is the same as Pin D2 see
// http://arduino.cc/en/Hacking/PinMapping168 for the 'raw' pin mapping
#define IRpin_PIN      PIND
#define IRpin          2

// the maximum pulse we'll listen for - 65 milliseconds is a long time
#define MAXPULSE 65000
#define NUMPULSES 50

// what our timing resolution should be, larger is better
// as its more 'precise' - but too large and you wont get
// accurate timing
#define RESOLUTION 20 

// What percent we will allow in variation to match the same code
#define FUZZINESS 20

// we will store up to 100 pulse pairs (this is -a lot-)
uint16_t pulses[NUMPULSES][2];  // pair is high and low pulse 
uint8_t currentpulse = 0; // index for pulses we're storing

#include "ircommander.h"

void setup() {
  Serial.begin(9600);
  Serial.println("Ready to decode IR!");
}

void loop(void) {
  int numberpulses;
 
  numberpulses = listenForIR();
 
  Serial.print("Heard ");
  Serial.print(numberpulses);
  Serial.println("-pulse long IR signal");

  if (IRcompare(numberpulses, IRSetupSignal, SetupSize)) {
  Serial.println("SETUP SIG!!!!!!!!!");
  }
  if (IRcompare(numberpulses, IRStopSignal, StopSize)) {
  Serial.println("STOP SIG!!!!!!!!!!");
  }
  if (IRcompare(numberpulses, IRUpSignal, UpSize)) {
  Serial.println("UP SIG!!!!!!!!!!");
  }
  if (IRcompare(numberpulses, IRDownSignal, DownSize)) {
  Serial.println("DOWN SIG!!!!!!!!!!");
  }
}

// listen for IR signal
int listenForIR(void) {
  currentpulse = 0;
 
  while (1) {
    uint16_t highpulse, lowpulse;  // temporary storage timing
    highpulse = lowpulse = 0; // start out with no pulse length
 
  //  while (digitalRead(IRpin)) { // this is too slow!
    while (IRpin_PIN & (1 << IRpin)) {
 
       highpulse++;
       delayMicroseconds(RESOLUTION);
       if ((highpulse >= MAXPULSE) && (currentpulse != 0)) {
         return currentpulse;
       }
    }
    pulses[currentpulse][0] = highpulse;
 
    // same as above
    while (! (IRpin_PIN & _BV(IRpin))) {
       lowpulse++;
       delayMicroseconds(RESOLUTION);
       if ((lowpulse >= MAXPULSE)  && (currentpulse != 0)) {
         return currentpulse;
       }
    }
    pulses[currentpulse][1] = lowpulse;
    currentpulse++;
  }
}

// process IR signal
boolean IRcompare(int numpulses, int Signal[], int SignalSize[]) {
 
  for (int i=0; i< numpulses-2; i++) {
    int oncode = pulses[i][1] * RESOLUTION / 10;
    int offcode = pulses[i+1][0] * RESOLUTION / 10;
 
    // check to make sure the error is less than FUZZINESS percent
    if (abs(oncode - Signal[i*2 + 1]) <= (Signal[i*2 + 1] * FUZZINESS / 100)) {
    } else {
      return false;
    }
    
    if (abs(offcode - Signal[i*2 + 2]) <= (Signal[i*2 + 2] * FUZZINESS / 100) || i == 0) {
    } else {
      return false;
    }
  }
  if (SignalSize[0] <= numpulses && numpulses <= SignalSize[1]) {
    return true;
  } else {
    return false;
  }
}