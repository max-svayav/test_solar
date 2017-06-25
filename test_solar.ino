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

#define EEPROM_SIGNATURE ((unsigned short) 0x2229)

const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

const int SYNC_RETRIES = 3;
const int SYNC_ATTEMPT_WAIT_MS = 30000;

const time_t CORRECTION_S_PER_DAY = (time_t) (10UL * SECS_PER_MIN); 
VL6180x sensor(0x29);

bool calibrated;
time_t backThen;
time_t lastWrite;
int n;
float acc;

void setup() {
  calibrated = try_calibration();
  init_sensor();
  eeprom_print();
  lastWrite = 0;
  n = 0;
  acc = 0;
}

void loop() {
  bool on = !calibrated || should_shine();
  for ( int i = 2 ; i <= 13 ; i += 1 ) {
    analogWrite(i, on ? 255 : 0);
  }

  if ( calibrated ) {
    acc += get_ambient();
    n += 1;
    time_t n = now();
    if(lastWrite == 0 || n - lastWrite >= 15) {
      lastWrite = n;
      const float ambient = acc / n;
      acc = 0;
      n = 0;
      eeprom_write(ambient);
      Serial.println(ambient);
    }    
  }
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
  return (h >= 5 && h < 10);
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

void init_sensor() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  if(sensor.VL6180xInit()) {
    for(int i = 0 ; i < 3 ; i += 1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
    }
  } else {
    sensor.VL6180xDefautSettings();
    delay(1000);
  }
}

short init_eeprom() {
  unsigned short check;
  short index; 
  EEPROM.get(0, check);
  EEPROM.get(sizeof(check), index);
  if(check != EEPROM_SIGNATURE || index <= sizeof(check) + sizeof(index) || index >= EEPROM.length()) {
    index = sizeof(check) + sizeof(index);    
    EEPROM.put(sizeof(check), index);
    for(int i = index ; i < EEPROM.length() ; i += 1) {
      EEPROM.write(i, -1);
    }
  }  
  return index;
}

void eeprom_write(const float f) {
  short index = init_eeprom();
  EEPROM.put(index, f);
  index += sizeof(f);
  if(index >= EEPROM.length()) {
    index = sizeof(EEPROM_SIGNATURE) + sizeof(index);
  } 
  EEPROM.put(0, EEPROM_SIGNATURE);
  EEPROM.put(sizeof(EEPROM_SIGNATURE), index);
}

struct batch {
  float a, b, c, d;
};

void eeprom_print() {
  const short index = init_eeprom();
  short i = index;
  Serial.print("Ambient=");
  do {
    float f;
    batch b;
    if(i + sizeof(b) <= EEPROM.length()) {
      EEPROM.get(i, b);
      i += sizeof(b);
      Serial.print(b.a);
      Serial.print(", ");
      Serial.print(b.b);
      Serial.print(", ");
      Serial.print(b.c);
      Serial.print(", ");
      Serial.print(b.d);
    } else {
      EEPROM.get(i, f);
      i += sizeof(f);
      Serial.print(f);
    }

    if(i >= EEPROM.length()) {
      i = sizeof(EEPROM_SIGNATURE) + sizeof(i);
    }
    if (i != index) {
      Serial.print(", ");
    }
  } while(i != index);
  Serial.println();
}

/*

interface TimerAction {
public:
  void fire(time_t t) = 0;
};

class IntervalTimer : public TimerAction {
private:
  long delta = 0;
  bool set = false;
  TimerAction & action;
  time_t then;
public:
  TimerAction & everyMinutes(long n, TimerAction &t) {
    return everySeconds(SECS_PER_MIN * n, t);
  }
  
  TimerAction & everySeconds(long n, TimerAction &t) {
    IntervalTimer::delta = n ;
    IntervalTimer::action = t;
    set = true;
    return this;
  }
  
  TimerAction & everyHours(long n, TimerAction &t) {
    return everySeconds(SECS_PER_HOUR * n, t);
  }

  void fire(time_t n) {
    if(then == 0 || n - then >= delta) {
      then = n - n % delta;
      action.fire(n);  
    }
  }
  
};

class Alarm : public TimerAction {
private:
  int hour;
  int minute;
  bool set = false;
  TimerAction & action;
public:
  TimerAction & set(int hour, int minute, TimerAction & action) {
    Alarm::hour = hour;
    Alarm::minute = minute;
    Alarm::action = action;
    set = true;
    return this;
  }

  void set() {
    this.set = true;
  }

  void fire(time_t n) {
    if(!set) {
      return;
    }
    if(::hour(n) >= hour && ::minute(n) >= minute) {
      action.fire(n);
      set = false;
    }
  }

};

class Clock {
private:
  
  class SecondHand : public TimerAction {
  protected:
    Clock &clk;
  public:    
    void fire(time_t n) {
      clk.tack(n);
    }
  } secondHand;

  const correctionPerDay = CORRECTION_S_PER_DAY;
 
  time_t backThen;
  TimerAction & actions[];
  int num;
  TimerAction seconds;

private:
  time_t now() {
    return ::now();
  }

  time_t time() {
    const time_t n = Clock::now();
    const time_t t = n + (elapsedDays(n) - elapsedDays(Clock::backThen)) * correctionPerDay;
    return t;
  }

protected:
  void tack(time_t n) {
    for(int i = 0; i < num; i += 1) {
      actions[i].fire(n); 
    }
  }

public:
  Clock(TimerAction actions[], int l) {
    Clock::actions = actions;
    num = l;
    secondHand.clk = *this;   
  }  
  
  void tick() {
    seconds.fire(Clock::time());
  }

  void start() {
    backThen = Clock::now();
    seconds.everySeconds(1, secondHand);
  }
  
};

class Aggregate {

private:
  float acc;
  int n;
  
public:
  Aggregate():acc{0},n{0} {}; 

  float mean() {
    const float f = acc / n;
    acc = 0;
    n = 0;
    return f;
  }

  int accumulate(float f) {
    acc += f;
    n += 1;
    return n;
  }

  void reset() {
    n = 0;
    acc = 0;
  }

};

void testClock() {
  Alarm
  TimerAction & timers[] = [
    IntervalTimer().everySecond(1, ),    
    IntervalTimer().everySecond(1, ),    
    IntervalTimer().everySecond(1, ),    
  ];
}

*/
