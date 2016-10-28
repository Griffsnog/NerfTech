// NerfCommp RapidStrike control code

// Do not remove the include below
#include "NerfComp_RapidStrike.h"
#include <SPI.h>
#include <Wire.h>
#include "libraries\Adafruit_GFX.h"
#include "libraries\Adafruit_SSD1306.h"
#include <Servo.h>
#include <avr/pgmspace.h>

////////////////////////////////////////////////////////////////////////////////

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#include "SevenSegmentBitmaps.h"
#include "NerfLogo.h"

#define PIN_BARREL_START            2
#define PIN_BARREL_END              3
#define PIN_MAGSWITCH               4
#define PIN_REVTRIGGER              5
#define PIN_JAMDOOR                 6
#define PIN_TRIGGER                 12
#define PIN_PLUNGER_END             10
#define PIN_VOLTAGE_BATTERY         A7
#define PIN_PLUNGER_MOTOR_FWD       7
#define PIN_PLUNGER_MOTOR_REV       8
#define PIN_PLUNGER_MOTOR_PWM       11

#define VOLTAGE_BATTERY_SCALER      (1/65.8)
#define VOLTAGE_BATTERY_IIR_GAIN    0.01

#define MOTOR_DIR_FWD               0
#define MOTOR_DIR_REV               1
#define MOTOR_DIR_BRAKE             2
#define MOTOR_DIR_OFF               3

#define PIN_FLYWHEEL_MOTOR_ESC      9

#define PLUNGER_STATE_IDLE              0
#define PLUNGER_STATE_FLYWHEEL_REVUP    1
#define PLUNGER_STATE_CLEAR_END_SWITCH  2
#define PLUNGER_STATE_RUN_PLUNGER       3
#define PLUNGER_STATE_ROUND_DELAY       4
#define PLUNGER_STATE_WAIT_FOR_TRIGGGER_RELEASE 5

//#define PLUNGER_PWM_RUN_SPEED           145
#define PLUNGER_PWM_RUN_SPEED           120
#define PLUNGER_PWM_MAX                 255

#define TRIGGER_DELAY_TIME              10
#define ROUND_DELAY_TIME                10
#define FLYWHEEL_REVUP_TIME_SEMI        500
#define FLYWHEEL_REVUP_TIME_FULL        300
#define FLYWHEEL_REVDOWN_TIME_FULL      2000

#define FLYWHEEL_MOTOR_ESC_NEUTRAL      90
#define FLYWHEEL_MOTOR_ESC_PRE_RUN_FULL 96
#define FLYWHEEL_MOTOR_ESC_REVUP        160
#define FLYWHEEL_MOTOR_ESC_RUN          140
#define FLYWHEEL_MOTOR_ESC_BRAKE        15

#define VELOCITY_FPS_MIN                20.0
#define VELOCITY_FPS_MAX                400.0
//#define BARREL_LENGTH_CM                117
#define BARREL_LENGTH_INCHES            4.6063

#define HEARTBEAT_UPDATE_PERIOD         50
#define HEARTBEAT_PRINT_PERIOD          250

#define VOLTAGE_MIN         0.0
#define VOLTAGE_MAX         15.0

#define MAGTYPE_EMPTY     0
#define MAGTYPE_CLIP_6    1
#define MAGTYPE_CLIP_10   4
#define MAGTYPE_CLIP_12   2
#define MAGTYPE_CLIP_15   3
#define MAGTYPE_CLIP_18   8
#define MAGTYPE_DRUM_18   9
#define MAGTYPE_DRUM_25   10
#define MAGTYPE_DRUM_35   12
#define MAGTYPE_UNKNOWN   16

#define PIN_MAGTYPE_BIT0       A0
#define PIN_MAGTYPE_BIT1       A1
#define PIN_MAGTYPE_BIT2       A2
#define PIN_MAGTYPE_BIT3       A3

#define MAGAZINE_TYPE_DELAY     3

typedef struct MagazineType {
  const uint8_t code;
  const char* name;
  const uint8_t capacity;
} MagazineType;

