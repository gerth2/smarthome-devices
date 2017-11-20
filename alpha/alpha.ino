/*
 * Top level entry file for the device.
 *
 * Change Notes:
 *  Chris Gerth - 2017-11-18
 *     Created
 *  
 */



#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "WEMOS_DHT12.h"
#include "PubSubClient.h"
#include "avgFilter.h"

/* Set wifi creds from private file */
/*Swap this with your own file to define your home wifi creds */
#include "B:\Projects\wifi_defs\gerth_wifi.h" 

/* Web Server */
ESP8266WebServer WebServer ( 80 );
const char *ssid = HOME_WIFI_SSID;
const char *password = HOME_WIFI_PASS;
IPAddress LocalIP(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2,ALPHA_NODE);
IPAddress GatewayIP(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2,HA_ROUTER);
IPAddress SubnetMask(255, 255, 255, 0);

/* MQTT */
IPAddress MQTT_server(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2, MQTT_SERVER);
WiFiClient wclient;
PubSubClient MQTT_client(wclient, MQTT_server);

/* DHT Temp/Humidity sensor */
DHT12 DHT_sensor;
float Temp_degF_raw;
float Humidity_pct_raw;
float Temp_degF;
float Humidity_pct;
const float Temp_offset_degF = -2.5f; /* Tuned to align with a measured sensor */
const float Humidity_offset_pct = 0.0f; /* Assumed Correct */
boolean sensor_faulted = false;
FilterData Temp_filter;
FilterData Humidity_filter;

/* IO Constants */
#define LED_PIN  LED_BUILTIN 
#define LED_ON   0
#define LED_OFF  1

/* Scheduler constants */
unsigned long Loop_start_time_ms;
unsigned long Prev_loop_start_time_ms;

#define WEBSERVER_TASK_RATE_MS 1000
unsigned long Webserver_task_prev_run_time_ms;

#define SENSOR_SAMPLE_TASK_RATE_MS 100
unsigned long Sensor_sample_task_prev_run_time_ms;

#define MQTT_TASK_RATE_MS 10000
unsigned long MQTT_task_prev_run_time_ms;



/****************************************************************************************/
/**** Local Helper Functions                                                            */                            
/****************************************************************************************/

void printWebpageServed(void){
  Serial.println ( "Serving " + WebServer.uri() + " to " + WebServer.client().remoteIP().toString() );
  return;
}


/****************************************************************************************/
/**** MQTT Functions                                                                    */                            
/****************************************************************************************/



/****************************************************************************************/
/**** HTTP Request (Webpage) Handling functions                                         */                            
/****************************************************************************************/

/* 
 * Main (root) webpage
 */
void handleRoot() {
  char temp[600];
  char tempStr[10] = {0};
  char humidStr[10] = {0};
  char temprStr[10] = {0};
  char humidrStr[10] = {0};
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  snprintf ( temp, 600,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ALPHA Node</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>ALPHA Node</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Sensor Status: %s</p>\
    <p>Temperature: %s Deg F</p>\
    <p>Humidity: %s %%</p>\
    <p>Raw Temperature: %s Deg F</p>\
    <p>Raw Humidity: %s %%</p>\
  </body>\
</html>",

    hr, 
    min % 60, 
    sec % 60,
    (sensor_faulted ? "FAULTED" : "OK" ),
    dtostrf(Temp_degF, 3, 2, tempStr),
    dtostrf(Humidity_pct, 3, 2, humidStr),
    dtostrf(Temp_degF_raw, 3, 2, temprStr),
    dtostrf(Humidity_pct_raw, 3, 2, humidrStr)
  );

  printWebpageServed();
  WebServer.send ( 200, "text/html", temp );
}

///*
// * Simple SVG graph renderer for temperature (Unused)
// */
//void drawGraph() {
//  String out = "";
//  char temp[100];
//  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"800\" height=\"300\">\n";
//  out += "<rect width=\"800\" height=\"300\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
//  out += "<g stroke=\"black\">\n";
//  int y = rand() % 130;
//  for (int x = 10; x < 790; x+= 10) {
//    int y2 = rand() % 130;
//    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
//    out += temp;
//    y = y2;
//  }
//  out += "</g>\n</svg>\n";
//
//  printWebpageServed();
//  WebServer.send ( 200, "image/svg+xml", out);
//}

/* 
 * All-other fallback (404-ish) 
 */
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += WebServer.uri();
  message += "\nMethod: ";
  message += ( WebServer.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += WebServer.args();
  message += "\n";

  for ( uint8_t i = 0; i < WebServer.args(); i++ ) {
    message += " " + WebServer.argName ( i ) + ": " + WebServer.arg ( i ) + "\n";
  }

  printWebpageServed();
  WebServer.send ( 404, "text/plain", message );
}

/****************************************************************************************/
/**** Perodic tasks                                                                     */                            
/****************************************************************************************/

