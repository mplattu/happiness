#include <Arduino_MKRGPS.h>
#include <SPI.h>
#include <SD.h>

const int chipSelect = SDCARD_SS_PIN;


void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    while (!Serial);

    if (!GPS.begin()) {
        Serial.println("Failed to initialise GPS. Halting.");
        while (1);
    }

    Serial.print("Initializing SD card...");
    if (!SD.begin(chipSelect)) {
        Serial.println("Failed. Halting.");
        while (1);
    }
    Serial.println("OK");

    Serial.print("Configuring GPS refresh rate...");
    GPS.setUpdateRate(2000, 1, 1);
    Serial.println("OK");

    Serial.print("Waiting for GPS/satellites...");
    while (GPS.satellites() == 0) {
        GPS.available();
    }
    Serial.print("OK");
}

void appendDataFile(float latitude, float longitude, float altitude, float speed, int satellites, unsigned long unixTime) {
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
        f.print(unixTime);
        f.println("}");
        f.close();
    }
    else {
        Serial.println("Could not open SD for writing");
    }
}


void loop() {
    GPS.available();
    
    float latitude = GPS.latitude();
    float longitude = GPS.longitude();
    float altitude = GPS.altitude();
    float speed = GPS.speed();
    int satellites = GPS.satellites();
    unsigned long unixTime = GPS.getTime();

    if (satellites != 0) {
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

        appendDataFile(latitude, longitude, altitude, speed, satellites, unixTime);
    }
}
