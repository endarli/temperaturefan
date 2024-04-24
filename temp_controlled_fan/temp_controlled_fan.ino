#include "Arduino_SensorKit.h"
#include <Wire.h> // For the LCD
#include "rgb_lcd.h" // For the LCD
#include "ircommander.h"

// PINS
#define Environment Environment_I2C // For temperature sensor
#define relay_PIN 6                 // Switch the circuit on and off
#define IRpin_PIN      PIND
#define IRpin          2
const byte OC1A_PIN = 9;            //pin 9, speed PWM ctrl
const byte OC1B_PIN = 10;

// TACH/PWM VARS
volatile bool countRPMPhase = false;
volatile unsigned long count = 0;
unsigned long start_time;
bool countprint;
long rpm;
const word PWM_FREQ_HZ = 25000;     //Adjust this value to adjust the frequency
const word TCNT1_TOP = 16000000/(2*PWM_FREQ_HZ);

float temp_to_spd = 10;             // 80/8 (8 is the range of temps we're covering)

// SYTEM VARS
// setting true: temp , false: manual
bool setting = true;
bool status = false;
rgb_lcd lcd;                        // LCD "object"
float setTemp = 75;
float temperature = 70;
long setMan = 1;
char *setManS[] = {"Low", "Med", "High"};

// IR VARS
#define MAXPULSE 65000
#define NUMPULSES 50
#define RESOLUTION 20
#define FUZZINESS 20
uint16_t pulses[NUMPULSES][2];      // pair is high and low pulse 
uint8_t currentpulse = 0;           // index for pulses we're storing

void setup() {
  Serial.begin(9600);
  Environment.begin();              // Initialize temp sensor  
  lcd.begin(16, 2);                 // set up the LCD's number of columns and rows
  lcd.setCursor(0, 0);
  lcd.print("    FAN OFF     ");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  pinMode(relay_PIN, OUTPUT);
  pinMode(OC1A_PIN, OUTPUT); // For the speed control

  analogReference(EXTERNAL);

  // Clear Timer1 control and count registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // Set Timer1 configuration
  // COM1A(1:0) = 0b10   (Output A clear rising/set falling)
  // COM1B(1:0) = 0b00   (Output B normal operation)
  // WGM(13:10) = 0b1010 (Phase correct PWM)
  // ICNC1      = 0b0    (Input capture noise canceler disabled)
  // ICES1      = 0b0    (Input capture edge select disabled)
  // CS(12:10)  = 0b001  (Input clock select = clock/1)
  
  // Set the PWM control for a computer fan
  TCCR1A |= (1 << COM1A1) | (1 << WGM11);
  TCCR1B |= (1 << WGM13) | (1 << CS10);
  ICR1 = TCNT1_TOP;
}

// put your main code here, to run repeatedly:
void loop() {
  int numberpulses;
  numberpulses = listenForIR();

  // DHT20 and TMP36 
  Serial.print("Temperature = ");
  temperature = (Environment.readTemperature() * 9.0 / 5.0) + 32.0;

  if (IRcompare(numberpulses, IRStopSignal, StopSize)) {
    if (status) {
      // turn relay on
      digitalWrite(relay_PIN, LOW);
      status = !status;
    } else {
      // turn relay off
      digitalWrite(relay_PIN, HIGH);
      status = !status;
    }
  }

  if (status) {
    //read other IR signals and do stuff if fan is on
    if (IRcompare(numberpulses, IRSetupSignal, SetupSize)) {
      setting = !setting;
    }

    // turn speed up
    if (IRcompare(numberpulses, IRUpSignal, UpSize)) {
      if (setting == true) {
        // temp control
        if (setTemp < 90) {
          setTemp++;
        }
      } else {
        // manual control
        if (setMan < 2) {
          setMan++;
        }
      }
    }

    // turn speed down
    if (IRcompare(numberpulses, IRDownSignal, DownSize)) {
      if (setting == true) {
        // temp control
        if (60 < setTemp) {
          setTemp--;
        }
      } else {
        // manual control
        if (0 < setMan) {
          setMan--;
        }
      }
    }

    // temp control
    if (setting) {
        setPwmDuty(tempToDuty());
    } else {
      setPwmDuty(setMan*30 + 30);
    }
    
    // Print same values in LCD
    lcd.setCursor(0, 0);
    if (setting) {
      lcd.print("AT:");
      int temp = temperature;
      lcd.print(temp);
      lcd.print(" F, ");
      lcd.print("ST:");
      lcd.print(int(setTemp));
      lcd.print(" F     ");
    } else {
      lcd.print("Mode: ");
      lcd.print(setManS[setMan]);
      lcd.print("        ");
    }
    lcd.setCursor(0, 1);
    lcd.print("Speed: ");
    if(setting){
      lcd.print(tempToDuty());
    }else{
      lcd.print(setMan*30 + 30);
    }
    lcd.print(" %    ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("    FAN OFF     ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
  }
  irConsolePrint();
}

void irConsolePrint(){
  Serial.print("Temperature = ");
  Serial.print(temperature); //print actual temperature
  Serial.println(" F");

  Serial.print("Set Temp = ");
  Serial.print(int(setTemp)); //print set temperature
  Serial.println(" F");

  Serial.print("Mode: ");
  if (setting) {
    Serial.println("Temp");
  } else {
    Serial.println("Manual");
  }

  Serial.print("Status: ");
  if (status) {
    Serial.println("On");
  } else {
    Serial.println("Off");
  }
}

int tempToDuty() {
  int duty = 0;
  if ((temperature - setTemp) > 0) {
    duty = (temperature - setTemp) * temp_to_spd + 20;
  }
  return duty;
}
void setPwmDuty(byte duty) {
  OCR1A = (word) (duty*TCNT1_TOP)/100;
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