MagazineType const magazineTypes[] = {
    {MAGTYPE_EMPTY,   "----", -1},
    {MAGTYPE_CLIP_6,  "Clip6", 6},
    {MAGTYPE_CLIP_10, "Clip10", 10},
    {MAGTYPE_CLIP_12, "Clip12", 12},
    {MAGTYPE_CLIP_15, "Clip15", 15},
    {MAGTYPE_CLIP_18, "Clip18", 18 },
    {MAGTYPE_DRUM_18, "Drum18", 18},
    {MAGTYPE_DRUM_25, "Drum25", 25 },
    {MAGTYPE_DRUM_35, "Drum35", 35 },
    {MAGTYPE_UNKNOWN, "????", 10 }
};


#define DISPLAY_UPDATE_PERIOD 100

#define SET_PLUNGER_STATE(s)  plungerState = s;plungerStateTime = millis();sp=true
#define STATE_DELAY(t)      ((millis() - plungerStateTime) > t)


// global variables for main system stats
int magazineType = MAGTYPE_EMPTY;
MagazineType* magazineTypePtr = (MagazineType*)&magazineTypes[MAGTYPE_EMPTY];
int roundCount = -1;
int roundsPerMin = -1;
int roundsJamCount = 0;
float velocity = -1;
float voltageBattery = 0.0;
float voltageBatteryAvg = 0.0;
boolean jamDoorOpen = false;

volatile unsigned long timeBarrelStart = 0;
volatile boolean timeBarrelStartFlag = false;

volatile unsigned long timeBarrelEnd = 0;
volatile boolean timeBarrelEndFlag = false;

void displayUpdate(void);

unsigned long heartbeatUpdateTime = 0;
unsigned long heartbeatPrintTime = 0;

// create servo object to control the flywheel ESC
Servo servoESC;

boolean magazineSwitchRead() {
  return !digitalRead(PIN_MAGSWITCH);
}

boolean revTriggerRead() {
  return !digitalRead(PIN_REVTRIGGER);
}

boolean jamDoorRead() {
  return !digitalRead(PIN_JAMDOOR);
}

boolean triggerRead() {
  return !digitalRead(PIN_TRIGGER);
}

boolean plungerEndRead() {
  return !digitalRead(PIN_PLUNGER_END);
}

MagazineType* magazineTypeLookup(int magTypeVal) {
  uint8_t typeIdx = 0;
  const MagazineType* tempPtr;

  do {
    tempPtr = &magazineTypes[typeIdx];
    if (tempPtr->code == magTypeVal) {
      // found the correct magazine type.  break and return.
      break;
    }
    typeIdx++;
  } while (tempPtr->code != MAGTYPE_UNKNOWN);
  return (MagazineType*)tempPtr;
}

#define GLITCH_CHECKS 4

boolean glitchReject(int pin) {
  boolean returnVal = true;
  volatile int i;

  for (i = 0; i < GLITCH_CHECKS; i++) {
    if (digitalRead(pin) == LOW) {
      // Pin is low again.  this was a glitch.  return false.
      returnVal = false;
      break;
    } else {
      // want ~500ns delay here, around 10 clock ticks
      i++;
      i--;
      i++;
      i--;
    }
  }
  return returnVal;
}

void irqBarrelStart() {
  // the dart crossed the first (chamber) barrel sensor
  if (glitchReject(PIN_BARREL_START)) {
    // good signal.  note the start time
    timeBarrelStart = micros();
  }
}

void irqBarrelEnd() {
  // the dart crossed the last (muzzle) barrel sensor
  if (glitchReject(PIN_BARREL_END)) {
    // good signal.  note the end time, and set the display update flag
    timeBarrelEnd = micros();
    timeBarrelEndFlag = true;
  }
}

int magTypeRead() {
  int magType = 0;
  if (!digitalRead(PIN_MAGTYPE_BIT0)) {magType |= 1;}
  if (!digitalRead(PIN_MAGTYPE_BIT1)) {magType |= 2;}
  if (!digitalRead(PIN_MAGTYPE_BIT2)) {magType |= 4;}
  if (!digitalRead(PIN_MAGTYPE_BIT3)) {magType |= 8;}
  return magType;
}


