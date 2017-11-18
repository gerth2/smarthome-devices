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

/* Set wifi creds from private file */
/*Swap this with your own file to define your home wifi creds */
#include "B:\Projects\wifi_creds\gerth_wifi.h" 
const char *ssid = HOME_WIFI_SSID;
const char *password = HOME_WIFI_PASS;

/* Web Server */
ESP8266WebServer WebServer ( 80 );

/* MTTQ */

/* DHT Temp/Humidity sensor */
DHT12 DHT_sensor;
float Temp_degF_raw;
float Temp_degF;
float Humidity_pct_raw;
float Humidity_pct;
const float Temp_offset_degF = -6.0f;
const float Humidity_offset_pct = 0.0f;

/* IO Constants */
#define LED_PIN  LED_BUILTIN 
#define LED_ON   0
#define LED_OFF  1

/* Scheduler constants */
unsigned long Loop_start_time_ms;
unsigned long Prev_loop_start_time_ms;

#define WEBSERVER_TASK_RATE_MS 1000
unsigned long Webserver_task_prev_run_time_ms;

#define SENSOR_SAMPLE_TASK_RATE_MS 500
unsigned long Sensor_sample_task_prev_run_time_ms;



/****************************************************************************************/
/**** Local Helper Functions                                                            */                            
/****************************************************************************************/

void printWebpageServed(void){
  Serial.println ( "Serving " + WebServer.uri() + " to " + WebServer.client().remoteIP().toString() );
  return;
}



/****************************************************************************************/
/**** HTTP Request (Webpage) Handling functions                                         */                            
/****************************************************************************************/

/* 
 * Main (root) webpage
 */
void handleRoot() {
  char temp[600];
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
    <p>Temperature: %d.%02d Deg F</p>\
    <p>Humidity: %d.%02d %%</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>",

    hr, 
    min % 60, 
    sec % 60,
    (int)(Temp_degF_raw), /*yucky way to have to format a float.*/
    (int)((Temp_degF_raw - floorf(Temp_degF_raw))*100.0),
    (int)(Humidity_pct_raw),
    (int)((Humidity_pct_raw - floorf(Humidity_pct_raw))*100.0)
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
  digitalWrite ( LED_PIN, LED_ON );

  Serial.println ( "Running sensor sample task" );
  byte ret_val = DHT_sensor.get();
  if( ret_val != 0 ) {
    Serial.print( "Error, could not read from sensor. Get() returned value " );
    Serial.println(ret_val);
  }
  else {
    Temp_degF_raw = DHT_sensor.fTemp;
    Humidity_pct_raw= DHT_sensor.humidity;
    Serial.print("Raw Sensor Values: ");
    Serial.print(Temp_degF_raw);
    Serial.print("degF Temp");
    Serial.print(Humidity_pct_raw);
    Serial.print("% Humidity");
    Serial.println("");
  }

  digitalWrite ( LED_PIN, LED_OFF );
}

void WebServerTaskUpdate(void){
  Serial.println ( "Running Webserver update task" );
  WebServer.handleClient();
}

/****************************************************************************************/
/**** Arduino top-level Functions                                                       */                            
/****************************************************************************************/

void setup ( void ) {
  Serial.begin ( 115200 );
  Serial.println ( "Beginning setup..." );

  pinMode ( LED_PIN, OUTPUT );
  digitalWrite ( LED_PIN, LED_ON );


  WiFi.begin ( ssid, password );
  Serial.println ( "Wifi hardware started, attempting to connect to AP" );

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

  Serial.println ( "Configuring WebServer..." );
  WebServer.on ( "/", handleRoot );
  WebServer.on ( "/test.svg", drawGraph );
  WebServer.onNotFound ( handleNotFound );
  WebServer.begin();
  Serial.println ( "HTTP WebServer started" );

  DHT_sensor.init();
  Serial.println ( "Sensor init'ed" );

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


  
}

