#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SDS011.h>
#include <SoftwareSerial.h>

// SH1106 OLED display related libraries
#include <Arduino.h>
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

/// Sensor pin setup
#define SDS_RX D5 //The cable from the SDS011 TXD pin goes to the defined pin on the 8266 board (i.e. tx goes to rx)
#define SDS_TX D6 //The cable from the SDS011 RXD pin goes to the defined pin on the 8266 board (i.e. rx goes to tx)
#define DHTPIN D4 
#define DHTTYPE DHT11   //DHT type : DHT11, DHT21, or DHT22 

// Network connectivity setup
const char* ssid     = "Your WiFi SSID";
const char* password = "Your WiFi Password";
const char* host = "api.thingspeak.com";
String apiKey = "Your Thingspeak channel write API key"; // thingspeak.com api key goes here

// Constant character setup, used for displaying symbols on OLED screen
const char DEGREE_SYMBOL[] = { 0xB0, '\0' }; // used for displaying degree symbol on OLED screen
const char MICRON_SYMBOL[] = { 0xB5, '\0' }; // used for displaying micron symbol on OLED screen
const char CUBED_SYMBOL[] = { 0xB3, '\0' }; // used for displaying subscript symbol on OLED screen


// Float setup for air structure calculations
float p10,p25;
int error;

struct Air {
  float pm25;
  float pm10;
  float humidity;
  float temperature;
};


DHT_Unified dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);
WiFiClient client;
SDS011 sds;

void setup() {
  Serial.begin(9600);
  sds.begin(SDS_RX,SDS_TX); // starts SDS011 sensor
  dht.begin(); // starts DHT sensor
  u8g2.begin(); // starts OLED screen
  connectToWiFi();
}

void loop() {
  Air airData = readPolution();

  // define strings that contain measurement values destined for OLED display
  String tempdisplay = "Temperature: " + String(airData.temperature) + DEGREE_SYMBOL + "C";
  String humiditydisplay = "Humidity: " + String(airData.humidity) + "%";
  String pm25display = "PM2.5: " + String(p25) + " " + MICRON_SYMBOL + "g/m" + CUBED_SYMBOL;
  String pm10display = "PM10: " + String(p10) + " "  + MICRON_SYMBOL + "g/m" + CUBED_SYMBOL;
  String fixed = F("COPY from FLASH"); // this line needed for u8g2 for displaying defined strings in drawStr

   //display measurements on OLED with measurements taken status
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,10,tempdisplay.c_str());
  u8g2.drawStr(0,20,humiditydisplay.c_str());
  u8g2.drawStr(0,30,pm25display.c_str());
  u8g2.drawStr(0,40,pm10display.c_str());
  u8g2.drawStr(0,60, "Measurements taken");
  u8g2.sendBuffer();
  
  if (client.connect(host,80) & airData.pm25 > 0.0) {
    String postStr = apiKey;
    postStr +="&field1=";
    postStr += String(p25);
    postStr +="&field2=";
    postStr += String(p10);
    postStr +="&field3=";
    postStr += String(airData.temperature);
    postStr +="&field4=";
    postStr += String(airData.humidity);
    postStr +="&field5=";
    postStr += String(airData.pm25);
    postStr +="&field6=";
    postStr += String(airData.pm10);
    postStr += "\r\n\r\n";
  
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);  
  }
  client.stop();

  //add one second delay before updating OLED status
  delay(1000);
  
  //display measurements on OLED with measurements uploded status
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,10,tempdisplay.c_str());
  u8g2.drawStr(0,20,humiditydisplay.c_str());
  u8g2.drawStr(0,30,pm25display.c_str());
  u8g2.drawStr(0,40,pm10display.c_str());
  u8g2.drawStr(0,60, "Measurements uploaded");
  u8g2.sendBuffer();
  
  delay(15000);
  
  server.handleClient();
}

void connectToWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  Serial.print("Connecting to ");
  Serial.println(ssid); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  startServer();
}

void startServer(){
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
    Air airData = readPolution();
    server.send(200, "text/plain", "PM2.5: " + String(p25) + " raw, " + String(airData.pm25) + " (" + String(calculatePolutionPM25(airData.pm25)) + "% normy) | PM10: " + String(p10) + " raw, " +  String(airData.pm10) + " (" + String(calculatePolutionPM10(airData.pm10)) + "% normy) | Temperature: " + airData.temperature + " | Humidity: " + airData.humidity);  
}

Air readPolution(){
  float temperature, humidity;
  error = sds.read(&p25,&p10);
  if (!error) {
    sensors_event_t event;  
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println("Error reading temperature!");
    } else {
      temperature = event.temperature;
    }
  
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println("Error reading humidity!");
    } else {
      humidity = event.relative_humidity;
    }
   
    Air result = (Air){normalizePM25(p25/10, humidity), normalizePM10(p10/10, humidity), humidity, temperature};
    return result;
  } else {
    Serial.println("Error reading SDS011");
    return (Air){0.0, 0.0, 0.0, 0.0};
  }
}

//Correction algorythm thanks to help of Zbyszek Kiliański (Krakow Zdroj)
float normalizePM25(float pm25, float humidity){
  return pm25/(1.0+0.48756*pow((humidity/100.0), 8.60068));
}

float normalizePM10(float pm10, float humidity){
  return pm10/(1.0+0.81559*pow((humidity/100.0), 5.83411));
}

float calculatePolutionPM25(float pm25){
  return pm25*100/25;
}

float calculatePolutionPM10(float pm10){
  return pm10*100/50;
}