void plungerMotorPWM(int dir, int pwm) {
  int dirFwd, dirRev;

  
  switch (dir) {
  case MOTOR_DIR_FWD: {dirFwd = HIGH; dirRev = LOW;} break;
  case MOTOR_DIR_REV: {dirFwd = LOW; dirRev = HIGH;} break;
  case MOTOR_DIR_BRAKE: {dirFwd = HIGH; dirRev = HIGH; } break;
  case MOTOR_DIR_OFF: default: {dirFwd = LOW; dirRev = LOW; } break;
  }

  digitalWrite(PIN_PLUNGER_MOTOR_FWD, dirFwd);
  digitalWrite(PIN_PLUNGER_MOTOR_REV, dirRev);
  analogWrite(PIN_PLUNGER_MOTOR_PWM, pwm);
}


void plungerMotorInit(void) {
  pinMode(PIN_PLUNGER_MOTOR_FWD, OUTPUT);
  pinMode(PIN_PLUNGER_MOTOR_REV, OUTPUT);
  pinMode(PIN_PLUNGER_MOTOR_PWM, OUTPUT);
  plungerMotorPWM(MOTOR_DIR_OFF, 0);
}


void setup() {
  Serial.begin(115200);
  Serial.println(" NerfComp: RapidStrike CS-18 ver 0.5");

  // Setup the pins and interrupts for the barrel photo interrupters
  pinMode(PIN_BARREL_START, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_BARREL_START), irqBarrelStart, RISING);

  pinMode(PIN_BARREL_END, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_BARREL_END), irqBarrelEnd, RISING);

  // Setup the pins for magazine type sensors and jam door sensor
  pinMode(PIN_MAGTYPE_BIT0, INPUT);
  pinMode(PIN_MAGTYPE_BIT1, INPUT);
  pinMode(PIN_MAGTYPE_BIT2, INPUT);
  pinMode(PIN_MAGTYPE_BIT3, INPUT);

  
  pinMode(PIN_MAGSWITCH, INPUT_PULLUP);
  pinMode(PIN_REVTRIGGER, INPUT_PULLUP);
  pinMode(PIN_JAMDOOR, INPUT_PULLUP);

  
  pinMode(PIN_TRIGGER, INPUT_PULLUP);
  pinMode(PIN_PLUNGER_END, INPUT_PULLUP);

  plungerMotorInit();

  // init the servo
  servoESC.attach(PIN_FLYWHEEL_MOTOR_ESC); // attaches the servo on pin 9 to the servo object
  servoESC.write(FLYWHEEL_MOTOR_ESC_NEUTRAL);

  // init the LED Display
  // generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C

  // Clear the display buffer and put up the splash screen
  display.clearDisplay();
  display.drawBitmap(0, 0, NerfLogoBitmap, NERF_LOGO_BITMAP_WIDTH, NERF_LOGO_BITMAP_HEIGHT, 1);
  display.display();


  // show the screen for a while
  delay(2000);

  // force an update to show  the initial data display
  displayUpdate();

  // reset the heartbeat time to avoid a bunch of initial updates
  heartbeatUpdateTime = millis();
  heartbeatPrintTime = heartbeatUpdateTime;
}


int plungerState = PLUNGER_STATE_IDLE;
unsigned long plungerStateTime;
unsigned long displayUpdateTime = 0;

boolean sp = true;

