#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <WiFiS3.h>
#include <ArduinoJson.h>
#include <RTC.h>
#include <SD.h>

const int SD_CS_PIN = 9;
int logCounter = 1;

hd44780_I2Cexp lcd;

char ssid[] = "";
char pass[] = "";

WiFiSSLClient client;

#define MQ3_PIN A2
#define RELAY_PIN 8
#define BUZZER_PIN 10

bool f = true;

int baselineReading = 0;

const int SAFE_THRESHOLD = 100;
const int LOW_THRESHOLD = 300;
const int MED_THRESHOLD = 500;

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,A0,A1};

String correctPassword = "7808";

bool engineUnlocked = false;
bool endtask = false;

int lockedAlcoholValue = -1;

const char* THINGSPEAK_SERVER = "api.thingspeak.com";
const char* THINGSPEAK_WRITE_API_KEY = "...";
void displayLogData();
void logDataToSD(String data);
void warmupMQ3();

char getKeyPressed_manual()
{
    for(int r=0;r<ROWS;r++)
    {
        digitalWrite(rowPins[r],LOW);
        delay(5);

        for(int c=0;c<COLS;c++)
        {
            if(digitalRead(colPins[c])==LOW)
            {
                delay(20);

                if(digitalRead(colPins[c])==LOW)
                {
                    char pressedKey = keys[r][c];

                    while(digitalRead(colPins[c])==LOW);

                    digitalWrite(rowPins[r],HIGH);

                    return pressedKey;
                }
            }
        }

        digitalWrite(rowPins[r],HIGH);
        delay(5);
    }

    return 0;
}

void initializeSystem()
{
    lcd.clear();
    lcd.print(" Welcome to ");
    lcd.setCursor(0,1);
    lcd.print("LOCK 'N' BREATHE");
    delay(4000);

    lcd.clear();
    lcd.print("System is ");
    lcd.setCursor(0,1);
    lcd.print("initializing...");
    delay(3000);
}

void decide()
{
    lcd.clear();
    lcd.print("Press A to Start");
    lcd.setCursor(0,1);
    lcd.print("Press D for data");

    while(true)
    {
        char d = getKeyPressed_manual();

        if(d=='A')
        {
            lcd.clear();
            warmupMQ3();
            break;
        }
        else if(d=='D')
        {
            lcd.clear();
            lcd.print("Displaying data...");
            lcd.clear();

            displayLogData();

            if(f==false)
            {
                lcd.clear();
                lcd.print("Press A to Start");
                lcd.setCursor(0,1);
                lcd.print("Press D for data");
                delay(1000);
                continue;
            }
        }
    }
}

void warmupMQ3()
{
    lcd.clear();
    lcd.print("Please wait for");
    lcd.setCursor(0,1);
    lcd.print("60 seconds");

    delay(3000);

    for(int i=60;i>0;i--)
    {
        lcd.clear();
        lcd.print("Warming sensor");
        lcd.setCursor(0,1);
        lcd.print("Wait: ");
        lcd.print(i);
        lcd.print("s");

        delay(1000);
    }

    lcd.clear();
    lcd.print("Calibrating...");
    lcd.setCursor(0,1);
    lcd.print("Don't blow yet!");

    delay(3000);

    long total = 0;

    for(int i=0;i<10;i++)
    {
        total += analogRead(MQ3_PIN);
        delay(100);
    }

    int avgReading = total / 10;

    baselineReading = map(avgReading,0,1023,0,1000);

    lcd.clear();
    lcd.print("Sensor ready!");
    delay(2000);

    Serial.print("Baseline: ");
    Serial.print(baselineReading);
    Serial.println(" ppm");
}

