#ifndef NerfComp_h
#define NerfComp_h

//#define ARDUINO_NANO
#define ARDUINO_PROMICRO

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

#define MAGAZINE_TYPE_DELAY     3
#define MAGAZINE_NAME_SIZE  7

#ifdef ARDUINO_NANO
#define PIN_BARREL_START            2
#define PIN_BARREL_END              3
#define PIN_SAFETY_JAMDOOR          6
#define PIN_SAFETY_MAG              4
#define PIN_FLYWHEEL_ESC            9
#define PIN_FLYWHEEL_TRIGGER        5

#define PIN_BATTERY_VOLTAGE         A7
#define PIN_PLUNGER_TRIGGER         12
#define PIN_PLUNGER_MOTOR_FWD       7
#define PIN_PLUNGER_MOTOR_REV       8
#define PIN_PLUNGER_END_SWITCH      10
#define PIN_PLUNGER_MOTOR_PWM       11
#endif

#ifdef ARDUINO_PROMICRO
#define PIN_BARREL_START            1
#define PIN_BARREL_END              0
#define PIN_SAFETY_JAMDOOR          4
#define PIN_SAFETY_MAG              5
#define PIN_FLYWHEEL_ESC            6
#define PIN_FLYWHEEL_TRIGGER        7

#define PIN_BATTERY_VOLTAGE         A0
#define PIN_PLUNGER_TRIGGER         15
#define PIN_PLUNGER_MOTOR_FWD       14
#define PIN_PLUNGER_MOTOR_REV       16
#define PIN_PLUNGER_END_SWITCH      10
#define PIN_PLUNGER_MOTOR_PWM       9
#endif

//#define VOLTAGE_BATTERY_SCALER      (1/65.8)
#define VOLTAGE_BATTERY_SCALER      (1/68.5)
#define VOLTAGE_BATTERY_IIR_GAIN    0.002

#define MOTOR_DIR_FWD               0
#define MOTOR_DIR_REV               1
#define MOTOR_DIR_BRAKE             2
#define MOTOR_DIR_OFF               3


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
#define FEED_JAM_TIME                   500

#define FLYWHEEL_MOTOR_ESC_NEUTRAL      90
#define FLYWHEEL_MOTOR_ESC_PRE_RUN_FULL 96
#define FLYWHEEL_MOTOR_ESC_REVUP        160
#define FLYWHEEL_MOTOR_ESC_RUN          140
#define FLYWHEEL_MOTOR_ESC_BRAKE        15

#define VELOCITY_FPS_MIN                20.0
#define VELOCITY_FPS_MAX                400.0
#define DART_LENGTH_INCHES              2.85
#define DART_LENGTH_MM                  40

#define HEARTBEAT_UPDATE_PERIOD         50
#define HEARTBEAT_PRINT_PERIOD          250

#define VOLTAGE_MIN         0.0
#define VOLTAGE_MAX         15.0


#define DISPLAY_UPDATE_PERIOD 100
#define BARREL_INTER_DART_TIME_US   10000L

#define SET_PLUNGER_STATE(s)  plungerState = s;plungerStateTime = millis();sp=true
#define STATE_DELAY(t)      ((millis() - plungerStateTime) > t)



#define CONFIG_PARAM_NAME_SIZE 17

typedef struct ConfigParam {
  const char name[CONFIG_PARAM_NAME_SIZE];
  const int16_t valueDefault;
  const int16_t valueMin;
  const int16_t valueMax;
  const int16_t valueStep;
} ConfigParam;

int8_t paramCount(void);
int16_t paramValueDefault(uint8_t idx);
int16_t paramValueMax(uint8_t idx);
int16_t paramValueMin(uint8_t idx);
int16_t paramValueStep(uint8_t idx);
const char* paramName(uint8_t idx);

int16_t paramRead(uint8_t paramIdx);
void paramWrite(uint8_t paramIdx, int16_t val);
void paramDefaultCheck(void);
void paramInit(void);

int16_t paramReadDisplayDim(void);
int16_t paramReadResetAll(void);
int16_t paramReadInvertMag(void);

//////// globals ////////
extern float voltageBatteryAvg;
extern uint8_t magazineType;
extern uint8_t magazineTypeIdx;
extern int16_t roundsPerMin;
extern float velocity;
extern uint8_t roundsJamCount;
extern int8_t roundCount;
extern boolean jamDoorOpen;
extern boolean feedJam;
// extern volatile unsigned long timeBarrelStart;
// extern volatile boolean timeBarrelStartFlag;
// extern volatile unsigned long timeBarrelEnd;
// extern volatile boolean timeBarrelEndFlag;




//////// function declarations ////////
void paramInit(void);
int16_t paramRead(uint8_t paramIdx);
void SerialPrint_F (const char * str);
boolean switchMagSafetyRead(void);
boolean switchRevTriggerRead(void);
boolean switchJamDoorRead(void);
boolean switchTriggerRead(void);
boolean switchPlungerStopRead(void);
boolean sensorBarrelRead(void);
uint8_t magTypeReadBits(void);

uint8_t magazineTypesGetCode(uint8_t magazineTypeIdx);
uint8_t magazineTypesGetCapacity(uint8_t magazineTypeIdx);
const char * magazineTypesGetName(uint8_t magazineTypeIdx);  
int freeRam (void);  

void heartbeatInit(void);  
boolean heartbeatUpdate(void);
  
#endif // NerfComp_h
