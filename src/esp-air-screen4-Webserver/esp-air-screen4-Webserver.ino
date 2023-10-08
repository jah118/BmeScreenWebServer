#include <Wire.h>  // include Wire library (required for I2C devices)
#include <SPI.h>

// #include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>  // include Adafruit graphics library
// #include "Adafruit_BME680.h"
#include <Adafruit_ST7735.h>  // include Adafruit ST7735 TFT library
#include <Adafruit_Sensor.h>

#include "bsec.h"
#include <WiFi.h>
#include <WebServer.h>

#define TFT_RST 04   // TFT RST pin is connected to NodeMCU pin D4 (GPIO2)
#define TFT_CS 15    // 5 TFT CS  pin is connected to NodeMCU pin D3 (GPIO0)
#define TFT_DC 02    // TFT DC  pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_SCK 18   // TFT DC  pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_MOSI 19  // TFT DC  pin is connected to NodeMCU pin D2 (GPIO4)
// #define BME680_I2C_ADDRESS 0x76

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

//  ------------ debug / dev mode -------------------- //

bool debugOuts = true;

////////////////////////////////////////////////////////////


// Create an object of the class Bsec
Bsec iaqSensor;
String output;



// Helper Vars
float temp_temperature;
float temp_humidity;
float temp_pressure;
float temp_IAQ;
String IAQ_text;
float temp_carbon;
float temp_VOC;
const char* IAQsts;


// Helper functions declarations
void checkIaqSensorStatus(void);
void errLeds(void);
void checkIAQ(void);
void updateIAQStatus(int iaqLevel);
void displaySensorData(Adafruit_ST7735 &tft);


// Replace with your network credentials   ----  TODO add webserver fallback
const char* credentialsFile = "/credentials.txt";
const char* apSsid = "ESP32-Access-Point"; // Enter SSID here
const char* apPassword = "APPassword";   //Enter Password here 



WebServer server(80);
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;  // send readings timer

// Create an object of the class Bsec


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);
  while (!Serial)
    ;  // TODO DELETE OR CHECK IF WORKS

  // Wire.begin(21, 22);  // set I2C pins [SDA = D3, SCL = D2], default clock is 100kHz
  //                      // Wire.begin();   should not be neeeded

  //--------------screen boot draw---------------------------
  Serial.println(F("Starting... screen"));
  tft.initR(INITR_BLACKTAB);     // initialize a ST7735S chip, black tab
  tft.fillScreen(ST7735_BLACK);  // fill screen with black color

  //--------------Sensor--------------------------------------
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();


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

    // Display sensor data
  //displaySensorData(tft);

  // Display labels and units
  tft.setTextSize(2);  // text size = 2

  tft.setTextColor(ST7735_RED, ST7735_BLACK);  // set text color to red and black background
  tft.setCursor(25, 39);
  tft.print("TEMPERATURE =");

  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);  // set text color to cyan and black background
  tft.setCursor(34, 85);
  tft.print("HUMIDITY =");

  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);  // set text color to green and black background
  tft.setCursor(34, 131);
  tft.print("CO2 =");

  // Print degree symbol (°C)
  tft.drawCircle(89, 56, 2, ST7735_YELLOW);
  tft.setCursor(95, 54);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("C");

  // ##########  OLD ##############
  // tft.drawFastHLine(0, 30, tft.width(), ST7735_WHITE);  // draw horizontal white line at position (0, 30)

  // tft.setTextColor(ST7735_WHITE, ST7735_BLACK);  // set text color to white and black background
  // tft.setTextSize(1);                            // text size = 1
  // tft.setCursor(4, 0);                           // move cursor to position (4, 0) pixel
  // tft.print("AIR");
  // tft.setCursor(19, 15);  // move cursor to position (19, 15) pixel
  // tft.print("STATION");


  // // display.display();
  // delay(100);
  // // tft.drawFastHLine(0, 76, tft.width(), ST7735_WHITE);   // draw horizontal white line at position (0, 76)
  // // tft.drawFastHLine(0, 122, tft.width(), ST7735_WHITE);  // draw horizontal white line at position (0, 122)
  // // tft.setTextColor(ST7735_RED, ST7735_BLACK);            // set text color to red and black background

  // tft.setCursor(25, 39);  // move cursor to position (25, 39) pixel
  // tft.print("TEMPERATURE =");
  // tft.setTextColor(ST7735_CYAN, ST7735_BLACK);  // set text color to cyan and black background
  // tft.setCursor(34, 85);                        // move cursor to position (34, 85) pixel
  // tft.print("HUMIDITY =");
  // tft.setTextColor(ST7735_GREEN, ST7735_BLACK);  // set text color to green and black background
  // tft.setCursor(34, 131);                        // move cursor to position (34, 131) pixel
  // tft.print("co2 =");
  // tft.setTextSize(2);  // text size = 2
  // // print °C
  // tft.drawCircle(89, 56, 2, ST7735_YELLOW);       // print degree symbol ( ° )
  // tft.setCursor(95, 54);                          // move cursor to position (95, 54) pixel
  // tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);  // set text color to yellow and black background
  // tft.print("C");



  //------------------------WIFI START----------------------------------------------------
  Serial.println("Connecting to ");
  Serial.println(ssid);


  //Connect to your local wi-fi network
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());


  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();

  Serial.println("HTTP server started");
}