bool waitForBlow()
{
    lcd.clear();

    const char* message =
        "Please blow into the sensor ";

    int messageLength = strlen(message);

    int scrollPosition = 0;
    const int LCD_WIDTH = 16;

    unsigned long startTime = millis();

    int maxReading = 0;

    while(millis() - startTime < 10000)
    {
        lcd.setCursor(0,0);

        for(int i=0;i<LCD_WIDTH;i++)
        {
            int charIndex =
              (scrollPosition + i) % messageLength;

            lcd.print(message[charIndex]);
        }

        scrollPosition++;

        if(scrollPosition >= messageLength)
            scrollPosition = 0;

        int reading = analogRead(MQ3_PIN);

        if(reading > maxReading)
            maxReading = reading;

        int remaining =
           10 - ((millis()-startTime)/1000);

        lcd.setCursor(0,1);
        lcd.print("Time left: ");
        lcd.print(remaining);
        lcd.print("s");

        delay(500);
    }

    int rawPPM =
       map(maxReading,0,1023,0,1000);

    lockedAlcoholValue =
       rawPPM - baselineReading;

    if(lockedAlcoholValue < 0)
        lockedAlcoholValue = 0;

    lcd.clear();
    lcd.print("Reading alcohol");
    lcd.setCursor(0,1);
    lcd.print("level...");
    delay(2000);

    Serial.print("Alcohol: ");
    Serial.print(lockedAlcoholValue);
    Serial.println(" ppm");

    return true;
}

bool sendToThingSpeak(float latitude, float longitude, int alcoholPPM)
{
    WiFiClient tsClient;

    if (!tsClient.connect(THINGSPEAK_SERVER, 80))
    {
        Serial.println("ThingSpeak connection failed");
        return false;
    }

    String postData =
        String("field1=") + latitude +
        "&field2=" + longitude +
        "&field3=" + alcoholPPM;

    String httpRequest =
        String("POST /update?api_key=") +
        THINGSPEAK_WRITE_API_KEY +
        " HTTP/1.1\r\n" +
        "Host: " + THINGSPEAK_SERVER + "\r\n" +
        "Connection: close\r\n" +
        "Content-Type: application/x-www-form-urlencoded\r\n" +
        "Content-Length: " + postData.length() + "\r\n\r\n" +
        postData;

    tsClient.print(httpRequest);

    unsigned long timeout = millis();

    while (tsClient.connected() &&
           millis() - timeout < 5000)
    {
        while (tsClient.available())
        {
            String line =
                tsClient.readStringUntil('\n');

            Serial.println(line);
        }
    }

    tsClient.stop();

    Serial.println("Data sent to ThingSpeak");

    return true;
}

