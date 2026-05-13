#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "credentials.h"

// Redis
#include <Redis.h>
#include <RedisInternal.h>

//Libraries for microSD card
#include "FS.h"
#include <SD.h>
#include "SPI.h"

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define buttonPin 4       // Pushbutton

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Wi-Fi
bool WiFi_Connection;

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 7200;

// Redis
WiFiClient redis;

Adafruit_BME680 bme;  // I2C

const char* PARAM_INPUT_1 = "seaLevelPressure";
float seaLevelPressure = 1013.2472;

float alt;
float temperature;
float humidity;
float pressure;
float gasResistance;

int buttonState;            // current reading from the input pin
int lastButtonState = LOW;  // previous reading from the input pin

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time

// Screens
int displayScreenNum = 0;
int displayScreenNumMax = 4;

unsigned long lastTime = 0;
unsigned long timerDelay = 20000;  // send readings timer

unsigned long send_time;

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned char temperature_icon[] = {
  0b00000001, 0b11000000,  //        ###
  0b00000011, 0b11100000,  //       #####
  0b00000111, 0b00100000,  //      ###  #
  0b00000111, 0b11100000,  //      ######
  0b00000111, 0b00100000,  //      ###  #
  0b00000111, 0b11100000,  //      ######
  0b00000111, 0b00100000,  //      ###  #
  0b00000111, 0b11100000,  //      ######
  0b00000111, 0b00100000,  //      ###  #
  0b00001111, 0b11110000,  //     ########
  0b00011111, 0b11111000,  //    ##########
  0b00011111, 0b11111000,  //    ##########
  0b00011111, 0b11111000,  //    ##########
  0b00011111, 0b11111000,  //    ##########
  0b00001111, 0b11110000,  //     ########
  0b00000111, 0b11100000,  //      ######
};

unsigned char humidity_icon[] = {
  0b00000000, 0b00000000,  //
  0b00000001, 0b10000000,  //        ##
  0b00000011, 0b11000000,  //       ####
  0b00000111, 0b11100000,  //      ######
  0b00001111, 0b11110000,  //     ########
  0b00001111, 0b11110000,  //     ########
  0b00011111, 0b11111000,  //    ##########
  0b00011111, 0b11011000,  //    ####### ##
  0b00111111, 0b10011100,  //   #######  ###
  0b00111111, 0b10011100,  //   #######  ###
  0b00111111, 0b00011100,  //   ######   ###
  0b00011110, 0b00111000,  //    ####   ###
  0b00011111, 0b11111000,  //    ##########
  0b00001111, 0b11110000,  //     ########
  0b00000011, 0b11000000,  //       ####
  0b00000000, 0b00000000,  //
};

unsigned char arrow_down_icon[] = {
  0b00001111, 0b11110000,  //     ########
  0b00011111, 0b11111000,  //    ##########
  0b00011111, 0b11111000,  //    ##########
  0b00011100, 0b00111000,  //    ###    ###
  0b00011100, 0b00111000,  //    ###    ###
  0b00011100, 0b00111000,  //    ###    ###
  0b01111100, 0b00111110,  //  #####    #####
  0b11111100, 0b00111111,  // ######    ######
  0b11111100, 0b00111111,  // ######    ######
  0b01110000, 0b00001110,  //  ####      ####
  0b00111000, 0b00011100,  //   ####    ####
  0b00011100, 0b00111000,  //    ####  ####
  0b00001110, 0b01110000,  //     ########
  0b00000111, 0b11100000,  //      ######
  0b00000011, 0b11000000,  //       ####
  0b00000001, 0b10000000,  //        ##
};

unsigned char wind_icon[] = {
  0b00000000, 0b01111000,  //
  0b00000000, 0b11001100,  //
  0b00000000, 0b11001100,  //
  0b00000000, 0b00001100,  //
  0b00000000, 0b00001100,  //
  0b01111111, 0b11111000,  //
  0b00000000, 0b00000000,  //
  0b00011111, 0b11111110,  //
  0b00000000, 0b00000111,  //
  0b00000000, 0b00110011,  //
  0b01111111, 0b10110011,  //
  0b00000000, 0b11011111,  //
  0b00011000, 0b11001110,  //
  0b00011000, 0b11000000,  //
  0b00001111, 0b10000000,  //
  0b00000000, 0b00000000,  //
};

// Create display marker for each screen
void displayIndicator(int displayNumber) {
  int xCoordinates[5] = { 44, 54, 64, 74, 84 };
  for (int i = 0; i < 5; i++) {
    if (i == displayNumber) {
      display.fillCircle(xCoordinates[i], 60, 2, WHITE);
    } else {
      display.drawCircle(xCoordinates[i], 60, 2, WHITE);
    }
  }
}