void loop() {
  // put your main code here, to run repeatedly
  unsigned long time_trigger = millis();
  server.handleClient();
  getBME680Readings(time_trigger);
  displaySensorData(tft); // This should maybe only be called if getBME680Readings get reading, for better handlng of bad data ....
}

void checkIAQ() {
  int iaqValue = iaqSensor.staticIaq;
  updateIAQStatus(iaqValue);
}

void updateIAQStatus(int iaqLevel) {
  switch (iaqLevel) {
    case 0 ... 50:
      IAQsts = "Good";
      Serial.print("IAQ: Good");
      tft.print("IAQ: Good");
      break;

    case 51 ... 100:
      IAQsts = "Average";
      Serial.print("IAQ: Average");
      tft.print("IAQ: Average");
      break;

    case 101 ... 150:
      IAQsts = "Little Bad";
      Serial.print("IAQ: Little Bad");
      tft.print("IAQ: Little Bad");
      break;

    case 151 ... 200:
      IAQsts = "Bad";
      Serial.print("IAQ: Bad");
      tft.print("IAQ: Bad");
      break;

    case 201 ... 300:
      IAQsts = "Worse";
      Serial.print("IAQ: Worse");
      tft.print("IAQ: Worse");
      break;

    case 301 ... 500:
      IAQsts = "Very Bad";
      Serial.print("IAQ: Very Bad");
      tft.print("IAQ: Very Bad");
      break;

    default:
      IAQsts = "Very Very Bad";
      Serial.print("IAQ: Very Very Bad");
      tft.print("IAQ: Very Very Bad");
      break;
  }
}

// Helper function definitions
void checkIaqSensorStatus(void) {
  if (iaqSensor.bsecStatus != BSEC_OK) {
    if (iaqSensor.bsecStatus < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.bsecStatus);
      Serial.println(output);
      for (;;)
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
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME68X warning code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
    }
  }
}

void errLeds(void) {
  Serial.println("output error fuk");
  delay(100);
  Serial.println("output error fuk2");
  delay(100);
}