void displayDateTime()
{
    lcd.clear();
    lcd.print(" Fetching Date");
    lcd.setCursor(0, 1);
    lcd.print(" and Time : ");

    delay(1000);

    WiFiClient httpClient;

    Serial.println("Trying HTTP time API...");

    unsigned long epochTime = WiFi.getTime();

    if (epochTime == 0)
    {
        Serial.println("NTP failed, retrying...");
        delay(1000);
        epochTime = WiFi.getTime();
    }

    if (epochTime == 0)
    {
        Serial.println("ERROR: Could not get time");

        lcd.clear();
        lcd.print("Time fetch");
        lcd.setCursor(0, 1);
        lcd.print("failed");

        delay(2000);
        return;
    }

    Serial.print("Got epoch time: ");
    Serial.println(epochTime);

    String timezoneStr = "Asia/Kolkata";
    int offsetSeconds = 19800;

    if (httpClient.connect("ip-api.com", 80))
    {
        Serial.println("Connected to ip-api.com");

        httpClient.println(
            "GET /json/?fields=timezone,offset HTTP/1.1");

        httpClient.println("Host: ip-api.com");
        httpClient.println("Connection: close");
        httpClient.println();

        delay(500);

        unsigned long timeout = millis();

        while (!httpClient.available() &&
               millis() - timeout < 10000)
        {
            delay(10);
        }

        if (httpClient.available())
        {
            Serial.println("Got response from ip-api");

            while (httpClient.available())
            {
                String line =
                    httpClient.readStringUntil('\n');

                if (line == "\r")
                    break;
            }

            String response = "";

            while (httpClient.available())
            {
                response +=
                    (char)httpClient.read();
            }

            httpClient.stop();

            Serial.println("Response: " + response);

            int tzIndex =
                response.indexOf("\"timezone\":\"");

            if (tzIndex != -1)
            {
                String timezone =
                    response.substring(
                        tzIndex + 12,
                        tzIndex + 50);

                int endQuote =
                    timezone.indexOf("\"");

                timezoneStr =
                    timezone.substring(0, endQuote);

                Serial.println(
                    "Timezone: " + timezoneStr);
            }

            int offsetIndex =
                response.indexOf("\"offset\":");

            if (offsetIndex != -1)
            {
                String offsetStr =
                    response.substring(
                        offsetIndex + 9,
                        offsetIndex + 20);

                int comma =
                    offsetStr.indexOf(",");

                if (comma == -1)
                    comma = offsetStr.indexOf("}");

                offsetStr =
                    offsetStr.substring(0, comma);

                offsetSeconds =
                    offsetStr.toInt();

                Serial.print("Offset seconds: ");
                Serial.println(offsetSeconds);
            }
        }
        else
        {
            Serial.println(
                "No response from ip-api, using default IST");
        }
    }
    else
    {
        Serial.println(
            "Could not connect to ip-api, using default IST");
    }

    epochTime += offsetSeconds;

    int ss = epochTime % 60;
    epochTime /= 60;

    int mm = epochTime % 60;
    epochTime /= 60;

    int hh = epochTime % 24;
    epochTime /= 24;

    int days = epochTime;

    int year = 1970;
    int month = 1;
    int day = 1;

    int daysInMonth[] =
    {
        31,28,31,30,31,30,
        31,31,30,31,30,31
    };

    while (days >= 365)
    {
        int daysThisYear = 365;

        if ((year % 4 == 0 &&
             year % 100 != 0) ||
             (year % 400 == 0))
        {
            daysThisYear = 366;
        }

        if (days >= daysThisYear)
        {
            days -= daysThisYear;
            year++;
        }
        else
        {
            break;
        }
    }

    if ((year % 4 == 0 &&
         year % 100 != 0) ||
         (year % 400 == 0))
    {
        daysInMonth[1] = 29;
    }

    for (int m = 0; m < 12; m++)
    {
        if (days >= daysInMonth[m])
        {
            days -= daysInMonth[m];
            month++;
        }
        else
        {
            day = days + 1;
            break;
        }
    }

    lcd.clear();
lcd.print("Date:");
lcd.setCursor(0, 1);

char dateStr[11];
sprintf(dateStr, "%04d-%02d-%02d", year, month, day);

lcd.print(dateStr);
delay(3000);

lcd.clear();
lcd.print("Time ");
lcd.print(":");
lcd.setCursor(0, 1);

char timeStr[9];
sprintf(timeStr, "%02d:%02d:%02d", hh, mm, ss);

lcd.print(timeStr);
delay(3000);

Serial.print("Final DateTime: ");
Serial.print(dateStr);
Serial.print(" ");
Serial.println(timeStr);

Serial.println("--- Time Display Complete ---\n");
}

