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
#include "PubSubClient.h"
#include "avgFilter.h"

#include "I2Cdev.h"
#include "MPU6050.h"

#include <limits.h>

#include "WebSocketsServer.h"

/* Set wifi creds from private file */
/*Swap this with your own file to define your home wifi creds */
#include "B:\Projects\wifi_defs\gerth_wifi.h" 

/* Web Server */
ESP8266WebServer WebServer ( 80 );
const char *ssid = HOME_WIFI_SSID;
const char *password = HOME_WIFI_PASS;
IPAddress LocalIP(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2,DELTA_NODE);
IPAddress GatewayIP(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2,HA_ROUTER);
IPAddress SubnetMask(255, 255, 255, 0);
WebSocketsServer webSocket = WebSocketsServer(81);
byte connectedClient = -1;

/* MQTT */
IPAddress MQTT_server(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2, MQTT_SERVER);
WiFiClient wclient;
PubSubClient MQTT_client(wclient, MQTT_server);

/* Motion Sensor */
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
#include "Wire.h"


/* IO Constants */
#define LED_PIN  LED_BUILTIN 
#define LED_ON   0
#define LED_OFF  1

/* Scheduler constants */
unsigned long Loop_start_time_ms;
unsigned long Prev_loop_start_time_ms;

#define WEBSERVER_TASK_RATE_MS 100
unsigned long Webserver_task_prev_run_time_ms;

#define SENSOR_SAMPLE_TASK_RATE_MS 20
unsigned long Sensor_sample_task_prev_run_time_ms;

#define MQTT_TASK_RATE_MS 100
unsigned long MQTT_task_prev_run_time_ms;



/****************************************************************************************/
/**** Local Helper Functions                                                            */                            
/****************************************************************************************/

void printWebpageServed(void){
  Serial.println ( "Serving " + WebServer.uri() + " to " + WebServer.client().remoteIP().toString() );
  return;
}

/****************************************************************************************/
/**** Websocket Functions                                                                    */                            
/****************************************************************************************/

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    
        switch(type) {
            case WStype_DISCONNECTED:
                Serial.printf("[%u] Disconnected!\n", num);
                break;
            case WStype_CONNECTED:
                {
                    IPAddress ip = webSocket.remoteIP(num);
                    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                    
                    // send message to client
                    webSocket.sendTXT(num, "Connected");
                }
                break;
            case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);
    
                // send message to client
                // webSocket.sendTXT(num, "message here");
    
                // send data to all connected clients
                // webSocket.broadcastTXT("message here");
                break;
            case WStype_BIN:
                Serial.printf("[%u] get binary length: %u\n", num, length);
    
                // send message to client
                // webSocket.sendBIN(num, payload, length);
                break;
        }
    
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
  char temp[3000];
  char tempStr[10] = {0};
  char humidStr[10] = {0};
  char temprStr[10] = {0};
  char humidrStr[10] = {0};
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  snprintf ( temp, 3000,

"<html>\
  <head>\
    <!--<meta http-equiv='refresh' content='5'/>-->\
    <title>DELTA Node</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
    <script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>\
    <script type='text/javascript'>\n\
      var port = '81';\n\
      var hostname = window.location.hostname+':'+port;\n\
      var dataSocket = new WebSocket('ws://'+hostname);\n\
      var numTransmissions = 0;\n\
      var data;\n\
      var options;\n\
      var chart;\n\
      console.log(hostname);\n\
      google.charts.load('current', {'packages':['corechart']});\n\
      google.charts.setOnLoadCallback(drawChart);\n\
      function drawChart() {\n\
        data = google.visualization.arrayToDataTable([\n\
          ['Time', 'AcelX', 'AcelY', 'AcelZ', 'GyroX', 'GyroY', 'GyroZ'],\n\
          [0,0,0,0,0,0,0]\n\
        ]);\n\
        options = {\n\
          title: 'RT Data',\n\
          curveType: 'none',\n\
          legend: { position: 'bottom' },\n\
          explorer: {},\n\
          hAxis: { 0:{title: 'Accel (g)'}, 1:{title: 'RotVel (deg/sec)'}},\n\
          vAxes: { 0:{title: 'Accel (g)'}, 1:{title: 'RotVel (deg/sec)'}},\n\
          series: {0: {targetAxisIndex:0}, 1:{targetAxisIndex:0}, 2:{targetAxisIndex:0},\n\
                   3: {targetAxisIndex:1}, 4:{targetAxisIndex:1}, 5:{targetAxisIndex:1}}\n\
        };\n\
        chart = new google.visualization.LineChart(document.getElementById('curve_chart'));\n\
        chart.draw(data, options);\n\
      }\n\
      dataSocket.onopen = function (event) {\n\
        document.getElementById('id01').innerHTML = 'COM Status: Socket Open';\n\
      };\n\
      dataSocket.onerror = function (event) {\n\
        document.getElementById('id01').innerHTML = 'COM Status: Socket ERROR!';\n\
      };\n\
      dataSocket.onclose = function (event) {\n\
        document.getElementById('id01').innerHTML = 'COM Status: Socket Closed';\n\
      };\n\
      dataSocket.onmessage  = function (event) {\n\
        console.log(event.data);\n\
        numTransmissions++;\n\
        document.getElementById('id02').innerHTML = numTransmissions.toString();\n\
        var dataRow =  event.data.split(',').map(function(item) { return parseFloat(item, 10)});\n\
        data.addRow(dataRow);\n\
        if(numTransmissions %% 10 == 0){\n\
          chart.draw(data, options);\n\
        }\n\
      };\n\
    </script>\n\
  </head>\
  <body>\
    <h1>DELTA Node</h1>\
    <div id='id01'>COM Status: Not Yet Opened...</div>\
    Update Count:<div id='id02'>0</div>\
    <div id='curve_chart' style='width: 900px; height: 500px'></div>\
  </body>\
</html>"
);

  printWebpageServed();
  WebServer.send ( 200, "text/html", temp );
}

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
float convRawToG(int ra){
  return ((float)(ra))/16384.0; //from datasheet, assuming 2G range
}