void getBME680Readings(unsigned long time_trigger) {
  // Tell BME680 to begin measurement.
  if (iaqSensor.run()) {  // If new data is available

    if (debugOuts) {
      output = String(time_trigger);
      output += ", " + String(iaqSensor.iaq);
      output += ", " + String(iaqSensor.iaqAccuracy);
      output += ", " + String(iaqSensor.staticIaq);
      output += ", " + String(iaqSensor.co2Equivalent);
      output += ", " + String(iaqSensor.breathVocEquivalent);
      output += ", " + String(iaqSensor.rawTemperature);
      output += ", " + String(iaqSensor.pressure);
      output += ", " + String(iaqSensor.rawHumidity);
      output += ", " + String(iaqSensor.gasResistance);
      output += ", " + String(iaqSensor.stabStatus);
      output += ", " + String(iaqSensor.runInStatus);
      output += ", " + String(iaqSensor.temperature);
      output += ", " + String(iaqSensor.humidity);
      output += ", " + String(iaqSensor.gasPercentage);
      Serial.println(output);
    }

    temp_temperature = iaqSensor.temperature;
    temp_humidity = iaqSensor.humidity;
    temp_pressure = iaqSensor.pressure / 100.0;
    temp_IAQ = iaqSensor.staticIaq;
    temp_carbon = iaqSensor.co2Equivalent;
    temp_VOC = iaqSensor.breathVocEquivalent;

    checkIAQ();

  } else {
    checkIaqSensorStatus();
  }

}

void displaySensorData(Adafruit_ST7735 &tft) {
  tft.fillScreen(ST7735_BLACK);  // Clear the screen

  // Display Temperature
  tft.setCursor(11, 10);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("Temperature: ");
  tft.print(iaqSensor.temperature);
  tft.println(" *C");

  // Display Humidity
  tft.setCursor(11, 30);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("Humidity: ");
  tft.print(iaqSensor.humidity);
  tft.println(" %");

  // Display IAQ
  tft.setCursor(11, 50);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("IAQ: ");
  tft.print(iaqSensor.staticIaq);
  tft.println(" PPM");

  // Display CO2 Equivalent
  tft.setCursor(11, 70);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("CO2eq: ");
  tft.print(iaqSensor.co2Equivalent);
  tft.println(" PPM");
}


void handle_OnConnect() {



  server.send(200, "text/html", SendHTML(temp_temperature, temp_humidity, temp_pressure, temp_IAQ, temp_carbon, temp_VOC, IAQsts));
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
  html += temperature;
  html += "<sup class='units'>°C</sup>";
  html += "</p>";
  html += "<hr>";
  html += "</div>";

  //For Humidity
  html += "<p class='sensor'>";
  html += "<i class='fas fa-tint' style='color:#0275d8'></i>";
  html += "<span class='sensor-labels'> Humidity </span>";
  html += humidity;
  html += "<sup class='units'>%</sup>";
  html += "</p>";
  html += "<hr>";

  //For Pressure
  html += "<p class='sensor'>";
  html += "<i class='fas fa-tachometer-alt' style='color:#ff0040'></i>";
  html += "<span class='sensor-labels'> Pressure </span>";
  html += pressure;
  html += "<sup class='units'>hPa</sup>";
  html += "</p>";
  html += "<hr>";

  //For VOC IAQ
  html += "<div class='sensors'>";
  html += "<p class='sensor'>";
  html += "<i class='fab fa-cloudversify' style='color:#483d8b'></i>";
  html += "<span class='sensor-labels'> IAQ </span>";
  html += IAQ;
  html += "<sup class='units'>PPM</sup>";
  html += "</p>";
  html += "<hr>";

  //For C02 Equivalent
  html += "<p class='sensor'>";
  html += "<i class='fas fa-smog' style='color:#35b22d'></i>";
  html += "<span class='sensor-labels'> Co2 Eq. </span>";
  html += carbon;
  html += "<sup class='units'>PPM</sup>";
  html += "</p>";
  html += "<hr>";

  //For Breath VOC
  html += "<p class='sensor'>";
  html += "<i class='fas fa-wind' style='color:#0275d8'></i>";
  html += "<span class='sensor-labels'> Breath VOC </span>";
  html += VOC;
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