bool fetchLocation(float &lat, float &lon)
{
    lcd.clear();
    lcd.print("Fetching");
    lcd.setCursor(0, 1);
    lcd.print("location...");
    delay(1500);

    client.stop();
    delay(100);

    Serial.println("Connecting to ipinfo.io...");

    if (client.connect("ipinfo.io", 443))
    {
        Serial.println("Connected!");

        client.println("GET /json HTTP/1.1");
        client.println("Host: ipinfo.io");
        client.println("User-Agent: Arduino/1.0");
        client.println("Connection: close");
        client.println();

        unsigned long timeout = millis();

        while (!client.available())
        {
            if (millis() - timeout > 10000)
            {
                Serial.println("Timeout!");

                client.stop();

                lcd.clear();
                lcd.print("Location timeout");
                delay(2000);

                return false;
            }

            delay(10);
        }

        String response = "";
        bool jsonStarted = false;

        while (client.available())
        {
            String line = client.readStringUntil('\n');

            if (line.startsWith("{"))
            {
                jsonStarted = true;
            }

            if (jsonStarted)
            {
                response += line;
            }
        }

        client.stop();

        Serial.println("Response received");

        int locIndex = response.indexOf("\"loc\":");

        if (locIndex != -1)
        {
            int startQuote = response.indexOf("\"", locIndex + 6);
            int endQuote = response.indexOf("\"", startQuote + 1);

            String loc = response.substring(startQuote + 1, endQuote);

            int commaPos = loc.indexOf(',');

            if (commaPos != -1)
            {
                String lats = loc.substring(0, commaPos);
                String lons = loc.substring(commaPos + 1);

                lat = lats.toFloat();
                lon = lons.toFloat();

                lcd.clear();
                lcd.print("Latitude:");
                lcd.setCursor(0, 1);
                lcd.print(lat);
                delay(3000);

                lcd.clear();
                lcd.print("Longitude:");
                lcd.setCursor(0, 1);
                lcd.print(lon);
                delay(3000);

                Serial.println("Location success!");

                return true;
            }
            else
            {
                lcd.clear();
                lcd.print("Parse error");
                delay(2000);
            }
        }
        else
        {
            lcd.clear();
            lcd.print("Location failed");
            delay(2000);
        }
    }
    else
    {
        Serial.println("Connection failed");

        lcd.clear();
        lcd.print("Cannot connect");
        delay(2000);
    }

    return false;
}

bool enterPassword()
{
    String input = "";

    lcd.clear();
    lcd.print("Enter password:");
    lcd.setCursor(0, 1);

    while (input.length() < 4)
    {
        char key = getKeyPressed_manual();

        if (key && key >= '0' && key <= '9')
        {
            input += key;

            lcd.setCursor(input.length() - 1, 1);
            lcd.print("*");

            delay(300);
        }
    }

    delay(800);

    if (input == correctPassword)
    {
        lcd.clear();
        lcd.print("Password");
        lcd.setCursor(0, 1);
        lcd.print("verified");

        delay(2500);

        lcd.clear();
        lcd.print("Access granted");

        delay(2500);

        return true;
    }
    else
    {
        lcd.clear();
        lcd.print("Wrong password");

        delay(2500);

        lcd.clear();
        lcd.print("Access denied");

        delay(2500);

        return false;
    }
}

