#include <Arduino.h>

/*
        Central heating relays are attached to two esp32 pins
        Allows OTA updates (in theory- need to test!)
            Ideally would interwork with wifimanager of some kind!
        Provides a single GET endpoint at /set_state passing hot_water and heating as params
        1 means on!
        Anything else is considered off

        Credentials are hardcoded in a credentials.h file (not in repo)
        with two variables:
              const char* ssid = "........";
              const char* password = "........";
*/

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include "credentials.h"

#define PIN_HOT_WATER 4
#define PIN_HEATING 15
#define ON_STATE false
#define OFF_STATE true


char hostname[]="heatingrelays";



// const char* ssid = "........";
// const char* password = "........";

AsyncWebServer server(80);

void set_heating_state(AsyncWebServerRequest *req)
{
  int params = req->params();
  bool new_heating=false;
  bool new_hot_water=false;
  for(int i=0;i<params;i++)
  {
    AsyncWebParameter* p = req->getParam(i);
    if(p->isFile())
    { //p->isPost() is also true
      Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if (strncmp("heating",p->name().c_str(),6)==0)
      {
        Serial.println("Heating to go on");
        new_heating=(p->value().c_str()[0]=='1');
      }
      if (strncmp("hot_water",p->name().c_str(),6)==0)
      {
        Serial.println("Hot water to go on");
        new_hot_water=(p->value().c_str()[0]=='1');
      }

    }
  }
  Serial.printf("Heating to be: %s\nHot water to be: %s\n",new_heating?"ON":"OFF",new_hot_water?"ON":"OFF");

  digitalWrite(PIN_HOT_WATER,new_hot_water?ON_STATE:OFF_STATE);
  digitalWrite(PIN_HEATING,new_heating?ON_STATE:OFF_STATE);

  req->send(200,"text/plain","OK");
}

void setup(void) {
  pinMode(PIN_HOT_WATER,OUTPUT);
  pinMode(PIN_HEATING,OUTPUT);
  Serial.begin(115200);

  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  delay(1000);
  WiFi.begin(ssid, password);
  delay(1000);
  Serial.printf("Connecting to: %s\n",ssid);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is a sample response.\n* Use /set_state?heating=1&hot_water=0\n use /update to change the firmware");
  });

  server.on("/set_state",HTTP_GET,[](AsyncWebServerRequest *request)
  {
    set_heating_state(request);
  });

  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
}