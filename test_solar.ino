#include <math.h>
#include <TimeLib.h>

// single character message tags
#define TIME_HEADER   'T'   // Header tag for serial time sync message
#define FORMAT_HEADER 'F'   // Header tag indicating a date format message
#define FORMAT_SHORT  's'   // short month and day strings
#define FORMAT_LONG   'l'   // (lower case l) long month and day strings

#define TIME_REQUEST  7     // ASCII bell character requests a time sync message 

const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

const int SYNC_RETRIES = 3;
const int SYNC_ATTEMPT_WAIT_MS = 30000;

const time_t CORRECTION_S_PER_DAY = (time_t) (30UL * SECS_PER_MIN); 

bool calibrated;
unsigned long backThen;

void setup() {
  calibrated = try_calibration();
}

void loop() {
  bool on = !calibrated || should_shine();
  for ( int i = 2 ; i <= 13 ; i += 1 ) {
    analogWrite(i, on ? 255 : 0);
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
  const time_t t = n + (elapsedDays(n) - backThen) * CORRECTION_S_PER_DAY;
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
    backThen = elapsedDays(now());
    return true;
  }

  Serial.println("Time not synced. Always on mode.");
  return false;
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