void processAlcoholLevel()
{
    int ppm = lockedAlcoholValue;

    lcd.clear();
    lcd.print("Analyzing...");
    lcd.setCursor(0, 1);
    lcd.print("Level: ");
    lcd.print(ppm);
    lcd.print(" ppm");

    delay(2500);

    float lat = 0;
    float lon = 0;

    if (ppm == 0)
    {
        lcd.clear();
        lcd.print("No Blow Detected");

        delay(1500);

        endtask = true;
    }
    else if (ppm >= 1 && ppm < SAFE_THRESHOLD)
    {
        lcd.clear();
        lcd.print("SAFE: ");
        lcd.print(ppm);
        lcd.print(" ppm");

        delay(2500);

        lcd.clear();
        lcd.print("No alcohol");
        lcd.setCursor(0, 1);
        lcd.print("detected");

        delay(2500);

        lcd.clear();
        lcd.print("Engine unlocked");
        lcd.setCursor(0, 1);
        lcd.print("Drive safe");

        logDataToSD(String(ppm) + " ppm, SAFE");

        digitalWrite(RELAY_PIN, LOW);

        engineUnlocked = true;

        delay(2500);
    }
    else if (ppm >= SAFE_THRESHOLD &&
             ppm < LOW_THRESHOLD)
    {
        lcd.clear();
        lcd.print("LOW: ");
        lcd.print(ppm);
        lcd.print(" ppm");

        delay(2500);

        lcd.clear();
        lcd.print("Range: 100-299");
        lcd.setCursor(0, 1);
        lcd.print("ppm");

        delay(2500);

        lcd.clear();
        lcd.print("Engine unlocked");
        lcd.setCursor(0, 1);
        lcd.print("Drive safe");

        logDataToSD(String(ppm) + " ppm, LOW");

        digitalWrite(RELAY_PIN, LOW);

        engineUnlocked = true;

        delay(2500);
    }
    else if (ppm >= LOW_THRESHOLD &&
             ppm < MED_THRESHOLD)
    {
        lcd.clear();
        lcd.print("MEDIUM: ");
        lcd.print(ppm);
        lcd.print(" ppm");

        delay(2500);

        lcd.clear();
        lcd.clear();
lcd.print("Range: 300-499");
lcd.setCursor(0, 1);
lcd.print("ppm");
delay(2500);

lcd.clear();

unsigned long startTime = millis();
unsigned long lastTimerUpdate = 0;
unsigned long lastScrollUpdate = 0;

int lastRemaining = -1;

const int TIMER_INTERVAL = 1000;
const int SCROLL_SPEED = 150;
const int LCD_WIDTH = 16;

const char* message = "Press 1 to bypass...";
int messageLength = strlen(message);

int scrollPosition = 0;
bool bypassRequested = false;

while (millis() - startTime < 30000)
{
    unsigned long currentTime = millis();

    char key = getKeyPressed_manual();

    if (key == '1')
    {
        bypassRequested = true;
        break;
    }

    if (currentTime - lastTimerUpdate >= TIMER_INTERVAL)
    {
        lastTimerUpdate = currentTime;

        int remaining =
            30 - ((currentTime - startTime) / 1000);

        lcd.setCursor(0, 1);
        lcd.print("Time: ");
        lcd.print(remaining);
        lcd.print("s  ");
    }

    if (currentTime - lastScrollUpdate >= SCROLL_SPEED)
    {
        lastScrollUpdate = currentTime;

        lcd.setCursor(0, 0);

        for (int i = 0; i < LCD_WIDTH; i++)
        {
            int charIndex =
                (scrollPosition + i) % messageLength;

            lcd.print(message[charIndex]);
        }

        scrollPosition++;

        if (scrollPosition >= messageLength)
        {
            scrollPosition = 0;
        }
    }
}

if (bypassRequested)
{
    lcd.clear();
    lcd.print("Bypass required");
    delay(2500);

    lcd.clear();
    lcd.print("Press * for");
    lcd.setCursor(0, 1);
    lcd.print("password");

    unsigned long pStartTime = millis();
    bool starPressed = false;

    while (millis() - pStartTime < 15000)
    {
        char key = getKeyPressed_manual();

        if (key == '*')
        {
            starPressed = true;
            break;
        }
    }

    if (starPressed && enterPassword())
    {
        lcd.clear();
        lcd.print("Engine unlocked");
        delay(2500);

        lcd.clear();
        lcd.print("Drive carefully");

        logDataToSD(
            String(ppm) +
            " ppm, MEDIUM (Bypassed)"
        );

        digitalWrite(RELAY_PIN, LOW);

        tone(BUZZER_PIN, 2000);

        engineUnlocked = true;

        delay(2500);

        displayDateTime();

        if (fetchLocation(lat, lon))
        {
            Serial.println(
                "Location fetched. Saving and uploading."
            );

            sendToThingSpeak(lat, lon, ppm);
        }
        else
        {
            Serial.println("Failed to thing...");
        }
    }
    else
    {
        lcd.clear();
        lcd.print("Bypass failed");
        delay(2500);

        lcd.clear();
        lcd.print("Engine locked");

        logDataToSD(
            String(ppm) +
            " ppm, MEDIUM (Bypass FAILED)"
        );

        digitalWrite(RELAY_PIN, HIGH);

        tone(BUZZER_PIN, 3000, 10000);

        delay(2500);

        displayDateTime();

        if (fetchLocation(lat, lon))
        {
            Serial.println(
                "Location fetched. Saving and uploading."
            );

            sendToThingSpeak(lat, lon, ppm);
        }
        else
        {
            Serial.println("Failed to thing...");
        }

        delay(2000);

        lcd.clear();
        lcd.print("--End--");

        delay(2000);

        endtask = true;
    }
}
else
{
    lcd.clear();
    lcd.print("No response");

    delay(2500);

    lcd.clear();
    lcd.print("Engine locked");

    logDataToSD(
        String(ppm) +
        " ppm, MEDIUM (No Bypass)"
    );

    digitalWrite(RELAY_PIN, HIGH);

    displayDateTime();

    if (fetchLocation(lat, lon))
    {
        Serial.println(
            "Location fetched. Saving and uploading."
        );

        sendToThingSpeak(lat, lon, ppm);
    }
    else
    {
        Serial.println("Failed to thing...");
    }

    delay(2000);

    endtask = true;
}
}
else if (ppm >= MED_THRESHOLD)
{
    lcd.clear();
    lcd.print("HIGH: ");
    lcd.print(ppm);
    lcd.print(" ppm");

    delay(2500);

    lcd.clear();
    lcd.print("Range: above");
    lcd.setCursor(0, 1);
    lcd.print("500 ppm");

    delay(2500);

    lcd.clear();
    lcd.print("Bypass denied");

    delay(2500);

    lcd.clear();
    lcd.print("Engine locked");

    logDataToSD(String(ppm) + " ppm, HIGH");

    digitalWrite(RELAY_PIN, HIGH);

    tone(BUZZER_PIN, 4000, 20000);

    delay(2500);

    displayDateTime();

    if (fetchLocation(lat, lon))
    {
        Serial.println("Location fetched. Saving and uploading.");

        sendToThingSpeak(lat, lon, ppm);
    }
    else
    {
        Serial.println("Failed to thing...");
    }

    delay(2000);

    lcd.clear();
    lcd.print(" --End--");
}
}

