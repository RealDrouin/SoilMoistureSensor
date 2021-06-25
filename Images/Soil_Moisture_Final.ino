
/* Soil Moisture Monitor.
    Hardware ESP8266 Soil Moisture Probe V2.2 (NodeMcu1.0).
    Date 21.may.2018 Made by Ve2Cuz Real Drouin.

  ///////// Pin Assigment ///////

  A0  Input Soil Moisture and Battery
  GPIO4   SDA for tmp112
  GPIO5   SCL for tmp112
  GPIO12  Button S1 (Active Access Point for Config Sensor)
  GPIO13  LED
  GPIO14  Clock Output for soil moisture sensor
  GPIO15  SWITCH for measuring Soil Moisture or Battery Voltage

  //////////////////////////////////////////////////////////////////////

  ThinsSpeak

  Field 1 = Temperature Celcius
  Field 2 = Soil Moisture %
  Field 3 = Battery %
  Field 4 = Sleep Time
  Field 5 = Wifi Strength dBm

  /////////////////////////////////////////
*/

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <ESP8266HTTPUpdateServer.h> //Firmware Update ota
#include <ESP8266WebServer.h> //Firmware Update ota

#include <Wire.h>

// I2C address for temperature sensor
const int TMP_ADDR  = 0x48;

// Time to sleep (in seconds):
int sleepTimeS;

/////////// Temp Variable //////////
int Sys; // 0 = Imperial or 1 = Metric
float tCelcius;
float tFar;
float temp;
float TempCal = 0;
float RealTemp = 0;

////////////  Setup  //////////////
bool Setup = 0;

///////////// WIFI VARIABLE //////////////
const String HostName = "SOIL MOISTURE SENSOR";
const String Ver = "Ver 1.2";
String ssid = "";
int ssidlength;
String pass = "";
int passlength;
const char* APpassword = ""; //-- Password Access Point--

/////////// ThingSpeak ///////////////////
String apiKey = "";

////////////// Soil Moisture Sensor //////
const int PIN_CLK   = 14; // or D5
const int PIN_SOIL  = A0;
int Soil = 0;
int soil_hum = 0;
int SoilTrig;
int SoilValue;

/////// Setup Push Button //////////////
const int ConfigPin = 12; // D6
bool Config;
const int Led = 13; // D7

////// Electronique Switch Soil-Moisture & Battery Voltage //////
const int Switch = 15; // D8
int batt; // Numeric Value
int Batt; // % Value

///////////////////////////////////////////////////
String readString;

WiFiServer serveur(80);
WiFiClient client;
ESP8266WebServer httpServer(8080); //Firmware Update ota
ESP8266HTTPUpdateServer httpUpdater; //Firmware Update ota

//////////// Read Temperature Celsius //////////
float readTemperature() {

  // Begin transmission
  Wire.beginTransmission(TMP_ADDR);
  // Select Data Registers
  Wire.write(0X00);
  // End Transmission
  Wire.endTransmission();

  delay(500);

  // Request 2 bytes , Msb first
  Wire.requestFrom(TMP_ADDR, 2 );

  // Read temperature as Celsius (the default)
  while (Wire.available()) {
    int msb = Wire.read();
    int lsb = Wire.read();
    Wire.endTransmission();

    int rawtmp = msb << 8 | lsb;
    int value = rawtmp >> 4;

    temp = value * 0.0625;

    return temp;
  }
}

///////// Get Soil Sensor Value //////////
float readSoilSensor() {
  float tmp = 0;
  float total = 0;
  float rawVal = 0;
  int sampleCount = 3;

  for (int i = 0; i < sampleCount; i++) {
    rawVal = analogRead(PIN_SOIL);
    total += rawVal;
  }

  tmp = total / sampleCount;
  return tmp;
}

/////////// Get Batt Voltage ///////////
float readBatt() {
  float tmp = 0;
  float total = 0;
  float rawVal = 0;
  int sampleCount = 3;

  for (int i = 0; i < sampleCount; i++) {
    rawVal = analogRead(PIN_SOIL);
    total += rawVal;
  }
  tmp = total / sampleCount;
  return tmp;
}
//////////////////////////////////////////////

