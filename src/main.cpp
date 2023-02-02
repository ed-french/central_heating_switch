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
#include <WiFi.h>
#include <HTTPClient.h>

#include "credentials.h"
#include "TFT_eSPI.h"



#define PIN_HOT_WATER 26
#define PIN_HEATING 27
#define PIN_OIL_BURNER 25
#define ON_STATE false
#define OFF_STATE true

char get_time_url[]="http://192.168.1.125/gettime";


TFT_eSPI tft = TFT_eSPI();


char hostname[]="heatingandoil";

bool tick_flag=true;

char temp_buff[1023];

char current_time[20];
bool oil_burning=false;

char ip_address[20];

bool heating_on=false;
bool hot_water_on=false;


// const char* ssid = "........";
// const char* password = "........";

AsyncWebServer server(80);


void show_message(const char * mess,uint16_t delay_ms=2000)
{
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.print(mess);
  delay(delay_ms);
}


void restart_esp()
{

  //ESP.restart();
  AsyncElegantOTA.restart();// Using this in preference as it includes a better shutdown of the server perhaps?
  
}


void get_time()
{
// fetches the time from the server locally, goes in the global current_time
  // check wifi still connected, otherwise save last sent and reboot

  if (WiFi.status()!=WL_CONNECTED)
  {
    Serial.println("WIFI DOWN!!!!");
    show_message("WiFi Down",3000);    ESP.restart();
  }


  // Make request

    HTTPClient http;


    uint8_t tries_left=1;


    while (tries_left>0)
    {
      char mess_buff[255];
      http.begin(get_time_url);

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);      

      //sprintf(mess_buff,"HTTP RESPONSE:\n%d",httpResponseCode);
      //show_message(mess_buff);

      if (httpResponseCode>0)
      {
        String payload = http.getString();
        Serial.println(payload); 

        if (httpResponseCode==200)
        {
          strncpy(current_time,payload.substring(11,16).c_str(),12);
          Serial.printf("Time found is : %s\n",current_time);
          http.end();
          return;
        }
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        sprintf(current_time,"fail %d",httpResponseCode);

        Serial.println("Trying again...");
        delay(100);

      }

      http.end();      
      


      tries_left--;
    }
}

void get_oil_burning()
{
  // Sets the global oil_burning
  oil_burning=!digitalRead(PIN_OIL_BURNER);
}

void get_and_show_time()
{
    get_time();
  // displays the time prt of it on the tft bottom without deleting anything else there
    tft.fillRect(0,100,240,135,TFT_OLIVE);
    tft.setCursor(8,108);
    tft.print(current_time);
}

void update_display(bool update_time)
{
    if (update_time) get_time(); // Update the record of the time
    get_oil_burning();
    tft.fillScreen(TFT_BLACK);



    // Display tick flag (small o in top right)
    tft.setCursor(225,0);
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED,TFT_BLACK);
    if (tick_flag) tft.print("o");


    // Display Burning or not
    tft.setCursor(58,3);
    tft.setTextSize(3);
    if (oil_burning)
    {
      tft.setTextColor(TFT_WHITE,TFT_RED);
      tft.print("BURNING");
    } else {
      tft.setTextColor(TFT_LIGHTGREY,TFT_DARKGREY);
      tft.print("NO BURN");
    }


    // Hot water
    tft.setCursor(15,40);
    tft.setTextSize(3);
    if (hot_water_on)
    {
      tft.setTextColor(TFT_WHITE,TFT_RED);
      tft.print("WATER");
    } else {
      tft.setTextColor(TFT_DARKGREY,TFT_BLACK);
      tft.print("WATER");
    }

    //HEATING

    tft.setCursor(145,40);
    tft.setTextSize(3);

    if (heating_on)
    {
      tft.setTextColor(TFT_WHITE,TFT_RED);
      tft.print("HEAT");
    } else {
      tft.setTextColor(TFT_DARKGREY,TFT_BLACK);
      tft.print("HEAT");
      
    }

    





    // Display the IP address
    tft.setTextSize(2);
    tft.setCursor(6,87);
    tft.setTextColor(TFT_GREENYELLOW,TFT_BLACK);
    tft.print(ip_address);

    // Display the time
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE,TFT_OLIVE);
    tft.fillRect(0,110,240,135,TFT_OLIVE);
    tft.setCursor(95,115);
    tft.print(current_time);



}


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

  heating_on=new_heating;
  hot_water_on=new_hot_water;

  req->send(200,"text/plain","OK");
}

void setup(void)
{
  pinMode(PIN_HOT_WATER,OUTPUT);
  pinMode(PIN_HEATING,OUTPUT);
  pinMode(PIN_OIL_BURNER,INPUT_PULLUP);

  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  tft.setTextSize(2);


  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  delay(1000);
  WiFi.begin(ssid, password);
  delay(1000);
  Serial.printf("Connecting to: %s\n",ssid);
  Serial.println("");

  // Wait for connection
  uint32_t wifi_timeout=millis()+40000; // 40 seconds, then it reboots
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis()>wifi_timeout)
    {
      esp_restart(); // Force restart if wifi hasn't connected by timeout
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  strncpy(ip_address,WiFi.localIP().toString().c_str(),19);
  Serial.println(ip_address);

  sprintf(temp_buff,"CONNECTED\nIP ADDR:\n%s",ip_address);
  show_message(temp_buff,5000);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is a sample response.\n"
                                      "\t* Use /set_state?heating=1&hot_water=0\n"
                                      "\t* use /update to change the firmware\n"
                                      "\t* use /getfreeheap to see memory state\n"
                                      "\t* use /restart to reboot");
  });

  server.on("/set_state",HTTP_GET,[](AsyncWebServerRequest *request)
  {
    set_heating_state(request);
  });

  server.on("/restart",HTTP_GET,[](AsyncWebServerRequest *request)
  {
    restart_esp();
  });

  
  server.on("/getfreeheap",HTTP_GET,[](AsyncWebServerRequest *request) {
    uint32_t free=ESP.getFreeHeap();
    sprintf(temp_buff,"Free heap=%d",free);
    request->send(200,"text/plain",temp_buff);
  });
    

  AsyncElegantOTA.begin(&server,ota_username,ota_password);    // Start AsyncElegantOTA
  server.begin();
  Serial.println("HTTP server started");
}





void loop(void)
{
  for (uint8_t i=0;i<60;i++)
  {
      if (millis()>(1000*60*60*24*1) || WiFi.status()!=WL_CONNECTED)  // Reboot 1x per day
      {
        restart_esp();
      }

      update_display(i==0);// Once a minute fetch the time

      tick_flag=!tick_flag;
      delay(1000);
  }

  
  
}
  