void loop() {
  static int magazineTypeCounter;
  static boolean magazineNew = false;
  static unsigned long roundTimePrev = 0;

  boolean displayUpdateEnable = true;
  boolean displayUpdateForce = false;

  // look for a dart at the end of the barrel
  if (timeBarrelEndFlag) {
    timeBarrelEndFlag = false;
    if (roundCount > 0) {
      Serial.print("round="); Serial.print(roundCount, DEC);
      roundCount--;
      // compute time of flight in microseconds
      unsigned long time = timeBarrelEnd - timeBarrelStart;
      Serial.print(" time="); Serial.print(time, DEC);

      // compute velocity in feet/sec. apply some sanity checks
      if (time > 0) {
        float velocityTemp = (BARREL_LENGTH_INCHES * 1000000) / (12 * (float) time);
        Serial.print(" vel="); Serial.print(velocityTemp, 1);
        if ((velocityTemp < VELOCITY_FPS_MIN) || (velocityTemp > VELOCITY_FPS_MAX)) {
          // velocity error
          velocity = -1.0;
        } else {
          velocity = velocityTemp;
        }
      } else {
        // time <= 0.  time error => velocity error.
        velocity = -1.0;
      }
      // compute rounds per minute for bursts
      unsigned long roundTime = millis();
      if (roundTimePrev != 0) {
        unsigned long roundTimeDelta = roundTime - roundTimePrev;
        if (roundTimeDelta > 0) {
          // rounds per minute =
          unsigned long rpmTemp = 60000ul / roundTimeDelta;
          Serial.print(" delta="); Serial.print(roundTimeDelta, DEC);
          Serial.print(",rpm="); Serial.print(rpmTemp, DEC);
          if (rpmTemp > 60) {
            roundsPerMin = (int) rpmTemp;
          } else {
            // too slow a rate of fire to matter.  single shot
            // clear the rd/min display
            roundsPerMin = -1;
          }
        }
      }
      roundTimePrev = roundTime;
      Serial.println("");
    }
  }


  // read the motor voltage every 100ms
  boolean p = false;
  if (millis() > heartbeatUpdateTime) {
    heartbeatUpdateTime += HEARTBEAT_UPDATE_PERIOD;
    if (millis() > heartbeatPrintTime) {
      heartbeatPrintTime += HEARTBEAT_PRINT_PERIOD;
      p = true;
    }
    p = false;
    if (p) {Serial.print("hb ");}
    if (p) {Serial.print("  rounds="); Serial.print(roundCount, DEC); }


    // update the motor voltage
    int voltageMotorRaw = analogRead(PIN_VOLTAGE_BATTERY);
    float voltageMotorTemp = (float) voltageMotorRaw * VOLTAGE_BATTERY_SCALER;

    if (p) {Serial.print("  vmot="); Serial.print(voltageMotorRaw, DEC); }
    if (p) {Serial.print(","); Serial.print(voltageMotorTemp, 2); }
    // add some sanity checks to the voltage
    if ((voltageMotorTemp >= VOLTAGE_MIN) && (voltageMotorTemp <= VOLTAGE_MAX)) {
      // compute a IIR low-pass filter
      voltageBatteryAvg = VOLTAGE_BATTERY_IIR_GAIN * voltageMotorTemp + (1 - VOLTAGE_BATTERY_IIR_GAIN) * voltageBattery;
      voltageBattery = voltageMotorTemp;
    }



    // check the magazine type
    int magTypeTemp = magTypeRead();
    if (p) {Serial.print("  mag="); Serial.print(magTypeTemp, DEC); }
    if (magazineType != magTypeTemp) {
      magazineTypeCounter = 0;
      magazineNew = true;
    } else {
      if (magazineTypeCounter < MAGAZINE_TYPE_DELAY) {
        magazineTypeCounter++;
      } else {
        if (magazineNew) {
          magazineTypePtr = magazineTypeLookup(magTypeTemp);
          roundCount = magazineTypePtr->capacity;
          roundsJamCount = 0;
          magazineNew = false;
          Serial.println(""); Serial.print("(new magazine ");
          Serial.print(magazineTypePtr->name); Serial.println(")");
        }
      }
    }
    magazineType = magTypeTemp;
    if (p) {Serial.print(","); Serial.print(magazineTypePtr->name);}


    // check the jam door
    if ((magazineTypePtr->code != MAGTYPE_EMPTY) && (magazineTypeCounter == MAGAZINE_TYPE_DELAY)) {
      boolean jamDoorOpenTemp = jamDoorRead();
      if (p) {Serial.print(" jamdoor="); Serial.print(jamDoorOpenTemp);}
      if (jamDoorOpen != jamDoorOpenTemp) {
        if (jamDoorOpenTemp) {
          // jam door has gone from closed to open.  inc the jam count
          roundsJamCount++;
        } else {
          // jam door has gone from open to closed.  decrement a round
          if (roundCount > 0) {
            roundCount--;
          }
        }
        jamDoorOpen = jamDoorOpenTemp;
      }
    } else {
      jamDoorOpen = false;
    }


//    if (p) {Serial.print(" mag="); Serial.print(magazineSwitchRead());}
//    if (p) {Serial.print(" revTrig="); Serial.print(revTriggerRead());}
//    if (p) {Serial.print(" jam="); Serial.print(jamDoorRead());}
//    if (p) {Serial.print(" trig="); Serial.print(triggerRead());}
//    if (p) {Serial.print(" plunger="); Serial.print(plungerEndRead());}
  }

  //if (magazineSwitchRead() && jamDoorRead()) {
  // Safety switches are ok.  process the rev and fire triggers

  int ESCPos = FLYWHEEL_MOTOR_ESC_NEUTRAL;
  // for semi-auto mode, when the trigger is pulled, rev the flywheel, then power the trigger
  switch (plungerState) {
  case PLUNGER_STATE_IDLE: {
    if (sp) {Serial.println(""); Serial.println(" s:idle"); sp = false; }
    plungerMotorPWM(MOTOR_DIR_BRAKE, PLUNGER_PWM_MAX);
    if (revTriggerRead()) {
      ESCPos = FLYWHEEL_MOTOR_ESC_PRE_RUN_FULL;
    } else {
      ESCPos = FLYWHEEL_MOTOR_ESC_BRAKE;
    }
    if (triggerRead() && STATE_DELAY(TRIGGER_DELAY_TIME)) {
      SET_PLUNGER_STATE(PLUNGER_STATE_FLYWHEEL_REVUP);
    }
    break;
  }
  case PLUNGER_STATE_FLYWHEEL_REVUP: {
    if (sp) {Serial.println(" s:revup"); sp = false;}
    plungerMotorPWM(MOTOR_DIR_BRAKE, PLUNGER_PWM_MAX);
    ESCPos = FLYWHEEL_MOTOR_ESC_REVUP;
    displayUpdateEnable = false;
    unsigned long revTime;
    if (revTriggerRead()) {
      revTime = FLYWHEEL_REVUP_TIME_FULL;
    } else {
      revTime = FLYWHEEL_REVUP_TIME_SEMI;
    }
    if (STATE_DELAY(revTime)) {
      SET_PLUNGER_STATE(PLUNGER_STATE_CLEAR_END_SWITCH);
    }
    break;
  }
  case PLUNGER_STATE_CLEAR_END_SWITCH: {
    if (sp) {Serial.println(" s:clear"); sp = false;}
    plungerMotorPWM(MOTOR_DIR_FWD, PLUNGER_PWM_RUN_SPEED);
    ESCPos = FLYWHEEL_MOTOR_ESC_RUN;
    displayUpdateEnable = false;
    if (!plungerEndRead()) {
      SET_PLUNGER_STATE(PLUNGER_STATE_RUN_PLUNGER);
      //displayUpdateForce = true;
    }
    break;
  }
  case PLUNGER_STATE_RUN_PLUNGER: {
    if (sp) {Serial.println(" s:run"); sp = false;}
    plungerMotorPWM(MOTOR_DIR_FWD, PLUNGER_PWM_RUN_SPEED);
    ESCPos = FLYWHEEL_MOTOR_ESC_RUN;
    displayUpdateEnable = false;
    if (plungerEndRead() && STATE_DELAY(25)) {
      // plunger is at end.  stop it if we're in semi-auto
      if (!revTriggerRead()) {
        // semi auto.  stop the plunger
        SET_PLUNGER_STATE(PLUNGER_STATE_WAIT_FOR_TRIGGGER_RELEASE);
      } else {
        SET_PLUNGER_STATE(PLUNGER_STATE_ROUND_DELAY);
      }
    }
    break;
  }
  case PLUNGER_STATE_ROUND_DELAY: {
    if (sp) {Serial.println(" s:delay"); sp = false;}
    plungerMotorPWM(MOTOR_DIR_BRAKE, PLUNGER_PWM_MAX);
    // full auto.  leave the flywheel spinning
    ESCPos = FLYWHEEL_MOTOR_ESC_RUN;
    displayUpdateEnable = false;
    if ((millis() - plungerStateTime) > ROUND_DELAY_TIME) {
      //displayUpdateForce = true;
      if (triggerRead()) {
        // chamber another round
        SET_PLUNGER_STATE(PLUNGER_STATE_CLEAR_END_SWITCH);
      }
    }
    if (STATE_DELAY(FLYWHEEL_REVDOWN_TIME_FULL)) {
      // rev down the flywheel to idle speed until the trigger is pulled again
      SET_PLUNGER_STATE(PLUNGER_STATE_IDLE);
    }
    if (!revTriggerRead()) {
      // semi auto.  stop the plunger
      SET_PLUNGER_STATE(PLUNGER_STATE_WAIT_FOR_TRIGGGER_RELEASE);
    }
    break;
  }
  case PLUNGER_STATE_WAIT_FOR_TRIGGGER_RELEASE: {
    if (sp) {Serial.println(" s:release"); sp = false;}
    plungerMotorPWM(MOTOR_DIR_BRAKE, PLUNGER_PWM_MAX);
    ESCPos = FLYWHEEL_MOTOR_ESC_BRAKE;
    displayUpdateEnable = false;
    if (!triggerRead()) {
      SET_PLUNGER_STATE(PLUNGER_STATE_IDLE);
    }
    break;
  }
  }


  servoESC.write(ESCPos);

  if (p) {Serial.println("");}

  if (((millis() > (displayUpdateTime + DISPLAY_UPDATE_PERIOD))
      & displayUpdateEnable) || displayUpdateForce) {
    displayUpdateTime = millis();
    displayUpdateForce = false;
    displayUpdate();
  }
}

