#include <Arduino_MKRGPS.h>
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
bool firstCycle;

RTCZero rtc;

void onButtonPressedGreen() {
    buttonPressedGreen = true;
}

void onButtonPressedRed() {
    buttonPressedRed = true;
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
    else {
        while (1) {
            signalLedOff();
            beepOff();
            delay(50);
            signalLedOn();
            beepOn();
            delay(50);
        }
    }
}

void signalLedBlinkAndHalt(String errorMessage) {
    while (1) {
        Serial.println(errorMessage);
        writeErrorFile(errorMessage);
        signalLedOff();
        beepOff();
        delay(200);
        signalLedOn();
        beepOn();
        delay(200);
    }
}

void appendDataFile(float latitude, float longitude, float altitude, float speed, int satellites, unsigned long unixTime, String action) {
    unsigned long time;

    if (unixTime == 0) {
        time = rtc.getSeconds() + rtc.getMinutes() * 60 + rtc.getHours() * 3600;
    }
    else {
        time = unixTime;
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
        f.print(",\"unixTime\":");
        f.print(time);
        f.print(",\"action\":\"");
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
    // put your setup code here, to run once:
    Serial.begin(115200);

    pinMode(pinLed, OUTPUT);
    signalLedOff();

    pinMode(pinBeeper, OUTPUT);
    beepOff();

    Serial.print("Initializing RTC...");
    rtc.begin();
    rtc.setTime(0, 0, 0);
    rtc.setDate(0, 0, 0);
    Serial.println("OK");

    if (!GPS.begin()) {
        signalLedBlinkAndHalt("Failed to initialise GPS. Halting.");
    }
    beepOff();

    Serial.print("Initializing SD card...");
    if (!SD.begin(chipSelect)) {
        signalLedBlinkAndHalt("Failed to initialise SD card. Halting.");
    }
    Serial.println("OK");

    Serial.print("Writing boot log entry...");
    appendDataFile(0, 0, 0, 0, 0, 0, "boot");
    Serial.println("OK");

    Serial.print("Configuring GPS refresh rate...");
    GPS.setUpdateRate(2000, 1, 1);
    Serial.println("OK");

    Serial.print("Waiting for GPS/satellites...");
    while (GPS.satellites() == 0) {
        GPS.available();
        if (rtc.getSeconds() % 2 == 0) {
            signalLedOff();
        }
        else {
            signalLedOn();
        }
        if (rtc.getSeconds() % 15 == 0) {
            Serial.print(".");
            appendDataFile(0, 0, 0, 0, 0, 0, "boot-wait-gps");
            delay(1000);
        }
    }
    Serial.println("OK");

    pinMode(pinButtonGreen, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pinButtonGreen), onButtonPressedGreen, FALLING);
    buttonPressedGreen = false;

    pinMode(pinButtonRed, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pinButtonRed), onButtonPressedRed, FALLING);
    buttonPressedRed = false;

    firstCycle = true;
}

uint8_t lastGpsAvailableMinutes;
uint8_t lastGpsUnavailableMinutes;

void loop() {
    GPS.available();
    
    float latitude = GPS.latitude();
    float longitude = GPS.longitude();
    float altitude = GPS.altitude();
    float speed = GPS.speed();
    int satellites = GPS.satellites();
    unsigned long unixTime = GPS.getTime();

    if (satellites != 0) {
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
        Serial.println(unixTime);

        if (firstCycle) {
            appendDataFile(latitude, longitude, altitude, speed, satellites, unixTime, "boot");
            Serial.println("Boot entry was written to log");
            firstCycle = false;
        }

        if (buttonPressedGreen) {
            beepOn();
            appendDataFile(latitude, longitude, altitude, speed, satellites, unixTime, "green");
            Serial.println("Button: Green");
            signalLedOff();
            delay(1000);
            buttonPressedGreen = false;
            beepOff();
        }

        if (buttonPressedRed) {
            beepOn();
            appendDataFile(latitude, longitude, altitude, speed, satellites, unixTime, "red");
            Serial.println("Button: Red");
            signalLedOff();
            delay(1000);
            buttonPressedRed = false;
            beepOff();
        }

        if (rtc.getMinutes() % 15 == 0) {
            if (rtc.getMinutes() != lastGpsAvailableMinutes) {
                appendDataFile(latitude, longitude, altitude, speed, satellites, unixTime, "gps-available");
                lastGpsAvailableMinutes = rtc.getMinutes();
            }
        }
    }
    else {
        signalLedOff();

        if (rtc.getMinutes() % 15 == 0) {
            if (rtc.getMinutes() != lastGpsUnavailableMinutes) {
                appendDataFile(0, 0, 0, 0, 0, 0, "gps-unavailable");
                lastGpsUnavailableMinutes = rtc.getMinutes();
            }
        }
    }
}