void setup() {
  //////////////// I2C BUS /////////////////
  Wire.begin();

  //////////////// PIN I/O Setup /////////////
  pinMode(Led, OUTPUT);
  digitalWrite(Led, HIGH);

  pinMode(ConfigPin, INPUT_PULLUP);

  pinMode (Switch, OUTPUT); // LOW=Battery Voltage , HIGH=Soil Moisture
  digitalWrite (Switch, HIGH); // Soil-Moisture Selected

  /////////// Soil Moisture //////////////
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_SOIL, INPUT);
  analogWriteFreq(40000);
  analogWrite(PIN_CLK, 400);
  delay(500);

  // device address is specified in datasheet
  Wire.beginTransmission(TMP_ADDR); // transmit to device #44 (0x2c)
  Wire.write(byte(0x01));            // sends instruction byte
  Wire.write(0x60);             // sends potentiometer value byte
  Wire.endTransmission();     // stop transmitting

  ////////////////////////////////////////
  EEPROM.begin(512);
  delay(200);

  //////// READ EEPROM SSID & PASSWORD //////
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  ssid = esid;

  int passlength = EEPROM.read(32) + 33;
  delay(200);
  String epass = "";
  for (int i = 33; i < passlength; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  pass = epass;

  ////////// Temperature Calibration /////////
  String ecal = "";
  for ( int i = 127; i < 133;  ++i)
  {
    ecal += char(EEPROM.read(i));
  }
  TempCal = (ecal.toFloat()); // convert string to float

  //////// Sleep Time /////////
  String esleep = "";
  for ( int i = 150; i < 156;  ++i)
  {
    esleep += char(EEPROM.read(i));
  }
  sleepTimeS = (esleep.toInt()); // convert string to int

  /////////// ThingSpeak API KEY //////////
  int ApiKey = EEPROM.read(300) + 301;
  delay(200);
  String eApiKey = "";
  for (int i = 301; i < ApiKey; ++i)
  {
    eApiKey += char(EEPROM.read(i));
  }
  apiKey = eApiKey;

  /////////// System //////////
  Sys = EEPROM.read(426);

  EEPROM.end();

  ////////////////////////////////////////////////

  WiFi.mode(WIFI_STA); // Set to Station
  delay(200);
  WiFi.hostname(HostName); // display hostname on router.
  delay(200);

  WiFi.begin(esid.c_str(), epass.c_str());
  bool state = HIGH;
  int s = 0;
  while (s < 120) {

    if (WiFi.status() == WL_CONNECTED) {
      s = 120;
    }
    delay(500);
    s++;
    state = (state == HIGH) ? LOW : HIGH;
    digitalWrite (Led, state);

    Config = digitalRead(ConfigPin);

    if (Config == LOW)
    {
      Setup = 1;
      WiFi.disconnect();
      delay(1000);
      digitalWrite (Led, LOW);
      WiFi.mode(WIFI_AP);
      delay(200);
      WiFi.softAP("Soil Moisture Sensor", APpassword, 1); // 1 = Channel
      WiFi.begin();
      delay(1000);
      break;
    }
  }

  if (Setup == 0 && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    delay(1000);
    ESP.deepSleep(sleepTimeS * 1000000, WAKE_RF_DEFAULT);
    delay(10000);
  }

  if (Setup == 1) {
    serveur.begin();
    httpUpdater.setup(&httpServer);//ota
    httpServer.begin();//ota
  }
}