struct tm timeinfo;
//SCREEN NUMBER 0: DATE AND TIME
void displayLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  //GET DATE
  //Get full weekday name
  char weekDay[10];
  strftime(weekDay, sizeof(weekDay), "%a", &timeinfo);
  //Get day of month
  char dayMonth[4];
  strftime(dayMonth, sizeof(dayMonth), "%d", &timeinfo);
  //Get abbreviated month name
  char monthName[5];
  strftime(monthName, sizeof(monthName), "%b", &timeinfo);
  //Get year
  char year[6];
  strftime(year, sizeof(year), "%Y", &timeinfo);

  //Get hour (24 hour format)
  char hour[4];
  strftime(hour, sizeof(hour), "%H", &timeinfo);
  //Get minute
  char minute[4];
  strftime(minute, sizeof(minute), "%M", &timeinfo);

  //Display Date and Time on OLED display
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(19, 5);
  display.print(hour);
  display.print(":");
  display.print(minute);
  display.setTextSize(1);
  display.setCursor(16, 40);
  display.print(weekDay);
  display.print(", ");
  display.print(dayMonth);
  display.print(" ");
  display.print(monthName);
  display.print(" ");
  display.print(year);
  displayIndicator(displayScreenNum);
  display.display();
}
// SCREEN NUMBER 1: TEMPERATURE
void displayTemperature() {
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(15, 5, temperature_icon, 16, 16, 1);
  display.setCursor(35, 5);
  display.print(bme.temperature);
  display.cp437(true);
  display.setTextSize(1);
  display.print(" ");
  display.write(167);
  display.print("C");
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Humidity: ");
  display.print(bme.humidity);
  display.print(" %");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Pressure: ");
  display.print(bme.pressure / 100.0 * 0.75006375541921);
  display.print(" mmHg");
  displayIndicator(displayScreenNum);
  display.display();
}
// SCREEN NUMBER 2: HUMIDITY
void displayHumidity() {
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(15, 5, humidity_icon, 16, 16, 1);
  display.setCursor(35, 5);
  display.print(bme.humidity);
  display.print(" %");
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Temperature: ");
  display.print(bme.temperature);
  display.cp437(true);
  display.print(" ");
  display.write(167);
  display.print("C");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Pressure: ");
  display.print(bme.pressure / 100.0 * 0.75006375541921);
  display.print(" mmHg");
  displayIndicator(displayScreenNum);
  display.display();
}
// SCREEN NUMBER 3: PRESSURE
void displayPressure() {
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(5, 5, arrow_down_icon, 16, 16, 1);
  display.setCursor(25, 5);
  display.print(bme.pressure / 100.0 * 0.75006375541921);
  display.setTextSize(1);
  display.print(" mmHg");
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Temperature: ");
  display.print(bme.temperature);
  display.cp437(true);
  display.print(" ");
  display.write(167);
  display.print("C");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Altitude: ");
  display.print(bme.readAltitude(seaLevelPressure));
  display.print(" m");
  displayIndicator(displayScreenNum);
  display.display();
}
// SCREEN NUMBER 4: gas resistance
void displayGas() {
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(10, 5, wind_icon, 16, 16, 1);
  display.setCursor(31, 5);
  display.print(bme.gas_resistance / 1000.0);
  display.setTextSize(1);
  display.print(" KOm");
  display.setCursor(0, 34);
  display.print("Temperature: ");
  display.print(bme.temperature);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.print("C");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Humidity: ");
  display.print(bme.humidity);
  display.print(" %");
  display.setCursor(0, 44);
  displayIndicator(displayScreenNum);
  display.display();
}

// Display the right screen accordingly to the displayScreenNum
void updateScreen() {
  if (displayScreenNum == 0) {
    displayLocalTime();
  } else if (displayScreenNum == 1) {
    displayTemperature();
  } else if (displayScreenNum == 2) {
    displayHumidity();
  } else if (displayScreenNum == 3) {
    displayPressure();
  } else {
    displayGas();
  }
}

// Write to the SD card
void writeFile(fs::FS& fs, const char* path, const char* message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card
void appendFile(fs::FS& fs, const char* path, const char* message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
  } else if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void setupWiFi() {
  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_STA);

  send_time = millis();
  WiFi_Connection = false;

  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    Serial.println("Setting as a Wi-Fi Station..");
    display.println("Setting as a Wi-Fi Station..");
    display.display();
    delay(1000);
    if ((millis() - send_time) >= 120000) break;
  }

  if (WiFi.status() == WL_CONNECTED) {

    WiFi_Connection = true;
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    display.clearDisplay();
    display.println("Station IP Address: ");
    display.println();
    display.println(WiFi.localIP());
    display.display();
    delay(5000);
  }
}