float convRawToDegPerSec(int rg){
  return ((float)(rg))/131.0; //from datasheet, assuming 250deg/sec range
}


void SensorSampleTaskUpdate(void){
   char finalMsg[300];
   char tmp0[20];
   char tmp1[20];
   char tmp2[20];
   char tmp3[20];
   char tmp4[20];
   char tmp5[20];
   char tmp6[20];


    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);


    snprintf ( finalMsg, 300,
               "%s, %s, %s, %s, %s, %s, %s",
               dtostrf((float)millis()/1000.0,19,4,tmp0), 
               dtostrf(convRawToG(ax),19,4,tmp1), 
               dtostrf(convRawToG(ay),19,4,tmp2), 
               dtostrf(convRawToG(az),19,4,tmp3), 
               dtostrf(convRawToDegPerSec(gx),19,4,tmp4), 
               dtostrf(convRawToDegPerSec(gy),19,4,tmp5), 
               dtostrf(convRawToDegPerSec(gz),19,4,tmp6)
    );

    webSocket.broadcastTXT(finalMsg);

    //Serial.print("a/g:\t");
    //Serial.print(ax); Serial.print("\t");
    //Serial.print(ay); Serial.print("\t");
    //Serial.print(az); Serial.print("\t");
    //Serial.print(gx); Serial.print("\t");
    //Serial.print(gy); Serial.print("\t");
    //Serial.println(gz);
  
}

void WebServerTaskUpdate(void){

  if (WiFi.status() == WL_CONNECTED) {
    WebServer.handleClient();
    webSocket.loop();
  }

}

void MQTTTaskUpdate(void){


  digitalWrite ( LED_PIN, LED_ON );

  if (WiFi.status() == WL_CONNECTED) {

    if (!MQTT_client.connected()) {
      if (MQTT_client.connect("DELTA")) {
        Serial.println("MQTT Connected");
      } 
    }

    if (MQTT_client.connected()){
      
      //TODO: Publish qualified values
      //MQTT_client.publish("DELTA/Temp_F",dtostrf(Temp_degF, 3, 2, tempStr));
      //MQTT_client.publish("DELTA/Humidity_Pct",dtostrf(Humidity_pct, 3, 2, humidStr));
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
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  WebServer.on ( "/", handleRoot );
  WebServer.onNotFound ( handleNotFound );
  WebServer.begin();
  Serial.println ( "HTTP WebServer started" );

  /* Init & Configure the accel/gyro motion sensor */
  Wire.begin();
  accelgyro.initialize();
  accelgyro.setFullScaleGyroRange(MPU6050_GYRO_FS_250); //Force 250deg/sec max range setting
  accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_2); //Force 2G max accel setting
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
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