void SensorSampleTaskUpdate(void){

  byte ret_val = DHT_sensor.get();
  if( ret_val != 0 ) {
    Serial.print( "Error, could not read from sensor. Get() returned value " );
    Serial.println(ret_val);
    sensor_faulted = true;
  }
  else {
    Temp_degF_raw = DHT_sensor.fTemp + Temp_offset_degF;
    Humidity_pct_raw = DHT_sensor.humidity + Humidity_offset_pct;

    Temp_degF = AvgFilter_filter(&Temp_filter, Temp_degF_raw);
    Humidity_pct = AvgFilter_filter(&Humidity_filter, Humidity_pct_raw);

    //Serial.print("T_raw: ");
    //Serial.print(Temp_degF_raw);
    //Serial.print(" T: ");
    //Serial.print(Temp_degF);
    //Serial.print("  |  H_raw: ");
    //Serial.print(Humidity_pct_raw);
    //Serial.print(" H: ");
    //Serial.print(Humidity_pct);
    //Serial.println("");

    sensor_faulted = false;
  }

  
}

void WebServerTaskUpdate(void){

  if (WiFi.status() == WL_CONNECTED) {
    WebServer.handleClient();
  }

}

void MQTTTaskUpdate(void){

  char tempStr[10] = {0};
  char humidStr[10] = {0};

  digitalWrite ( LED_PIN, LED_ON );

  if (WiFi.status() == WL_CONNECTED) {

    if (!MQTT_client.connected()) {
      if (MQTT_client.connect("ALPHA")) {
        Serial.println("MQTT Connected");
      } 
    }

    if (MQTT_client.connected()){
      MQTT_client.publish("ALPHA/Temp_F",dtostrf(Temp_degF, 3, 2, tempStr));
      MQTT_client.publish("ALPHA/Humidity_Pct",dtostrf(Humidity_pct, 3, 2, humidStr));
      MQTT_client.loop();
    }

  }

  digitalWrite ( LED_PIN, LED_OFF );

}

/****************************************************************************************/
/**** Arduino top-level Functions                                                       */                            
/****************************************************************************************/

void setup ( void ) {
  Serial.begin ( 115200 );
  Serial.println();
  Serial.println ( "Beginning setup..." );

  /* Leave LED on throughout setup process for debug */
  pinMode ( LED_PIN, OUTPUT );
  digitalWrite ( LED_PIN, LED_ON );

  /* Connect to the home automation wifi network with fixed IP */
  WiFi.begin ( ssid, password );
  WiFi.config(LocalIP, GatewayIP, SubnetMask);
  WiFi.softAPdisconnect(true); //Disable the node as an access point
  Serial.println ( "Wifi hardware started, attempting to connect to AP..." );

  /* Wait for connection */
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

  /* Set up local webserver (useful for debugging the node directly) */
  Serial.println ( "Configuring WebServer..." );
  WebServer.on ( "/", handleRoot );
  WebServer.onNotFound ( handleNotFound );
  WebServer.begin();
  Serial.println ( "HTTP WebServer started" );

  /* Init & Configure the temp/humidity sensor */
  DHT_sensor.init();
  AvgFilter_init(&Temp_filter);
  AvgFilter_init(&Humidity_filter);
  Serial.println ( "Sensor init finished" );

  /* All done! */
  Serial.println ( "Init complete!" );
  digitalWrite ( LED_PIN, LED_OFF );

  
}



void loop ( void ) {

  /* We've implemented a super poor-man's scheduler */
  /*  Yah I could use an RTOS, but this was easier  */
  /*  for the limited scope I have... hopefully.    */
  Prev_loop_start_time_ms = Loop_start_time_ms;
  Loop_start_time_ms = millis();

  if(Prev_loop_start_time_ms > Loop_start_time_ms){
    /* Handle timer overflow - will probably hit this once every other month */
    /* This is so infrequent i'm not going to bother to be accurate. Just reset the task timers */
    /* This will definitely cause a glitch in timing, but this ain't no fancy-pants scheduler */
    Webserver_task_prev_run_time_ms = 0;
    Sensor_sample_task_prev_run_time_ms = 0;
  }

  /* Check if tasks need to be run */

  if(Sensor_sample_task_prev_run_time_ms + SENSOR_SAMPLE_TASK_RATE_MS < Loop_start_time_ms) {
    /* Time to run the sensor sample task */
    SensorSampleTaskUpdate();
    Sensor_sample_task_prev_run_time_ms = Loop_start_time_ms;

  }

  if(Webserver_task_prev_run_time_ms + WEBSERVER_TASK_RATE_MS < Loop_start_time_ms){
    /* Time to run the webserver sample task */
    WebServerTaskUpdate();
    Webserver_task_prev_run_time_ms = Loop_start_time_ms;
  }

  if(MQTT_task_prev_run_time_ms + MQTT_TASK_RATE_MS < Loop_start_time_ms){
    /* Time to run the MQTT update task */
    MQTTTaskUpdate();
    MQTT_task_prev_run_time_ms = Loop_start_time_ms;
  }


  
}

