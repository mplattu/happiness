#include <TinyGPSPlus.h>
#include <SPI.h>
#include <SD.h>
#include <RTCZero.h>

const int chipSelect = SDCARD_SS_PIN;

int pinLed = LED_BUILTIN;
int pinButtonGreen = 5;
int pinButtonRed = 6;
int pinBeeper = 4;
bool buttonPressedGreen;
bool buttonPressedRed;
bool firstLocationWrittenToLog;

#define USING_TIMER_TCC true
#include <SAMDTimerInterrupt.h>
#define TIMER_INTERVAL_MS 60000
#define SELECTED_TIMER TIMER_TCC
bool timerInterruptFlagUp;
SAMDTimer ITimer(SELECTED_TIMER);

RTCZero rtc;

void onButtonPressedGreen() {
    buttonPressedGreen = true;
}

void onButtonPressedRed() {
    buttonPressedRed = true;
}

void onTimerInterrupt() {
    timerInterruptFlagUp = true;
}

void signalLedOff() {
    digitalWrite(pinLed, LOW);
}

void signalLedOn() {
    digitalWrite(pinLed, HIGH);
}

void beepOff() {
    digitalWrite(pinBeeper, LOW);
}

void beepOn() {
    digitalWrite(pinBeeper, HIGH);
}

void writeErrorFile(String errorMessage) {
    File f = SD.open("error.log", FILE_WRITE);
    if (f) {
        f.println(errorMessage);
        f.close();
    }
}

void signalLedBlinkAndHalt(uint8_t errorCode, String errorMessage) {
    writeErrorFile(errorMessage);

    while (1) {
        Serial.print(errorMessage);
        Serial.print(" #");
        Serial.println(errorCode);

        signalLedOn();
        delay(3000);
        signalLedOff();

        for (uint8_t n=0; n < errorCode; n++) {
            beepOff();
            signalLedOff();
            delay(200);
            beepOn();
            signalLedOn();
            delay(200);
        }

        beepOff();
        signalLedOff();

        delay(1000);
    }
}

void appendDataFile(float latitude, float longitude, float altitude, float speed, int satellites, char * datetime, String action) {
    char datetimeToWrite[40];

    if (strcmp(datetime, "") == 0) {
        sprintf(datetimeToWrite, "%d", rtc.getSeconds() + rtc.getMinutes() * 60 + rtc.getHours() * 3600);
    }
    else {
        strcpy(datetimeToWrite, datetime);
    }

    File f = SD.open("data.log", FILE_WRITE);
    if (f) {
        f.print("{\"latitude\":");
        f.print(latitude, 7);
        f.print(",\"longitude\":");
        f.print(longitude, 7);
        f.print(",\"altitude\":");
        f.print(altitude, 1);
        f.print(",\"speed\":");
        f.print(speed, 1);
        f.print(",\"satellites\":");
        f.print(satellites);
        f.print(",\"timestamp\":\"");
        f.print(datetimeToWrite);
        f.print("\",\"action\":\"");
        f.print(action);
        f.print("\"");
        f.println("}");
        f.close();
    }
    else {
        Serial.println("Could not open SD for writing");
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    delay(1000);

    pinMode(pinLed, OUTPUT);
    signalLedOff();

    pinMode(pinBeeper, OUTPUT);
    beepOff();

    Serial.print("Initializing RTC...");
    rtc.begin();
    rtc.setTime(0, 0, 0);
    rtc.setDate(0, 0, 0);
    Serial.println("OK");

    Serial.print("Initializing SD card...");
    if (!SD.begin(chipSelect)) {
        signalLedBlinkAndHalt(1, "Failed to initialise SD card. Halting.");
    }
    Serial.println("OK");

    if (!Serial1) {
        signalLedBlinkAndHalt(2, "No serial connection to GPS. Halting.");
    }

    Serial.print("Initialising timer handler...");
    if (!ITimer.attachInterruptInterval_MS(TIMER_INTERVAL_MS, onTimerInterrupt)) {
        signalLedBlinkAndHalt(3, "Cannot initialise timer. Halting.");
    }
    Serial.println("OK");

    beepOff();

    Serial.print("Writing boot log entry...");
    appendDataFile(0, 0, 0, 0, 0, "", "boot");
    Serial.println("OK");

    pinMode(pinButtonGreen, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pinButtonGreen), onButtonPressedGreen, FALLING);
    buttonPressedGreen = false;

    pinMode(pinButtonRed, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pinButtonRed), onButtonPressedRed, FALLING);
    buttonPressedRed = false;

    firstLocationWrittenToLog = false;
}

TinyGPSPlus gps;
uint8_t lastGpsAvailableMinutes;
uint8_t lastGpsUnavailableMinutes;

float latitude = 0;
float longitude = 0;
float altitude = 0;
float speed = 0;
uint32_t satellites = 0;
uint32_t unixTime = 0;
char date[20];
char time[20];
char datetime[40];

void loop() {
    while (Serial1.available() > 0) {
        gps.encode(Serial1.read());

        if (gps.location.isUpdated()) {
            latitude = gps.location.lat();
            longitude = gps.location.lng();
        }

        if (gps.altitude.isUpdated()) {
            altitude = gps.altitude.meters();
        }

        if (gps.speed.isUpdated()) {
            speed = gps.speed.kmph();
        }

        if (gps.satellites.isUpdated()) {
            satellites = gps.satellites.value();
        }

        if (gps.date.isUpdated()) {
            sprintf(date, "%04d-%02d-%02d", gps.date.year(), gps.date.month(), gps.date.day());
            sprintf(datetime, "%s %s", date, time);
        }

        if (gps.time.isUpdated()) {
            sprintf(time, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
            sprintf(datetime, "%s %s", date, time);
        }

        if (satellites != 0 && latitude != 0 && longitude != 0) {
            signalLedOn();

            Serial.print("Location: ");
            Serial.print(latitude, 7);
            Serial.print(", ");
            Serial.print(longitude, 7);
            Serial.print("  Alt: ");
            Serial.print(altitude, 7);
            Serial.print("  Speed: ");
            Serial.print(speed, 2);
            Serial.print("  Satellites: ");
            Serial.print(satellites);
            Serial.print("  Time: ");
            Serial.println(datetime);

            if (!firstLocationWrittenToLog) {
                appendDataFile(latitude, longitude, altitude, speed, satellites, datetime, "boot");
                Serial.println("Boot entry was written to log");
                firstLocationWrittenToLog = true;
            }

            if (buttonPressedGreen) {
                beepOn();
                appendDataFile(latitude, longitude, altitude, speed, satellites, datetime, "green");
                Serial.println("Button: Green");
                signalLedOff();
                delay(1000);
                buttonPressedGreen = false;
                beepOff();
            }

            if (buttonPressedRed) {
                beepOn();
                appendDataFile(latitude, longitude, altitude, speed, satellites, datetime, "red");
                Serial.println("Button: Red");
                signalLedOff();
                delay(1000);
                buttonPressedRed = false;
                beepOff();
            }

            if (timerInterruptFlagUp) {
                appendDataFile(latitude, longitude, altitude, speed, satellites, datetime, "gps-available");
                timerInterruptFlagUp = false;
            }
        }
        else {
            signalLedOff();

            if (timerInterruptFlagUp) {
                appendDataFile(0, 0, 0, 0, 0, "", "gps-unavailable");
                timerInterruptFlagUp = false;

                signalLedOn();
                delay(3000);
                signalLedOff();
            }

            Serial.print("No satellites/location\r");
        }
    }
}