void logDataToSD(String data)
{
    File dataFile =
        SD.open("dt.txt", FILE_WRITE);

    if (dataFile)
    {
        dataFile.println(
            String(logCounter) +
            ". " +
            data
        );

        logCounter++;

        dataFile.close();

        Serial.println(
            "Logged to SD: " + data
        );
    }
    else
    {
        Serial.println(
            "Error opening dt.txt"
        );

        lcd.clear();
        lcd.print("SD Write Error");

        delay(2000);
    }
}

void displayLogData()
{
    lcd.clear();

    File dataFile = SD.open("dt.txt");

    if (!dataFile)
    {
        if (dataFile)
        {
            dataFile.close();
        }

        Serial.println(
            "Error opening dt.txt or file not found"
        );

        lcd.print("Log is Empty");

        delay(2500);

        f = false;

        return;
    }

    if (dataFile.size() == 0)
    {
        if (dataFile)
        {
            dataFile.close();
        }

        Serial.println(
            "Log file is empty."
        );

        lcd.print("Log is Empty");

        dataFile.close();

        delay(2500);

        f = false;

        return;
    }

    lcd.print("Past Records: ");
    lcd.setCursor(0, 1);
    lcd.print("A=Next, *=Exit");

    delay(3000);

    while (true)
    {
        if (dataFile.available())
        {
            String line =
                dataFile.readStringUntil('\n');

            line.trim();

            Serial.println(
                "Displaying: " + line
            );

            lcd.clear();

            if (line.length() <= 16)
            {
                lcd.print(line);
            }
            else
            {
                lcd.print(
                    line.substring(0, 16)
                );

                lcd.setCursor(0, 1);

                lcd.print(
                    line.substring(16)
                );
            }

            char key = 0;

            while (key != 'A' &&
                   key != '*')
            {
                key =
                    getKeyPressed_manual();

                delay(20);
            }

            if (key == '*')
            {
                Serial.println(
                    "Exiting viewer."
                );

                break;
            }

            if (key == 'A')
            {
                Serial.println(
                    "Next entry."
                );

                continue;
            }
        }
        else
        {
            Serial.println(
                "End of log reached."
            );

            lcd.clear();
            lcd.print("End of log");
            }



    }
dataFile.close();
decide();
}

