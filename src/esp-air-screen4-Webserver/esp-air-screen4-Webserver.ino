#include <Wire.h>  // include Wire library (required for I2C devices)
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>     // include Adafruit graphics library
#include <Adafruit_ST7735.h>  // include Adafruit ST7735 TFT library
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>

#include "bsec.h"
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include "SPIFFS.h"

#define TFT_RST 04   // TFT RST pin is connected to NodeMCU pin D4 (GPIO2)
#define TFT_CS 15    // 5 TFT CS  pin is connected to NodeMCU pin D3 (GPIO0)
#define TFT_DC 02    // TFT DC  pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_SCK 18   // TFT DC  pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_MOSI 19  // TFT DC  pin is connected to NodeMCU pin D2 (GPIO4)


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);
DynamicJsonDocument jsonDoc(200);  // Adjust the size as needed

//  ------------ debug / dev mode -------------------- //

bool debugOuts = true;

////////////////////////////////////////////////////////////

//BSEC
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

#define STATE_SAVE_PERIOD	UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day


// Create an object of the class Bsec
Bsec iaqSensor;
String output;

uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;


// Helper Vars
double temp_temperature;
double temp_humidity;
float temp_pressure;
float temp_IAQ;
String IAQ_text;
float temp_carbon;
float temp_VOC;
const char* IAQsts;
float gasResistance;

unsigned long time_trigger = 0;

// Helper functions declarations
void connectToWiFi(const String& ssid, const String& password);
bool readCredentials(String& ssid, String& password);
bool initializeSPIFFS(void);
void checkIaqSensorStatus(void);
void errLeds(void);
void loadState(void);
void updateState(void);

void checkIAQ(void);
void updateIAQStatus(int iaqLevel);
void displaySensorData(Adafruit_ST7735& tft);
void getDataAndUpdateDisplay(unsigned long time_trigger, Adafruit_ST7735& tft);
bool readBME680Data(unsigned long time_trigger);
void displayError(const char* errorMessage);
void printIAQStatus(const char* status);
void handle_data(void);
void createJsonObject(void);
String generateJsonResponse(void);

// Replace with your network credentials   ----  TODO add webserver fallback
const char* credentialsFile = "/wifiCredentials.txt";  // TODO this is broken because of spiffs error
const char* apSsid = "ESP32-Access-Point";             // Enter SSID here
const char* apPassword = "APPassword";                 // Enter Password here

WebServer server(80);

void setup() {
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);
  Serial.begin(115200);
  delay(1000);
  while (!Serial)
    ;  // TODO DELETE OR CHECK IF WORKS


  // initialize SPIFFS, read credentials, and connect to WiFi ------------------------add back
  // if (initializeSPIFFS()) {
  //   String ssid, password;
  //   if (readCredentials(ssid, password)) {
  //     connectToWiFi(ssid, password);
  //   }
  // }

  // move
  // Set up Access Point (AP) if connection to WiFi failed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed. Setting up Access Point...");
    WiFi.softAP(apSsid, apPassword);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }


  //--------------screen boot draw---------------------------
  Serial.println(F("Starting... screen"));
  tft.initR(INITR_BLACKTAB);     // initialize a ST7735S chip, black tab
  tft.fillScreen(ST7735_BLACK);  // fill screen with black color

  //--------------Sensor--------------------------------------
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();

  loadState();

  bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE
  };

  iaqSensor.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Print the header
  output = "Timestamp [ms], IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [hPa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
  Serial.println(output);

  //-----------Screen base draw-------------------------------//
  Serial.println("OLED begun");

  // Display labels and units
  tft.initR(INITR_BLACKTAB);                            // initialize a ST7735S chip, black tab
  tft.fillScreen(ST7735_BLACK);                         // fill screen with black color
  tft.setTextSize(1);                                   // text size = 2
  tft.drawFastHLine(0, 30, tft.width(), ST7735_WHITE);  // draw horizontal white line at position (0, 30)

  // draw display and data
  displaySensorData(tft);

  //------------------------WIFI START----------------------------------------------------
  server.on("/", handle_OnConnect);
  server.on("/data", handle_data);

  server.onNotFound(handle_NotFound);

  server.begin();

  Serial.println("HTTP server started");
}

void loop() {
  // put your main code here, to run repeatedly
  time_trigger = millis();
  server.handleClient();
  getDataAndUpdateDisplay(time_trigger, tft);
}