void send_data() {

  getBME680Readings();
  send_time = millis();
  while (!SD.exists("/")) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("microSD card not found. Check microSD");
    display.display();
    Serial.println("microSD card not found. Check microSD");
    if ((millis() - send_time) >= 30000) ESP.restart();
    delay(1000);
  }

  String dataMessage = "DATE: " + String(timeinfo.tm_mday) + "." + String(timeinfo.tm_mon + 1) + "." + String(timeinfo.tm_year + 1900) + " TIME: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec) + " Temperature: " + String(temperature) + " \u2103 , Humidity: " + String(humidity) + " % , Pressure: " + String(pressure) + " mmHg , Gas Resistance: " + String(gasResistance) + " KOm , Altitude: " + String(alt) + " m. \r\n";
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());

  // Send Events to the Web Server with the Sensor Readings
  events.send("ping", NULL, millis());
  events.send(String(temperature).c_str(), "temperature", millis());
  events.send(String(humidity).c_str(), "humidity", millis());
  events.send(String(pressure).c_str(), "pressure", millis());
  events.send(String(gasResistance).c_str(), "gas", millis());
  events.send(String(alt).c_str(), "altitude", millis());

  Serial.print("redis\n");

  if (WiFi.status() != WL_CONNECTED && WiFi_Connection) {
    setupWiFi();
  }

  String key = "temperature";
  redis.print(String("*3\r\n") + "$5\r\n" + "LPUSH\r\n" + "$" + key.length() + "\r\n" + key + "\r\n" + "$" + String(temperature).length() + "\r\n" + temperature + "\r\n");
  key = "humidity";
  redis.print(String("*3\r\n") + "$5\r\n" + "LPUSH\r\n" + "$" + key.length() + "\r\n" + key + "\r\n" + "$" + String(humidity).length() + "\r\n" + humidity + "\r\n");
  key = "pressure";
  redis.print(String("*3\r\n") + "$5\r\n" + "LPUSH\r\n" + "$" + key.length() + "\r\n" + key + "\r\n" + "$" + String(pressure).length() + "\r\n" + pressure + "\r\n");
  key = "gasResistance";
  redis.print(String("*3\r\n") + "$5\r\n" + "LPUSH\r\n" + "$" + key.length() + "\r\n" + key + "\r\n" + "$" + String(gasResistance).length() + "\r\n" + gasResistance + "\r\n");

  // Waiting for a response from Redis
  while (redis.available() != 0)
    Serial.print((char)redis.read());
  delay(1000);
}

void getBME680Readings() {
  // Tell BME680 to begin measurement.
  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }
  temperature = bme.temperature;
  pressure = bme.pressure / 100.0 * 0.75006375541921;
  humidity = bme.humidity;
  gasResistance = bme.gas_resistance / 1000.0;
  alt = bme.readAltitude(seaLevelPressure);
}

String processor(const String& var) {
  getBME680Readings();
  //Serial.println(var);
  if (var == "TEMPERATURE") {
    return String(temperature);
  } else if (var == "HUMIDITY") {
    return String(humidity);
  } else if (var == "PRESSURE") {
    return String(pressure);
  } else if (var == "GAS") {
    return String(gasResistance);
  } else if (var == "ALTITUDE") {
    return String(alt);
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
  <title>BME680 Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"
    integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      text-align: center;
    }

    p {
      font-size: 1.2rem;
    }

    body {
      margin: 0;
    }

    .topnav {
      overflow: hidden;
      background-color: #4B1D3F;
      color: white;
      font-size: 1.7rem;
    }

    .content {
      padding: 20px;
    }

    .card {
      background-color: white;
      box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);
    }

    .cards {
      max-width: 700px;
      margin: 0 auto;
      display: grid;
      grid-gap: 2rem;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    }

    .reading {
      font-size: 2.8rem;
    }

    .card.temperature {
      color: #0e7c7b;
    }

    .card.humidity {
      color: #17bebb;
    }

    .card.pressure {
      color: #e8c917;
    }

    .card.gas {
      color: #d62246;
    }

    button {
      border-radius: 12px;
      background-color: #008CBA;
      color: white;
      font-size: 18px;
    }

    form {
      display: flex;
      justify-content: center;
      align-items: center;
      flex-direction: row;
    }

    input{
      margin: 0 5px;
    }

  </style>