void setup()
{
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(RELAY_PIN, HIGH);

    for (int i = 0; i < ROWS; i++)
    {
        pinMode(rowPins[i], OUTPUT);
        digitalWrite(rowPins[i], HIGH);
    }

    for (int i = 0; i < COLS; i++)
    {
        pinMode(colPins[i], INPUT_PULLUP);
    }

    Serial.begin(9600);

    lcd.begin(16, 2);
    lcd.backlight();

    Serial.println("Initializing SD card...");

    if (!SD.begin(SD_CS_PIN))
    {
        Serial.println("Card failed, or not present");

        lcd.clear();
        lcd.print("SD Card ERROR");
        lcd.setCursor(0, 1);
        lcd.print("Check wiring");

        delay(3000);
    }
    else
    {
        Serial.println("SD card initialized.");
    }

    initializeSystem();

    lcd.clear();

    WiFi.begin(ssid, pass);

    unsigned long wifiStart = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - wifiStart < 15000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi connected!");

        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        lcd.clear();
        lcd.print("WiFi Failed");
        lcd.setCursor(0, 1);
        lcd.print("Continuing...");

        delay(2500);

        Serial.println("\nWiFi failed");
    }

    lcd.clear();
    lcd.print("Follow following");
    lcd.setCursor(0, 1);
    lcd.print("Procedures: ");

    delay(3000);

    decide();
    waitForBlow();
    processAlcoholLevel();
}
void oldloop()
{
    if (engineUnlocked)
    {
        lcd.clear();

        bool vehicleRunning = false;

        while (engineUnlocked)
        {
            lcd.clear();
            lcd.print("Vehicle started");
            lcd.setCursor(0, 1);
            lcd.print("Drive safely");

            vehicleRunning = true;

            delay(3000);

            lcd.clear();
            lcd.print("Follow following");
            lcd.setCursor(0, 1);
            lcd.print("Procedures: ");
            delay(3000);

            decide();
            waitForBlow();
            processAlcoholLevel();
        }
    }
}
    
void loop()
{
    if (engineUnlocked)
    {
        lcd.clear();

        bool vehicleRunning = false;

        while (engineUnlocked)
        {
            lcd.clear();
            lcd.print("Vehicle started");
            lcd.setCursor(0, 1);
            lcd.print("Drive safely");

            vehicleRunning = true;

            delay(3000);

            lcd.clear();

            lcd.print("Press D to turn");
            lcd.setCursor(0, 1);
            lcd.print("off engine");

            while(true)
            {
                char key = getKeyPressed_manual();

                if(key == 'D')
                {
                    digitalWrite(RELAY_PIN, HIGH);
                    noTone(BUZZER_PIN);
                    break;
                }
            }

            lcd.clear();
            lcd.print("Engine OFF");

            delay(2500);

            engineUnlocked = false;
            vehicleRunning = false;
            lockedAlcoholValue = -1;
        }

        lcd.clear();
        lcd.print("Restarting the");
        lcd.setCursor(0, 1);
        lcd.print("system...");

        delay(2500);

        decide();
        waitForBlow();
        processAlcoholLevel();
    }
    else if (endtask)
    {
        lcd.clear();
        lcd.print("Restarting the");
        lcd.setCursor(0, 1);
        lcd.print("system...");

        delay(2500);

        decide();
        waitForBlow();
        processAlcoholLevel();
    }
}