// Initializes SPIFFS and returns a boolean indicating success or failure.
bool initializeSPIFFS(void) {
  if (SPIFFS.begin()) {
    Serial.println("SPIFFS initialized.");
    return true;
  } else {
    Serial.println("Failed to initialize SPIFFS.");
    return false;
  }
}

// Reads SSID and password from the credentials file and returns a boolean indicating success or failure.
bool readCredentials(String& ssid, String& password) {
  File file = SPIFFS.open(credentialsFile, "r");
  if (file) {
    ssid = file.readStringUntil('\n');
    password = file.readStringUntil('\n');
    file.close();
    return true;
  } else {
    Serial.println("Failed to open credentials file.");
    return false;
  }
}

// Connects to WiFi using the provided SSID and password.
void connectToWiFi(const String& ssid, const String& password) {
  // Connect to WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Gets IAQ And update str IAQStatus
void checkIAQ() {
  int iaqValue = iaqSensor.staticIaq;
  updateIAQStatus(iaqValue);
}

void printIAQStatus(const char* status) {
  IAQsts = status;
  if (debugOuts) {
    Serial.print("IAQ: ");
    Serial.println(status);
  }
  tft.setCursor(8, 72);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("IAQStatus: ");
  tft.print(status);
}

// Updates str IAQStatus
void updateIAQStatus(int iaqLevel) {
  switch (iaqLevel) {
    case 0 ... 50:
      printIAQStatus("Good");
      break;

    case 51 ... 100:
      printIAQStatus("Average");
      break;

    case 101 ... 150:
      printIAQStatus("Little Bad");
      break;

    case 151 ... 200:
      printIAQStatus("Bad");
      break;

    case 201 ... 300:
      printIAQStatus("Worse");
      break;

    case 301 ... 500:
      printIAQStatus("Very Bad");
      break;

    default:
      printIAQStatus("Very Very Bad");
      break;
  }
}

// Helper function definitions
void checkIaqSensorStatus(void) {
  if (iaqSensor.bsecStatus != BSEC_OK) {
    if (iaqSensor.bsecStatus < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.bsecStatus);
      Serial.println(output);
      for (;;)     //The for (;;) construct is an infinite loop
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BSEC warning code : " + String(iaqSensor.bsecStatus);

      Serial.println(output);
    }
  }

  if (iaqSensor.bme68xStatus != BME68X_OK) {
    if (iaqSensor.bme68xStatus < BME68X_OK) {
      output = "BME68X error code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
      for (;;)     //The for (;;) construct is an infinite loop
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME68X warning code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
    }
  }
}

void errLeds(void) {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  Serial.println("output error fuk");
  delay(100);
  Serial.println("output error fuk2");
  delay(100);
  displayError("Could not get data, nothing to show");
}