#define POSX_SEVEN_SEG_DIGIT_0  66
#define POSX_SEVEN_SEG_DIGIT_1  98
#define POSY_SEVEN_SEG_DIGIT  12

void displayUpdate() {
  // Draw the HUD Display
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("Rapid");
  display.setTextSize(1);
  display.print("Mag:");
  display.println(magazineTypePtr->name);
  display.print("Rd/m:");
  if (roundsPerMin > 0) {
    display.println(roundsPerMin, DEC);
  } else {
    display.println("---");

  }
  display.print("Ft/s:");
  if (velocity >= 0.0) {
    display.println(velocity, 1);
  } else {
    display.println("---");
  }
  display.print("Volt:");
  display.println(voltageBattery, 1);
  if (jamDoorOpen) {
    // draw the jam text inverted if the door is open
    display.setTextColor(BLACK, WHITE); // 'inverted' text
  }
  display.print("Jam:");
  display.println(roundsJamCount, DEC);
  display.setTextColor(WHITE);

  // draw the round digits.
  int digit0 = SEVEN_SEGMENT_BITMAP_DASH;
  int digit1 = SEVEN_SEGMENT_BITMAP_DASH;
  if ((roundCount >= 0) && (roundCount <= 99)) {
    digit0 = roundCount / 10;
    digit1 = roundCount % 10;
  }
  display.setCursor(86, 0);
  display.println("Rounds:");
  display.drawBitmap(POSX_SEVEN_SEG_DIGIT_0, POSY_SEVEN_SEG_DIGIT,
      (uint8_t *) &(SevenSegmentBitMaps[digit0]),
      SEVEN_SEGMENT_BITMAP_WIDTH, SEVEN_SEGMENT_BITMAP_HEIGHT, 1);
  display.drawBitmap(POSX_SEVEN_SEG_DIGIT_1, POSY_SEVEN_SEG_DIGIT,
      (uint8_t *) &(SevenSegmentBitMaps[digit1]),
      SEVEN_SEGMENT_BITMAP_WIDTH, SEVEN_SEGMENT_BITMAP_HEIGHT, 1);

  // draw dividing lines
  display.drawLine(POSX_SEVEN_SEG_DIGIT_0 - 3, 0,
  POSX_SEVEN_SEG_DIGIT_0 - 3, display.height() - 1, WHITE);

  display.display();
}
