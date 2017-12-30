#include <GxEPD.h>
#include <GxGDEP015OC1/GxGDEP015OC1.cpp>
// For the 4.2 inch display use the following class instead
// #include <GxGDEW042T2>/GxGDEW042T2.cpp
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
 
// ESP8266 generic/common.h
//static const uint8_t SS    = 15;
//static const uint8_t MOSI  = 13;
//static const uint8_t MISO  = 12;
//static const uint8_t SCK   = 14;
 
// FreeFonts from Adafruit_GFX 
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

GxIO_Class io(SPI, SS/* CS */,  D3/* DC */,  D4/* RST */);
//GxEPD_Class display(io, D4 /* RST */, D2 /* BUSY */);
GxEPD_Class display(io);
 
Adafruit_BME280 bme; // I2C

const char* ssid = "ssid";
const char* password = "password";

double h, t, p, dp;
int oldT;
int oldH;
char temperatureString[6];
char dpString[6];
char humidityString[6];
char pressureString[8];

WiFiServer server(80);

void setup() {
//  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(10);
  //Wire.begin(int sda, int scl);
  Wire.begin(9, 10);
  // Connecting to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Uncomment and adjust the following lines if you want to use a static IP address
  // IPAddress ip(192, 168, 1, 106); // where xx is the desired IP Address
  // IPAddress gateway(192, 168, 1, 1); // set gateway to match your network
  // IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
  // WiFi.config(ip, gateway, subnet);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("Web server running...");
  delay(6000);
  i2cScanner();
  Serial.println(F("BME280 test"));
  if (!bme.begin()) {
    Serial.println("Could not find a BME280 sensor!");
  }
  display.init();
}

double computeDewPoint2(double celsius, double humidity) {
    double RATIO = 373.15 / (273.15 + celsius);  // RATIO was originally named A0, possibly confusing in Arduino context
    double SUM = -7.90298 * (RATIO - 1);
    SUM += 5.02808 * log10(RATIO);
    SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
    SUM += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
    SUM += log10(1013.246);
    double VP = pow(10, SUM - 3) * humidity;
    double T = log(VP/0.61078);   // temp var
    return (241.88 * T) / (17.558 - T);
}

void getWeather() {  
    h = bme.readHumidity();
    t = bme.readTemperature();
    dp = computeDewPoint2(t, h);    
    p = bme.readPressure()/100.0F;

    dtostrf(t, 5, 1, temperatureString);
    dtostrf(h, 5, 1, humidityString);
    dtostrf(p, 7, 1, pressureString);
    dtostrf(dp, 5, 1, dpString);
    delay(100); 
}

void loop() {
  getWeather();
  displayBME280();
  // Listenning for new clients
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New client");
    // boolean to detect end of http request
    boolean blank_line = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // If the client's http request was received completely, the device sends the
        // http response which includes the web page in its body (payload)
        if (c == '\n' && blank_line) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            // the actual web page that displays temperature, humudity and air pressure
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            client.println("<head><META HTTP-EQUIV=\"refresh\" CONTENT=\"15\"></head>");
            client.println("<body><h1>ESP8266 Weather Web Server</h1>");
            client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
            client.println("<h3>Temperature = ");
            client.println(temperatureString);
            client.println("&deg;F</h3><h3>Humidity = ");
            client.println(humidityString);
            client.println("%</h3><h3>Approx. Dew Point = ");
            client.println(dpString);
            client.println("&deg;C</h3><h3>Pressure = ");
            client.println(pressureString);
            client.println(" hPa");
            client.println("</h3></td></tr></tbody></table></body></html>");  
            break;
        }
        if (c == '\n') {
          // when starts reading a new line
          blank_line = true;
        } else if (c != '\r') {
          // there is still a character in the current line
          blank_line = false;
        }
      }
    }  
    // closing the client connection
    client.stop();
    Serial.println("Client disconnected...");
  }
  delay(1000);
}

void i2cScanner() {
  byte error, address;
  int nDevices;
 
  Serial.println("Scanning...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");
 
      nDevices++;
    }
    else if (error==4)
    {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void displayBME280()
{
  int tt = abs(t*10);
  int hh = abs(h*10);
  if(tt < oldT || tt > oldT || hh < oldH-10 || hh > oldH+10) {
    oldT = tt;
    oldH = hh;
    char dC[3] = {'°', 'C', 0};
    Serial.print("Current humdity = ");
    Serial.print(h);
    Serial.print("% ");
    Serial.print("temperature = ");
    Serial.print(t);
    Serial.print(dC);
    Serial.print(" dewpoint = ");
    Serial.print(dp);
    Serial.println(dC);
   
    uint16_t middleY = display.height() / 2;
    display.setRotation(3);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold24pt7b);
    display.setTextSize(2);
    display.setCursor(8, 72);
    display.print(t, 1);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextSize(2);
    display.setCursor(16, 146);
    display.print(h, 1);
    display.setTextSize(1);
    display.print("%");
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(2);
    display.setCursor(16, 198);
    display.print(p, 1);
    display.setTextSize(1);
    display.print(" hPa");
   
    display.update();
  }
}
