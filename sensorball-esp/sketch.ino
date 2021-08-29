/*  More information at: https://www.aeq-web.com/
 *  ESP32 SIM800 HTTP POST TEST | V 1.0_2020-AUG-18
 */
//#include "types.h"
#include "TinyGPS++.h";
#include "HardwareSerial.h";
#include <SoftwareSerial.h>

#include <ArduinoJson.h>

#include "DHT.h"
#include <Regexp.h>

#define LED            13

#define SIM800L_RX     27
#define SIM800L_TX     26
#define SIM800L_PWRKEY 4
#define SIM800L_RST    5
#define SIM800L_POWER  23

#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

#define GPS_RX      35
#define GPS_TX      34


//const char simPIN[] = "xxxx"; // "xxxx"; 
String apn = "webaut";                    //APN
String apn_u = "";                     //APN-Username
String apn_p = "";                     //APN-Password

String jsonOutput = "";

static const int RXPin = 35, TXPin = 34;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial SerialGPS(RXPin, TXPin);
//HardwareSerial SerialGPS(3);

#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float dht_h = 0.0;
float dht_t = 0.0;

String sim_battery = "";

// ############################
bool DEBUG = false; // to send even if no gps signal available

String url = "";  //URL of Server
//String url = ""; //URL redcat
bool USESSL = true;
bool DISPLAY_GPSINFO = true;
bool DISPLAY_ATINFO = true;
bool SKIP_GPS_TESTING = false;
long loop_intervall = 20; // seconds
 String sensorUUID = "7f87414d-053d-40ab-80fd-363e3b452a8f"; // BALL1  
// String sensorUUID = "22c8a393-aa14-481b-8bb5-47f5f966763c";    // BALL2  

const char simPIN[] = "xxxx"; // Ball 1
//const char simPIN[] = "xxxx"; // Ball 2
// ############################


void setup()
{
  // Initialize GUI
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(1000);
  digitalWrite(LED, LOW);
  m_blink(500,3);
  Serial.begin(115200);
  
  
  // Initialize GPS
  SerialGPS.begin(GPSBaud);
  // Testing GPS
  if( !SKIP_GPS_TESTING ){
    displayGPSInfoExtendedHeader();
    while( true ){
      m_blink(1000,1);
      gps_task();
      if (gps.location.isValid()){
        m_blink(250,3);
        break;
      }
      if( DISPLAY_GPSINFO ){ //debug
        displayGPSInfoExtended();
      }
      m_blink(500,2);
    }
  }

  // Initialize GPRS
  Serial.println("ESP32+SIM800L AT CMD Test");
  pinMode(SIM800L_POWER, OUTPUT);
  digitalWrite(SIM800L_POWER, HIGH);
  Serial2.begin(115200, SERIAL_8N1, SIM800L_TX, SIM800L_RX);
  Serial.println("Reset Module");
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  Serial.println("Module Resetted");
  delay(15000);
  long wtimer = millis();
  while (wtimer + (5000) > millis()) {
    while (Serial2.available()) {
      Serial.write(Serial2.read());
    }
  }
  /*
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
  delay(2000);*/
  gsm_send_serial("AT");
  gsm_send_serial("AT+SAPBR=3,1,Contype,GPRS");
  gsm_send_serial("AT+SAPBR=3,1,APN," + apn);
  if (apn_u != "") {
    gsm_send_serial("AT+SAPBR=3,1,USER," + apn_u);
  }
  if (apn_p != "") {
    gsm_send_serial("AT+SAPBR=3,1,PWD," + apn_p);
  }
  // Testing GPRS

  
  // Initialize Sensor
  dht.begin();
  Serial.println("DHT Initialized");

  
  // Show Status - Start|Error
  m_blink(250,2);
  
  // Start Loop
  
}


void loop() {
  long wtimer = millis();
  long loop_time = 30000;
  
  dht_task();
  gps_task();
  sim_task();
  create_payload();
  display_payload();

  if ( gps.location.isValid() || DEBUG ){
    gsm_http_post(jsonOutput);
    m_blink(500, 2);
  } else{
    Serial.println("Skipping GPRS Push...");
    m_blink(100, 2);
    delay(5000);
    return;
  }
  
  Serial.println("Loop Done");
  
  while (wtimer + (loop_intervall * 1000) > millis()) {}
}
void m_blink(int t, int c){
  for(int i = 0; i <= c;  i++){
    blink_once(t);
  }
}
void blink_once(int t){
  digitalWrite(LED, HIGH);
  delay(t);
  digitalWrite(LED, LOW);
  delay(t);
}

void create_payload(){
  jsonOutput = "";
  
  const size_t capacity = 2*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(7) + 160;
  DynamicJsonDocument root(capacity);
 
  JsonObject actuators = root.createNestedObject("actuators");
  
  JsonObject actuators_uuid = actuators.createNestedObject(sensorUUID);
  actuators_uuid["latitude"] = gps.location.lat();
  actuators_uuid["longitude"] = gps.location.lng();
  actuators_uuid["altitude"] = gps.altitude.meters();
  actuators_uuid["satellites"] = gps.satellites.value();
  actuators_uuid["speed"] = gps.speed.kmph();
  actuators_uuid["temp"] = dht_t;
  actuators_uuid["humi"] = dht_h;

  serializeJson( root, jsonOutput );
}
void display_payload(){
  Serial.println( jsonOutput );
}