////////////////////////////////////////////////////
void loop() {

  if (Setup == 1)
  {
    httpServer.handleClient(); //OTA
    ////////////////// SERVER HTTP //////////////
    WiFiClient client = serveur.available();
    if (!client) {
      yield();
      return;
    }

    if (client) {

      while (client.connected()) {

        if (client.available()) {

          char c = client.read();

          if (readString.length() < 100) {
            readString += c;
          }

          if (c == '\n') {

            if (readString.indexOf("?" ) < 0) {
            }

            else
            {
              EEPROM.begin(512);

              if (readString.indexOf("?ssid=") > 0)
              {
                for (int i = 0; i < 32; ++i)
                {
                  EEPROM.write(i, 0);
                }
                delay(200);
                EEPROM.commit(); // save on eeprom

                String qsid;
                readString.replace(" HTTP/1.1", ""); // replace characte by nothing on string
                ascii();
                readString.trim(); // remove space from string
                qsid = readString.substring(readString.indexOf ("?ssid=") + 6);

                for (int i = 0; i < qsid.length(); ++i)
                {
                  EEPROM.write(i, qsid[i]);
                }
                delay(200);
                EEPROM.commit();// save on eeprom
                ssid = qsid;
              }

              /////////////// PASSWORD SETUP ////////////
              if (readString.indexOf("?pass=") > 0 )
              {
                for (int i = 32; i < 98; ++i)
                {
                  EEPROM.write(i, 0);
                }
                delay(200);
                EEPROM.commit();// save on eeprom

                String qpass;
                readString.replace(" HTTP/1.1", ""); // replace characte by nothing on string
                ascii();
                readString.trim();// remove space from string
                qpass = readString.substring(readString.indexOf("?pass=") + 6);

                EEPROM.write(32, qpass.length());
                for (int i = 0; i < qpass.length(); ++i)
                {
                  EEPROM.write(33 + i, qpass[i]);
                }
                delay(200);
                EEPROM.commit();// save on eeprom
                pass = qpass;
              }

              //////////// Api Key /////////
              if (readString.indexOf("?apiKey=") > 0)
              {
                for (int i = 300; i < 325; ++i)
                {
                  EEPROM.write(i, 0);
                }
                delay(200);
                EEPROM.commit(); // save on eeprom

                String qApiKey = "";
                readString.replace(" HTTP/1.1", ""); // replace characte by nothing on string
                ascii();
                readString.trim(); // remove space from string
                qApiKey = readString.substring(readString.indexOf ("?apiKey=") + 8);

                int qapikeylength = qApiKey.length();
                EEPROM.write(300, qapikeylength);

                for (int i = 0; i < qApiKey.length(); ++i)
                {
                  EEPROM.write(301 + i, qApiKey[i]);
                }
                delay(200);
                EEPROM.commit();
                apiKey = qApiKey;
              }

              ///////// Sleep Time /////////
              if (readString.indexOf("?sleep=") > 0)
              {
                for (int i = 150; i < 160; ++i)
                {
                  EEPROM.write(i, 0);
                }
                delay(200);
                EEPROM.commit(); // save on eeprom

                String qsleep = "";
                readString.replace(" HTTP/1.1", ""); // replace characte by nothing on string
                ascii();
                readString.trim(); // remove space from string
                qsleep = readString.substring(readString.indexOf ("?sleep=") + 7);

                for (int i = 0; i < qsleep.length(); ++i)
                {
                  EEPROM.write(150 + i, qsleep[i]);
                }
                delay(200);
                EEPROM.commit();
                sleepTimeS = (qsleep.toInt());
              }

              /////////  Temperature Calibration ////////
              if (readString.indexOf("?cal=") > 0)
              {
                ReadSensor();

                for (int i = 127; i < 133; ++i)
                {
                  EEPROM.write(i, 0);
                }
                delay(200);
                EEPROM.commit(); // save on eeprom

                String qcal;
                String rtemp;
                readString.replace(" HTTP/1.1", ""); // replace characte by nothing on string
                ascii();
                readString.trim(); // remove space from string
                rtemp = readString.substring(readString.indexOf ("?cal=") + 5);

                RealTemp = (rtemp.toFloat());

                if (Sys == 1)
                {
                  RealTemp = RealTemp * 9 / 5 + 32; // Convert celcius to fahrenheit before save on eeprom
                }

                TempCal = temp - RealTemp;

                qcal = String(TempCal);

                for (int i = 0; i < qcal.length(); ++i)
                {
                  EEPROM.write(127 + i, qcal[i]);
                }
                delay(200);
                EEPROM.commit();
              }

              ///////// RESET ESP //////////
              if (readString.indexOf("?reset") > 0 )
              {
                client.println(" HTTP/1.1 200 OK");
                client.println("Content-Type: text/html");
                client.println("");
                client.println("<!DOCTYPE HTML>");
                client.println("<html><head>");
                client.println("<title>Soil Moisture Monitor");
                client.println("</title></head>");
                client.println("<body><center>");
                client.println("<br><br><br><h1>RESET Sensor !!");
                client.print("</h1></center></body></html>");
                client.println("\r\n\r\n");

                delay(1);
                readString = "";
                client.flush();
                client.stop();

                WiFi.disconnect();
                delay(10000);
                ESP.restart();
              }

              //////// ERASE EEPROM /////////
              if (readString.indexOf("?erase") > 0 )
              {
                client.println(" HTTP/1.1 200 OK");
                client.println("Content-Type: text/html");
                client.println("");
                client.println("<!DOCTYPE HTML>");
                client.println("<html><head>");
                client.println("<title>Soil Moisture Monitor");
                client.println("</title></head>");
                client.println("<body><center>");
                client.println("<br><br><br><h1>ERASE EEPROM !!");
                client.print("</h1></center></body></html>");
                client.println("\r\n\r\n");

                delay(1);
                readString = "";
                client.flush();
                client.stop();

                WiFi.disconnect();

                for (int i = 0; i < 513; ++i)
                {
                  EEPROM.write(i, 0);
                }
                delay(200);
                EEPROM.commit(); // save on eeprom

                delay(10000);
                ESP.restart();
              }

              //////// System Metric or Imperial //////
              if (readString.indexOf("?System=") > 0)
              {
                String qSys = "";
                readString.replace(" HTTP/1.1", ""); // replace characte by nothing on string
                ascii();
                readString.trim(); // remove space from string
                qSys = readString.substring(readString.indexOf ("?System=") + 8);
                Sys = qSys.toInt();

                EEPROM.write(426, Sys);

                delay(200);
                EEPROM.commit();
              }
              EEPROM.end();
            }

            ////////////////////////////////////////////
            client.flush();
            ReadSensor();
            ////////////// HTML PAGE ///////////////
            client.println(" HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("charset=UTF-8");
            client.println("Connnection: close");
            client.println("");
            client.println("<!DOCTYPE HTML>");
            client.println("<html><head><title>SOIL MOISTURE SENSOR");
            client.println("</title>");
            client.println("<meta charset=UTF-8>");
            client.println("<meta name=viewport content=width=device-width, initial-scale=1>");


            client.println("<style>");
            ////// CSS Language //////
            String Button = "";

            Button += ".button {";
            Button += "background-color: #4CAF50;";
            Button += "border: none;";
            Button += "border-radius: 15px;";
            Button += "box-shadow: 0 9px #999;";
            Button += "color: #fff;";
            Button += "padding: 15px 25px;";
            Button += "text-align: center;";
            Button += "text-decoration: none;";
            Button += "display: inline-block;";
            Button += "font-size: 24px;";
            Button += "margin: 4px 2px;";
            Button += "cursor: pointer;";
            Button += "outline: none;";
            Button += "}";

            Button += ".button:hover {background-color: #3e8e41}";

            Button += ".button:active {";
            Button += "background-color: #3e8e41;";
            Button += "box-shadow: 0 5px #666;";
            Button += "transform: translateY(4px);";

            Button += "</style>";
            client.println(Button);

            client.println("</head>");
            client.println("<body><center><b><h1><font color = #994d00>");
            client.println(HostName);
            client.println("</b></h1></font>");

            client.print("<hr><h3><font color=red>NOTE : 0% = Dry , 100% = Wet</font></h3>");
            client.print("<h1><font color=green>"), client.print(Soil), client.println("% Soil Moisture</font></b><br>");

            if (Sys == 0) {
              client.println("<b><font color = black>Temp : "), client.print(tFar), client.print("  &#8457; </font>");

            }
            else {
              client.println("<b><font color = black>Temp : "), client.print(tCelcius), client.print(" &#8451 </font>");

            }

            if (Batt > 10)
            {
              client.print("<br><font color=blue>"), client.print(Batt), client.println("% Battery</h1></font></b>");
            }
            else
            {
              client.print("<br><font color=red>"), client.print(Batt), client.println("% Please Change Battery !!</h1></font></b>");
            }

            client.print("<br>");

            ////////// CSS Buttom ////////
            client.print("<a href=");
            client.print("/?erase");
            client.print(" class=button>Erase Memory");
            client.print("</a>");

            ////////// CSS Buttom ////////
            client.print("<a href=");
            client.print("/?reset");
            client.print(" class=button>Restart");
            client.print("</a>");
            //////////////////////////////

            //// SSID PASSWORD Menu /////
            client.print("<br><br><hr><h1><b>- SSID & PASSWORD SETUP -</h1></b>");
            client.print("<form><label>SSID: </label><input type=text name=ssid length=32 value="), client.print(ssid.c_str()), client.print("><input type=submit></form><br>");

            String Set = "";
            if (pass.length() > 0) {
              for (int i = 0; i < pass.length(); ++i)
              {
                Set += "*";
              }
            }
            else {
              Set = "Not_Set";
            }
            client.print("<form><label>PASS : </label><input type=text name=pass length=64 value="), client.print(Set), client.print("><input type=submit></form>");

            ////////////////////////////////

            client.println("<br><hr><h1><b>- SOIL MOISTURE SENSOR SETUP -</h1></b>");

            String APIKEY = "";
            if (apiKey.length() > 0) {
              for (int i = 0; i < apiKey.length(); ++i)
              {
                APIKEY += "*";
              }
            }
            else {
              APIKEY = "Not_Set";
            }
            client.print("<form><label>THINGSPEAK.com API KEYS : </label><input type=text name=apiKey maxlength=16 value="), client.print(APIKEY), client.print("><input type=submit></form>");

            client.print("<br><form><label>Sleep Time sec : </label><input type=number name=sleep value="), client.print(sleepTimeS), client.print(" min=30 max=2880><input type=submit></form>");

            /////////////////// Temps Calibration /////////////
            client.print("<br><br>");
            if (Sys == 0) {
              client.print("<b>Input Real Temps  &#8457 to calculate Temps Correction");
            }
            else {
              client.print("<b>Input Real Temps  &#8451 to calculate Temps Correction");
            }
            client.print("<br><form><label></label><input type=text name=cal size=6 maxlength=6><input type=submit></form>");
            client.print("<br>");
            ////////////////////////////////////////////////////

            ////////// CSS Buttom ////////
            if (Sys == 0) {
              client.print("<a href=");
              client.print("/?System=1");
              client.print(" class=button>Metric");
              client.print("</a>");
            }

            if (Sys == 1) {
              client.print("<a href=");
              client.print("/?System=0");
              client.print(" class=button>Imperial");
              client.print("</a>");
            }

            client.println("<br><br><hr><h1><b>- FIRMWARE UPDATE OVER THE AIR -</h1></b>");

            client.print("<a href=http://192.168.4.1");
            client.print(":8080/update");
            client.print(" class=button>Bin File");
            client.print("</a>");

            client.println("<br><br><hr/>");

            ////////////// Firm Info //////////////
            client.print("Soil Moisture Sensor ");
            client.print(Ver);
            client.print("<br>");
            client.print("<br>Made by VE2CUZ</center></body></html>");
            client.println("\r\n\r\n");

            delay(1);
            readString = "";
            client.flush();
            client.stop();
            break;
          }
        }
      }
    }
  }
  else {
    ReadSensor();
  }
}
//////////////////////////////////////////
void ReadSensor()
{
  ///////// Get Temp ///////
  temp = readTemperature(); // Real Temps in Celcius
  temp = (temp * 9 / 5 + 32); // Convert Celcius to Fahrenheit

  ////// Apply Temp Correction /////
  tFar = (temp - TempCal); // Apply temps Correction
  tCelcius = (tFar - 32) * 5 / 9; // Convert Fahrenheit to Celcius

  ///////// Get Battery Voltage ////////
  digitalWrite (Switch, LOW); // Battery Voltage Selected
  delay(200);
  batt = readBatt();
  Batt = map(batt, 736, 880, 0, 100); // 736 = 2.5v , 880 = 3.0v , esp dead at 2.3v
  if (Batt > 100) Batt = 100;
  if (Batt < 0) Batt = 0;

  ////// Get Soil Moisture //////
  delay(100);
  digitalWrite (Switch, HIGH); // Soil Moisture Selected
  delay(200);
  soil_hum = readSoilSensor();
  SoilValue = (100 * soil_hum / batt); // Battery Drop Correction
  Soil = map(SoilValue, 60, 82, 100, 0); // Convert to 0 - 100%, 0=Dry, 100=Wet
  if (Soil > 100) Soil = 100;
  if (Soil <  0) Soil = 0;

  ThingSpeak();
}

/////////// ThingSpeak /////////////
void ThingSpeak()
{
  if (Setup == 1) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (client.connect("api.thingspeak.com", 80)) {

      digitalWrite (Led, LOW);

      String postStr = "";
      postStr += apiKey;
      postStr += "&field1=";

      if (Sys == 0)
      {
        postStr += String(tFar);
      }
      else {
        postStr += String(tCelcius);
      }

      postStr += "&field2=";
      postStr += String(Soil);
      postStr += "&field3=";
      postStr += String(Batt);
      postStr += "&field4=";
      postStr += String(sleepTimeS);
      postStr += "&field5=";
      postStr += String(WiFi.RSSI());
      postStr += "\r\n\r\n";

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");

      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr.length());
      client.print("\n\n");
      client.print(postStr);
      delay(1000);
    }
  }
  digitalWrite (Led, HIGH);
  WiFi.disconnect();
  delay(1000);
  ESP.deepSleep(sleepTimeS * 1000000, WAKE_RF_DEFAULT);
  delay(10000);
}