void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM
    Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      Serial.println(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

void updateState(void)
{
  bool update = false;
  if (stateUpdateCounter == 0) {
    /* First state update when IAQ accuracy is >= 3 */
    if (iaqSensor.iaqAccuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD minutes */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
      EEPROM.write(i + 1, bsecState[i]);
      Serial.println(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}



void getDataAndUpdateDisplay(unsigned long time_trigger, Adafruit_ST7735& tft) {
  bool read = readBME680Data(time_trigger);
  if (read) {
    displaySensorData(tft);
  } else {
    // create a show error display func this need to happen after 5 -10 fails from sensor
    // Serial.println("could not get data so have nothin to show");
  }
}

bool readBME680Data(unsigned long time_trigger) {
  // Tell BME680 to begin measurement.
  if (iaqSensor.run()) {  // If new data is available
    if (debugOuts) {
      Serial.println("iaq, staticIaq, co2Equivalent, breathVocEquivalent, rawTemperature, rawHumidity, gasResistance, iaqAccuracy, pressure, stabStatus, runInStatus,  temperature, humidity, gasPercentage");
      output = String(time_trigger);
      output += ", " + String(iaqSensor.iaq);
      output += ", " + String(iaqSensor.staticIaq);
      output += ", " + String(iaqSensor.co2Equivalent);
      output += ", " + String(iaqSensor.breathVocEquivalent);
      output += ", " + String(iaqSensor.rawTemperature);
      output += ", " + String(iaqSensor.rawHumidity);
      output += ", " + String(iaqSensor.gasResistance);
      output += ", " + String(iaqSensor.iaqAccuracy);
      output += ", " + String(iaqSensor.pressure);
      output += ", " + String(iaqSensor.stabStatus);
      output += ", " + String(iaqSensor.runInStatus);
      output += ", " + String(iaqSensor.temperature);
      output += ", " + String(iaqSensor.humidity);
      output += ", " + String(iaqSensor.gasPercentage);
      Serial.println(output);
    }

    temp_temperature = iaqSensor.temperature;
    temp_humidity = iaqSensor.humidity;
    temp_pressure = (iaqSensor.pressure) / 100.0;
    temp_IAQ = iaqSensor.staticIaq;
    temp_carbon = iaqSensor.co2Equivalent;
    temp_VOC = iaqSensor.breathVocEquivalent;
    gasResistance = (iaqSensor.gasResistance) / 1000.0;

    checkIAQ();
    updateState();
    return true;
  } else {
    checkIaqSensorStatus();
    return false;
  }
}

void displayError(const char* errorMessage) {
  Serial.println(errorMessage);
  // TODO Add code to display the error on the screen if needed
}

void displaySensorData(Adafruit_ST7735& tft) {
  // tft.fillScreen(ST7735_BLACK);  // Clear the screen

  // Display Temperature
  tft.setTextColor(ST7735_RED, ST7735_BLACK);  // set text color to red and black background
  tft.setCursor(8, 36);
  tft.print("Temperature: ");
  tft.print(temp_temperature);

  // Display Humidity
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);  // set text color to cyan and black background
  tft.setCursor(8, 48);
  tft.print("Humidity: ");
  tft.print(temp_humidity);
  tft.println(" %");

  // Display IAQ
  tft.setCursor(8, 60);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("IAQ: ");
  tft.print(temp_IAQ);
  tft.println(" PPM");

  // IAQStatus (8, 72)

  // Display VOC IAQ
  tft.setCursor(8, 84);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("Breath VOC: ");
  tft.print(temp_VOC);
  tft.println(" PPM");

  // Display CO2 Equivalent
  tft.setCursor(8, 96);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("CO2eq: ");
  tft.print(temp_carbon);
  tft.println(" PPM");

  // Display Pressure
  tft.setCursor(8, 108);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("Pressure: ");
  tft.print(temp_pressure);
  tft.println(" hPa");

  // Display gasres
  tft.setCursor(8, 120);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("GasRes: ");
  tft.print(gasResistance);
  tft.println(" kOhm");
}

void createJsonObject() {
  jsonDoc["temperature"] = temp_temperature;
  jsonDoc["humidity"] = temp_humidity;
  jsonDoc["pressure"] = temp_pressure;
  jsonDoc["IAQ"] = temp_IAQ;
  jsonDoc["IAQ_text"] = IAQsts;
  jsonDoc["carbon"] = temp_carbon;
  jsonDoc["VOC"] = temp_VOC;
  jsonDoc["gasResistance"] = gasResistance;
}

String generateJsonResponse() {
  String jsonResponse;
  createJsonObject();  // Populate the JSON object

  // Serialize the JSON object to a string
  serializeJson(jsonDoc, jsonResponse);

  return jsonResponse;
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(temp_temperature, temp_humidity, temp_pressure, temp_IAQ, temp_carbon, temp_VOC, IAQsts));
}
void handle_data() {
  server.send(200, "application/json", generateJsonResponse());
}
void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature, float humidity, float pressure, float IAQ, float carbon, float VOC, const char* IAQsts) {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<title>BME680 Webserver</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.7.2/css/all.min.css'>";
  html += "<link rel='stylesheet' type='text/css' href='styles.css'>";
  html += "<style>";
  html += "body { background-color: #fff; font-family: sans-serif; color: #333333; font: 12px Helvetica, sans-serif box-sizing: border-box;}";
  html += "#page { margin: 18px; background-color: #fff;}";
  html += ".container { height: inherit; padding-bottom: 18px;}";
  html += ".header { padding: 18px;}";
  html += ".header h1 { padding-bottom: 0.3em; color: #f4a201; font-size: 25px; font-weight: bold; font-family: Garmond, 'sans-serif'; text-align: center;}";
  html += "h2 { padding-bottom: 0.2em; border-bottom: 1px solid #eee; margin: 2px; text-align: center;}";
  html += ".box-full { padding: 18px; border 1px solid #ddd; border-radius: 1em 1em 1em 1em; box-shadow: 1px 7px 7px 1px rgba(0,0,0,0.4); background: #fff; margin: 18px; width: 300px;}";
  html += "@media (max-width: 494px) { #page { width: inherit; margin: 5px auto; } #content { padding: 1px;} .box-full { margin: 8px 8px 12px 8px; padding: 10px; width: inherit;; float: none; } }";
  html += "@media (min-width: 494px) and (max-width: 980px) { #page { width: 465px; margin 0 auto; } .box-full { width: 380px; } }";
  html += "@media (min-width: 980px) { #page { width: 930px; margin: auto; } }";
  html += ".sensor { margin: 10px 0px; font-size: 2.5rem;}";
  html += ".sensor-labels { font-size: 1rem; vertical-align: middle; padding-bottom: 15px;}";
  html += ".units { font-size: 1.2rem;}";
  html += "hr { height: 1px; color: #eee; background-color: #eee; border: none;}";
  html += "</style>";

  //Ajax Code Start
  html += "<script>\n";
  html += "setInterval(loadDoc,1000);\n";
  html += "function loadDoc() {\n";
  html += "var xhttp = new XMLHttpRequest();\n";
  html += "xhttp.onreadystatechange = function() {\n";
  html += "if (this.readyState == 4 && this.status == 200) {\n";
  html += "document.body.innerHTML =this.responseText}\n";
  html += "};\n";
  html += "xhttp.open(\"GET\", \"/\", true);\n";
  html += "xhttp.send();\n";
  html += "}\n";
  html += "</script>\n";
  //Ajax Code END

  html += "</head>";
  html += "<body>";
  html += "<div id='page'>";
  html += "<div class='header'>";
  html += "<h1>BME680 IAQ Monitoring System</h1>";
  html += "</div>";
  html += "<div id='content' align='center'>";
  html += "<div class='box-full' align='left'>";
  html += "<h2>";
  html += "IAQ Status: ";
  html += IAQsts;
  html += "</h2>";
  html += "<div class='sensors-container'>";

  //For Temperature
  html += "<div class='sensors'>";
  html += "<p class='sensor'>";
  html += "<i class='fas fa-thermometer-half' style='color:#0275d8'></i>";
  html += "<span class='sensor-labels'> Temperature </span>";
  html += temp_temperature;
  html += "<sup class='units'>°C</sup>";
  html += "</p>";
  html += "<hr>";
  html += "</div>";

  //For Humidity
  html += "<p class='sensor'>";
  html += "<i class='fas fa-tint' style='color:#0275d8'></i>";
  html += "<span class='sensor-labels'> Humidity </span>";
  html += temp_humidity;
  html += "<sup class='units'>%</sup>";
  html += "</p>";
  html += "<hr>";

  //For Pressure
  html += "<p class='sensor'>";
  html += "<i class='fas fa-tachometer-alt' style='color:#ff0040'></i>";
  html += "<span class='sensor-labels'> Pressure </span>";
  html += temp_pressure;
  html += "<sup class='units'>hPa</sup>";
  html += "</p>";
  html += "<hr>";

  //For VOC IAQ
  html += "<div class='sensors'>";
  html += "<p class='sensor'>";
  html += "<i class='fab fa-cloudversify' style='color:#483d8b'></i>";
  html += "<span class='sensor-labels'> IAQ </span>";
  html += temp_IAQ;
  html += "<sup class='units'>PPM</sup>";
  html += "</p>";
  html += "<hr>";

  //For C02 Equivalent
  html += "<p class='sensor'>";
  html += "<i class='fas fa-smog' style='color:#35b22d'></i>";
  html += "<span class='sensor-labels'> Co2 Eq. </span>";
  html += temp_carbon;
  html += "<sup class='units'>PPM</sup>";
  html += "</p>";
  html += "<hr>";

  //For Breath VOC
  html += "<p class='sensor'>";
  html += "<i class='fas fa-wind' style='color:#0275d8'></i>";
  html += "<span class='sensor-labels'> Breath VOC </span>";
  html += temp_VOC;
  html += "<sup class='units'>PPM</sup>";
  html += "</p>";


  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  return html;
}