void dht_task(){
  dht_h = dht.readHumidity();
  dht_t = dht.readTemperature();
}

void gps_task(){
  long wtimer = millis();
  while (wtimer + 1500 > millis()) {
    while (SerialGPS.available() > 0){
      gps.encode(SerialGPS.read());
    }
  }
}
void sim_task(){
  String command = "AT+CBC";
  Serial.println("Send ->: " + command);
  Serial2.println(command);
  long wtimer = millis();
  sim_battery = "";
  while (wtimer + (500) > millis()) {
    while (Serial2.available()) {
      sim_battery = sim_battery + Serial2.readString();
    }
  }
  Serial.println(sim_battery);
  Serial.println(sim_battery);
  Serial.println("--------------------------------------");
  sim_battery = sim_battery.substring(15,24);
  Serial.println(sim_battery);
  Serial.println("--------------------------------------");
  
}

void gsm_http_post( String postdata) {
  Serial.println(" --- Start GPRS & HTTP --- ");
  gsm_send_serial("AT+SAPBR=1,1");
  gsm_send_serial("AT+SAPBR=2,1");
  gsm_send_serial("AT+HTTPINIT");
  gsm_send_serial("AT+HTTPPARA=CID,1");
  gsm_send_serial("AT+HTTPPARA=URL," + url);
  
  if(USESSL){
    gsm_send_serial("AT+HTTPSSL=1");
    gsm_send_serial("AT+SSLOPT=0,1");
  }
  
  gsm_send_serial("AT+HTTPPARA=CONTENT,application/json");
  gsm_send_serial("AT+HTTPDATA=" + String(postdata.length()) + ",3000");
  gsm_send_serial_long(postdata,5000);
  gsm_send_serial("AT+HTTPACTION=1");
  gsm_send_serial_long("AT+HTTPREAD", 2000);
  gsm_send_serial_long("AT+HTTPTERM", 2000);
  gsm_send_serial("AT+SAPBR=0,1");
}

void gsm_send_serial(String command){
  gsm_send_serial_long(command, 500);
}
void gsm_send_serial_long(String command, int timeout) {
  if(DISPLAY_ATINFO) {
    Serial.println("Send ->: " + command);  
  }
  Serial2.println(command);
  long wtimer = millis();
  while (wtimer + (timeout) > millis()) {
    while (Serial2.available()) {
      if(DISPLAY_ATINFO) {
        Serial.write(Serial2.read());
      }else{
        Serial2.read();
      }
      
    }
  }
  if(DISPLAY_ATINFO) {
    Serial.println();
  }
}

void displayGPSInfoExtendedHeader(){
  Serial.println(F("FullExample.ino"));
  Serial.println(F("An extensive example of many interesting TinyGPS++ features"));
  Serial.print(F("Testing TinyGPS++ library v. ")); Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println(F("by Mikal Hart"));
  Serial.println();
  Serial.println(F("Sats HDOP Latitude   Longitude   Fix  Date       Time     Date Alt    Course Speed Card  Distance Course Card  Chars Sentences Checksum"));
  Serial.println(F("          (deg)      (deg)       Age                      Age  (m)    --- from GPS ----  ---- to London  ----  RX    RX        Fail"));
  Serial.println(F("---------------------------------------------------------------------------------------------------------------------------------------"));
}
void displayGPSInfoExtended(){
    
  static const double LONDON_LAT = 51.508131, LONDON_LON = -0.128002;
      
      printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
      printInt(gps.hdop.value(), gps.hdop.isValid(), 5);
      printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
      printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
      printInt(gps.location.age(), gps.location.isValid(), 5);
      printDateTime(gps.date, gps.time);
      printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
      printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);
      printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
      printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.value()) : "*** ", 6);
          
      unsigned long distanceKmToLondon =
        (unsigned long)TinyGPSPlus::distanceBetween(
          gps.location.lat(),
          gps.location.lng(),
          LONDON_LAT, 
          LONDON_LON) / 1000;
      printInt(distanceKmToLondon, gps.location.isValid(), 9);
    
      double courseToLondon =
        TinyGPSPlus::courseTo(
          gps.location.lat(),
          gps.location.lng(),
          LONDON_LAT, 
          LONDON_LON);
    
      printFloat(courseToLondon, gps.location.isValid(), 7, 2);
    
      const char *cardinalToLondon = TinyGPSPlus::cardinal(courseToLondon);
    
      printStr(gps.location.isValid() ? cardinalToLondon : "*** ", 6);

      
      printInt(gps.charsProcessed(), true, 6);
      printInt(gps.sentencesWithFix(), true, 10);
      printInt(gps.failedChecksum(), true, 9);
      Serial.println();
      
      smartDelay(1000);
    
      if (millis() > 5000 && gps.charsProcessed() < 10)
        Serial.println(F("No GPS data received: check wiring"));
}

void displayGPSInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (SerialGPS.available())
      gps.encode(SerialGPS.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec)
{
  if (!valid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    Serial.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  smartDelay(0);
}