</head>
<body>
  <div class="topnav">
    <h3>BME680 WEB SERVER</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATURE</h4>
        <p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> HUMIDITY</h4>
        <p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card pressure">
        <h4><i class="fas fa-angle-double-down"></i> PRESSURE</h4>
        <p><span class="reading"><span id="pres">%PRESSURE%</span> mmHg</span></p>
      </div>
      <div class="card gas">
        <h4><i class="fas fa-wind"></i> GAS</h4>
        <p><span class="reading"><span id="gas">%GAS%</span> K&ohm;</span></p>
      </div>
      <div class="card alt">
        <h4><i class="fa-solid fa-up-down"></i></i> ALTITUDE</h4>
        <p><span class="reading"><span id="alt">%ALTITUDE%</span> m</span></p>
        <form action="/get" method="post">
          Sea Level Pressure: <input type="float" name="seaLevelPressure">
          <input type="submit" value="Submit">
        </form>
      </div>
    </div>
  </div>
  <script>
    if (!!window.EventSource) {
      var source = new EventSource('/events');

      source.addEventListener('open', function (e) {
        console.log("Events Connected");
      }, false);
      source.addEventListener('error', function (e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
      }, false);

      source.addEventListener('message', function (e) {
        console.log("message", e.data);
      }, false);

      source.addEventListener('temperature', function (e) {
        console.log("temperature", e.data);
        document.getElementById("temp").innerHTML = e.data;
      }, false);

      source.addEventListener('humidity', function (e) {
        console.log("humidity", e.data);
        document.getElementById("hum").innerHTML = e.data;
      }, false);

      source.addEventListener('pressure', function (e) {
        console.log("pressure", e.data);
        document.getElementById("pres").innerHTML = e.data;
      }, false);

      source.addEventListener('gas', function (e) {
        console.log("gas", e.data);
        document.getElementById("gas").innerHTML = e.data;
      }, false);

      source.addEventListener('alt', function (e) {
        console.log("alt", e.data);
        document.getElementById("alt").innerHTML = e.data;
      }, false);
    }
  </script>
</body>
</html>)rawliteral";


void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  send_time = millis();

  //Initialize SD card
  while (!SD.begin()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Card Mount Failed");
    Serial.println("Card Mount Failed");
    display.display();
    delay(1000);
    if ((millis() - send_time) >= 30000) ESP.restart();
  }
  send_time = 0;

  setupWiFi();

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Init BME680 sensor
  while (!bme.begin()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    display.println();
    display.println(F("Could not find a valid BME680 sensor, check wiring!"));
    display.display();
    delay(2000);
  }
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320*C for 150 ms


  // Connect to Redis
  if (!redis.connected()) {
    Serial.print("Redis not connected, connecting...");
    if (!redis.connect(redisHost, redisPort)) {
      Serial.print("Redis connection failed...");
      Serial.println("Waiting for next read");
      return;
    } else
      Serial.println("OK");
  }

  redis.print(String("*2\r\n") + "$4\r\n" + "AUTH\r\n" + "$" + String(redisPassword).length() + "\r\n" + String(redisPassword) + "\r\n");

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  server.on("/get", HTTP_POST, [](AsyncWebServerRequest* request) {
    String inputMessage;
    if (request->hasParam(PARAM_INPUT_1, true)) {
      inputMessage = request->getParam(PARAM_INPUT_1, true)->value();
      seaLevelPressure = inputMessage.toFloat();
      request->redirect("/");
    } else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
  });

  server.addHandler(&events);
  server.begin();
}

void loop() {
  // // read the state of the switch into a local variable
  int reading = digitalRead(buttonPin);

  // Change screen when the pushbutton is pressed
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        updateScreen();
        Serial.println(displayScreenNum);
        if (displayScreenNum < displayScreenNumMax) {
          displayScreenNum++;
        } else {
          displayScreenNum = 0;
        }
        lastTime = millis();
      }
    }
  }
  lastButtonState = reading;

  // Change screen every 20 seconds (timerDelay variable)
  if ((millis() - lastTime) > timerDelay) {
    updateScreen();
    Serial.println(displayScreenNum);
    if (displayScreenNum < displayScreenNumMax) {
      displayScreenNum++;
    } else {
      displayScreenNum = 0;
    }
    lastTime = millis();
  }
  if ((millis() - send_time) >= 30000) {

    getLocalTime(&timeinfo);
    send_data();
    send_time = millis();
  }
}