//////////////  ASCII HEx to Char /////////////

void ascii()
{
  readString.replace("+", "_");
  readString.replace("%20", "_");
  readString.replace("%21", "!");
  readString.replace("%22", "\"");
  readString.replace("%23", "#");
  readString.replace("%24", "$");
  readString.replace("%25", "%");
  readString.replace("%26", "&");
  readString.replace("%27", "'");
  readString.replace("%28", "(");
  readString.replace("%29", ")");
  readString.replace("%2A", "*");
  readString.replace("%2B", "+");
  readString.replace("%2C", ",");
  readString.replace("%2D", "-");
  readString.replace("%2E", ".");
  readString.replace("%2F", "/");
  readString.replace("%3A", ":");
  readString.replace("%3B", ";");
  ///////////////////////////////
  readString.replace("%3C", "");
  readString.replace("<", "");
  ///////////////////////////////
  readString.replace("%3D", "=");
  ///////////////////////////////
  readString.replace("%3E", "");
  readString.replace(">", "");
  ///////////////////////////////
  readString.replace("%3F", "?");
  readString.replace("%40", "@");
  readString.replace("%5B", "[");
  readString.replace("%5C", "\\");
  readString.replace("%5D", "]");
  readString.replace("%5E", "^");
  readString.replace("%5F", "_");
  readString.replace("%60", "");
  readString.replace("%7B", "{");
  readString.replace("%7C", "|");
  readString.replace("%7D", "}");
  readString.replace("%7E", "~");
}


