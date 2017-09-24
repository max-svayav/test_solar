#include <math.h>
#include <TimeLib.h>
#include <Wire.h>
#include <SparkFun_VL6180X.h>
#include <EEPROM.h>

// single character message tags
#define TIME_HEADER   'T'   // Header tag for serial time sync message
#define FORMAT_HEADER 'F'   // Header tag indicating a date format message
#define FORMAT_SHORT  's'   // short month and day strings
#define FORMAT_LONG   'l'   // (lower case l) long month and day strings

#define TIME_REQUEST  7     // ASCII bell character requests a time sync message 

#define EEPROM_SIGNATURE ((unsigned short) 0x272c)

const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

const int SYNC_RETRIES = 3;
const int SYNC_ATTEMPT_WAIT_MS = 30000;

const time_t CORRECTION_S_PER_DAY = (time_t) (10UL * SECS_PER_MIN); 
VL6180x sensor(0x29);

bool eeprom_diag = true;
bool calibrated;
bool sensor_init;
time_t backThen;
time_t lastWrite;
int n_acc;
float acc;

size_t ee_length;

void setup() {
  calibrated = try_calibration();
  sensor_init = init_sensor();
  ee_length = EEPROM.length();
  eeprom_print();
  lastWrite = 0;
  n_acc = 0;
  acc = 0;
}

void loop() {
  bool on = !calibrated || should_shine();
  for ( int i = 2 ; i <= 13 ; i += 1 ) {
    analogWrite(i, on ? 255 : 0);
  }

  if ( calibrated && sensor_init ) {
    acc += get_ambient();
    n_acc += 1;
    const time_t n = now();
    if(lastWrite == 0 || n - lastWrite >= SECS_PER_HOUR) {
      lastWrite = n;
      const float ambient = acc / n_acc;
      acc = 0;
      n_acc = 0;
      eeprom_write(ambient);
    }    
  }

  delay(100);
}

bool should_shine() {
  return check_sun() || check_schedule();
}

bool check_sun() {
  return false;
}

bool check_schedule() {
  const time_t n = now();
  const time_t t = n + (elapsedDays(n) - elapsedDays(backThen)) * CORRECTION_S_PER_DAY;
  const int h = hour(t);
  const int m = minute(t);          
  return (h >= 6 && h < 10) || (h >= 17 && h < 20);
}

bool try_calibration() {
  Serial.begin(9600);
  while (!Serial) ; // Needed for Leonardo only
  Serial.println("Waiting for sync message");

  for ( int i = 0 ; i < SYNC_RETRIES && timeStatus() == timeNotSet ; i += 1 ) {
    Serial.println("");
    Serial.println(SYNC_RETRIES - i);
    Serial.write(TIME_REQUEST);

    for ( unsigned long then = millis() ; millis() - then <= SYNC_ATTEMPT_WAIT_MS && timeStatus() == timeNotSet ; delay(1000) ) {
      if ( Serial.available() > 1 ) { // wait for at least two characters
        char c = Serial.read();
        if ( c == TIME_HEADER) {
          unsigned long pctime = Serial.parseInt();
          if ( pctime >= DEFAULT_TIME) {
            setTime(pctime); // Sync Arduino clock to the time received on the serial port
          }
        } else if ( c == FORMAT_HEADER) {
          char c = Serial.read();
          Serial.println(F("Ignoring format"));
        }
      }
    }

  }
  
  if (timeStatus() != timeNotSet) {
    digitalClockDisplay();
    Serial.println("Time synced. On by schedule mode.");
    backThen = now();
    return true;
  }

  Serial.println("Time not synced. Always on mode.");
  return false;
}

float get_ambient() {
  const float ambient = sensor.getAmbientLight(GAIN_1);
  return ambient;
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(dayStr(weekday()));
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(monthStr(month()));
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10) {
    Serial.print('0');
  }
  Serial.print(digits);
}

bool init_sensor() {
  if(sensor.VL6180xInit()) {
    Serial.println("VL6180x init failure");
    return false;
  } else {
    sensor.VL6180xDefautSettings();
    delay(1000);
    return true;
  }
}

short init_eeprom() {
  unsigned short check;
  short index; 
  EEPROM.get(0, check);
  EEPROM.get(sizeof(check), index);
  size_t i = index;
  if(check != EEPROM_SIGNATURE || i < sizeof(check) + sizeof(index) || i == ee_length || i + sizeof(float) > ee_length) {
    index = sizeof(check) + sizeof(index);    
    EEPROM.put(sizeof(check), index);
    EEPROM.get(sizeof(check), index);
    for(int j = index ; j < ee_length ; j += 1) {
      EEPROM.update(j, -1);
    }
    EEPROM.put(0, EEPROM_SIGNATURE);
  }
  if(eeprom_diag) {
    Serial.print("Sig=");
    EEPROM.get(0, check);
    Serial.print(check);
    Serial.print(";Offs=");
    EEPROM.get(sizeof(check), check);
    Serial.println(check);
    eeprom_diag = false;
  }
  return index;
}

void eeprom_write(const float f) {
  short index = init_eeprom();
  size_t i = index;
  EEPROM.put(i, f);
  float g;
  EEPROM.get(i, g);
  if (f != g) {
    EEPROM.put(i, f);
    EEPROM.put(i, (long) -1);
  }
  i += sizeof(f);
  if(i == ee_length || i + sizeof(float) > ee_length) {
    i = sizeof(EEPROM_SIGNATURE) + sizeof(index);
  } 
  index = i;
  EEPROM.put(sizeof(EEPROM_SIGNATURE), index);
}

void eeprom_print() {
  const short index = init_eeprom();
  size_t i = index;
  Serial.print("Ambient=");
  do {
    float f;
    EEPROM.get(i, f);
    i += sizeof(f);
    Serial.print(f);
    if (i == ee_length || i + sizeof(f) > ee_length) {
      i = sizeof(EEPROM_SIGNATURE) + sizeof(index);
    }
    if (i != index) {
      Serial.print(", ");
    }
  } while(i != index);
  Serial.println();
}

