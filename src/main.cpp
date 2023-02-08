#include <Arduino.h>
#include "AsyncTCP.h"

#include "ESPAsyncWebServer.h"
#include "ESPAsyncWiFiManager.h"         //https://gitWiFiManagerhub.com/tzapu/WiFiManager
//#include <WiFiManager.h>
#include <WebSerial.h>
#include <ESPmDNS.h>
#include "fishScheduler.h"
#include "fbdb.h"
#include "utility.h"
//#include "SPIFFS.h"
#include "Effortless_SPIFFS.h"
#include "ArduinoJson.h"
//#include <Wire.h>  // must be included here so that Arduino library object file references work
//#include <RtcDS3231.h>

//RtcDS3231<TwoWire> Rtc(Wire);

AsyncWebServer server(80);
DNSServer dns;
void configModeCallback (AsyncWiFiManager *myWiFiManager);
 
FishSched *mySched;
Database *db;

const char* host = "maker.ifttt.com";  //used in sendHttp
const int httpsPort = 80;  //used in sendHttp
String url = "";  //used in sendHttp
WiFiClient client;  //this is passed into Dosing constructor for connecting to doser
String iPAddress;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool blinking = false;
int LED = 2;
int w = 0;
bool aFlagWasSetInLoop = false;
bool thisIsAPumpEvent = false;


bool alreadyReset = true;
bool midnightDone = false;
int yr = 0;
String yrStr = "";
int mo = 0;
int da = 0;
String daStr = "";

int NORMAL_LEVEL_PIN = 15;
int HIGH_LEVEL_PIN = 15;
int LOW_LEVEL_PIN = 15;
int SUMP_MOTOR_PIN = 15;
int NEW_MOTOR_PIN = 15;
int RO_MOTOR_PIN = 15;

bool notDosing();
void setDate();
void checkDosingSched(int i);
bool notDosing();//TODO

void setup() {
     ///////////////////Start WiFi ///////////////////////////////////////
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server, &dns);
  //reset settings - for testing
  //wifiManager.resetSettings();
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,1,175), IPAddress(192,168,1,1), IPAddress(255,255,255,0), IPAddress(192,168,1,1), IPAddress(192,168,1,1));
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Doser")) {
    Serial.println("failed to connect and hit timeout");
    Serial.println("restarting esp");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  delay(50);
  //Serial.print("FreeHeap is :");
  //Serial.println(ESP.getFreeHeap());
  delay(50);
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  server.begin();
 WebSerial.begin(&server);
 delay(100);

pinMode(NORMAL_LEVEL_PIN, INPUT);
pinMode(HIGH_LEVEL_PIN, INPUT);
pinMode(LOW_LEVEL_PIN, INPUT);
pinMode(SUMP_MOTOR_PIN, OUTPUT);
pinMode(NEW_MOTOR_PIN, OUTPUT);
pinMode(RO_MOTOR_PIN, OUTPUT);

  mySched = new FishSched();
  db  = new Database();
  db->initDb();
  mySched->updateMyTime();

}

void loop() {
  if(blinking){
    digitalWrite(LED, 0);
    blinking = false;
  }else{
    digitalWrite(LED, 1);
    blinking = true;
  }
  //check if any calibraton button is pressed and only do that
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    count++;
    //db->callBack();
  }

  if(Database::dataChanged){
    db->setEvents(0);
    Database::dataChanged = false;
  }

  while (WiFi.status() != WL_CONNECTED ){
    //TODO db->callBack should be called
    if(w < 2){
      Serial.print('!');
      WebSerial.print("M_wfnc");
      //Serial.println("-1");
      delay(1000);
      w = 0;
      break;
    }else{
      w++;
    }
  }

  mySched->setNowTime(); //initializes time MUST DO THIS

  mySched->tick(); //sets hour
  mySched->tock();//sets minute

  int z = 0;
  for(int i= 0;i<37;i++){
      int flagSet = mySched->isFlagSet(i);
      z++;
      if(flagSet == 1){
        aFlagWasSetInLoop = true;
        //Serial.print("event is: ");
        //Serial.println(i);
        if(i != 1){
          if(i !=2){
            //WebSerial.print("Event ");
            //WebSerial.print(i);
            //WebSerial.print(" just fired");
          }else{
            //WebSerial.print("-");
          }
        }else{
          //WebSerial.print("+");
        }
        checkDosingSched(i);
       }//if 
      //Serial.println("out of flagset loop");
  }

}

//////////////////////////////////////////////////////////////
// call back needed for wifi
/////////////////////////////////////////////////////////////
void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  //myWiFiManager->startConfigPortal("ATOAWC");addDailyDoseAmt
  //myWiFiManager->autoConnect("DOSER");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());

}


void checkDosingSched(int i){
    if(db->isThisEventPumpSet(i, 0)){
      thisIsAPumpEvent = true;
     // doseBlue(i);
    }
    if (db->isThisEventPumpSet(i, 1)){
      thisIsAPumpEvent = true;
      //doseGreen(i);
    }
    if(db->isThisEventPumpSet(i, 2)){
      thisIsAPumpEvent = true;
      //doseYellow(i);
    }
    if (db->isThisEventPumpSet(i, 3)){
      //Serial.println("B+");
      thisIsAPumpEvent = true;
      //dosePurple(i);
    }
    if(notDosing() && !alreadyReset){ 
      mySched->setFlag(i,0);
    }

  if(i == 8 && !midnightDone){
    midnightDone = true;
    setDate();
    mySched->setFlag(i,0);
    mySched->updateMyTime();
    //TODO writeDailyDosesToDb();
    WebSerial.print("saved to db");
  }
  if(!thisIsAPumpEvent){
    mySched->setFlag(i,0);
  }
}

void setDate(){
  mySched->syncTime();
  yr = mySched->getCurrentYear();
  yr = yr;
  yrStr = String(yr);
  Serial.print("Year is: ");
  Serial.println(yrStr);
  mo = mySched->getCurrentMonth();
  Serial.print("Month is: ");
  Serial.println(mo);

  da = mySched->getCurrentDay();
  daStr = String(da-1);  //i need the day for save to db be the day before since i save at midnight
  Serial.print("YesterDay is: ");
  Serial.println(daStr);
 
  //TODO fileSystem.saveToFile("/curDay.txt", daStr);

}


bool notDosing(){  //TODO
  bool retVal = true;

  return retVal;
}