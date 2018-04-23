/*
 * Top level entry file for the device.
 *
 * Change Notes:
 *  Chris Gerth - 2018-04-22
 *     Created
 *  
 */



#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "PubSubClient.h"

/* Set wifi creds from private file */
/*Swap this with your own file to define your home wifi creds */
#include "B:\Projects\wifi_defs\gerth_wifi.h" 



/* IO Constants */
#define LED_PIN  LED_BUILTIN 
#define LED_ON   0
#define LED_OFF  1
#define DIST_SENSOR_PIN 0
#define RELAY_PIN 3
#define RELAY_ON 1
#define RELAY_OFF 0

/* State definitions */
enum door_states {
  DOOR_OPEN,           //Door is for sure open
  DOOR_IN_TRANSIT,     //Door should be in transit
  DOOR_CLOSED,         //Door is for sure closed
  DOOR_UNKNOWN,        //Not sure what door is doing
};

enum car_states {
  CAR_PRESENT,             //Car known to be in garage
  CAR_NOT_PRESENT,         //Car known to not to be in garage
  CAR_UNKNOWN              //Not sure if the car is there or not
};

enum door_sm_states {
  DSM_RELAY_ACTIVE,          //Relay active
  DSM_RELAY_INACTIVE_WAIT,   //Waiting after a trigger before handling next request
  DSM_RELAY_IDLE             //Waiting for the next trigger command cycle
};


/* Web Server */
ESP8266WebServer WebServer ( 80 );
const char *ssid = HOME_WIFI_SSID;
const char *password = HOME_WIFI_PASS;
IPAddress LocalIP(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2, EPSILON_NODE);
IPAddress GatewayIP(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2, HA_ROUTER);
IPAddress SubnetMask(255, 255, 255, 0);

/* MQTT */
IPAddress MQTT_server(HA_SUBNET0, HA_SUBNET1, HA_SUBNET2, MQTT_SERVER);
WiFiClient wclient;
PubSubClient MQTT_client(wclient);

/*LV MaxSonar EZ distance sensor - Analog */
#define DIST_IN_PER_ADC_CODES 60/170 //Determined empirically with a tape measure and my ceiling
boolean dist_sensor_online = false;
unsigned long dist_sensor_raw_adc = 0;
unsigned int  dist_sensor_val_in = 0;

/* Distance sensor interpretation */
#define DIST_SEN_TO_OPEN_DOOR_IN 24    //Distance from sensor face to door _when open_, plus a small margin.
#define DIST_SEN_TO_CAR_ROOF 48        //Distance from sensor face to roof of car, plus small margin
#define DIST_SEN_STATE_DEBOUNCE_MS 500 //State must be stable for a duration before the output actually changes
door_states in_door_state_raw = DOOR_UNKNOWN;
door_states in_door_state_dbn = DOOR_UNKNOWN;
car_states in_car_state_raw = CAR_UNKNOWN;
car_states in_car_state_dbn = CAR_UNKNOWN;

/* Door trigger relay */
#define RELAY_ON_TIME_MS 750
#define RELAY_OFF_MIN_TIME_MS 1500
unsigned int relay_user_cmd = false;
unsigned int relay_output_cmd = RELAY_OFF; //Commanded state to the relay

/* Door State Machine */
door_sm_states door_sm_state = DSM_RELAY_IDLE; //State machine internal state

/* Scheduler constants */
unsigned long Loop_start_time_ms;
unsigned long Prev_loop_start_time_ms;

#define WEBSERVER_TASK_RATE_MS 1000
unsigned long Webserver_task_prev_run_time_ms;

#define SENSOR_SAMPLE_TASK_RATE_MS 50
unsigned long Sensor_sample_task_prev_run_time_ms;

#define STATES_UPDATE_RATE_MS 100 
unsigned long States_update_prev_run_time_ms;

#define MQTT_TASK_RATE_MS 500 
unsigned long MQTT_task_prev_run_time_ms;



/****************************************************************************************/
/**** Local Helper Functions                                                            */                            
/****************************************************************************************/

void printWebpageServed(void){
  Serial.println ( "Serving " + WebServer.uri() + " to " + WebServer.client().remoteIP().toString() );
  return;
}

void handleTrigger(void){
  relay_user_cmd = true;
  Serial.println("User requested door toggle");
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
  char distStr[15] = {0};
  char doorStr[15] = {0};
  char carStr[15] = {0};
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  doorStateToStr(in_door_state_dbn, doorStr);
  carStateToStr(in_car_state_dbn, carStr);

  snprintf ( temp, 600,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>EPSILON Node</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>EPSILON Node</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Distance Sensor Status: %s</p>\
    <p>Distance Sensed : %s in</p>\
    <p>Door State: %s </p>\
    <p>Car State: %s </p>\
    <a href=\"/TOGGLE\"><button>Door Toggle</button></a></p>\
  </body>\
</html>",

    hr, 
    min % 60, 
    sec % 60,
    (dist_sensor_online ? "OK" : "FAULTED" ),
    dtostrf(dist_sensor_val_in, 3, 2, distStr),
    doorStr,
    carStr

  );

  printWebpageServed();
  WebServer.send ( 200, "text/html", temp );
}

void handleToggleReq() {
  char temp[600];
  snprintf ( temp, 600,
  "<html>\
    <head>\
      <meta http-equiv=\"refresh\" content=\"1; url=/\" />\
      <title>Door Toggle</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
    </head>\
    <body>\
      <h1>EPSILON Node</h1>\
      <br>Toggling door state...<br> \
    </body>\
  </html>"
  );
  
  WebServer.send ( 200, "text/html", temp );
  handleTrigger();
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

  static unsigned int debounce_loop_counter;
  static door_states in_door_state_raw_prev;
  static car_states in_car_state_raw_prev;

  // Sample sensor pulses. Note this will block for up to DIST_SENSOR_PULSE_TIMEOUT_US
  dist_sensor_raw_adc = analogRead(DIST_SENSOR_PIN);

  
  if(dist_sensor_raw_adc < 30){
    //Silly values, sensor unavailable
    dist_sensor_val_in = 0;
    dist_sensor_online = false;
  } else {
    //Reading was good. 
    dist_sensor_val_in = dist_sensor_raw_adc*DIST_IN_PER_ADC_CODES;
    dist_sensor_online = true;
  }

  Serial.print ( "Got sensor reading: " );
  Serial.println ( dist_sensor_val_in );

  if(dist_sensor_online){
    //Update prev states
    in_door_state_raw_prev = in_door_state_raw;
    in_car_state_raw_prev  = in_car_state_raw;

    //Eval new raw states
    if(dist_sensor_val_in < DIST_SEN_TO_OPEN_DOOR_IN){
      in_door_state_raw = DOOR_OPEN;
      in_car_state_raw = CAR_UNKNOWN;
    } else if(dist_sensor_val_in < DIST_SEN_TO_CAR_ROOF) {
      in_door_state_raw = DOOR_CLOSED;
      in_car_state_raw = CAR_PRESENT;
    } else {
      in_door_state_raw = DOOR_CLOSED;
      in_car_state_raw = CAR_NOT_PRESENT;
    }


    //Debounce raw state
    if((in_door_state_raw_prev != in_door_state_raw) || (in_car_state_raw_prev  != in_car_state_raw)){
      debounce_loop_counter = 30;
    } else {
      if(debounce_loop_counter != 0){
        debounce_loop_counter--;
      } else {
        in_door_state_dbn = in_door_state_raw;
        in_car_state_dbn  = in_car_state_raw;
      }
    }

  } else {
    //Immediately force states to unknown if sensor is unknown
    in_door_state_dbn = DOOR_UNKNOWN;
    in_car_state_dbn  = CAR_UNKNOWN;
  }
  
}

void WebServerTaskUpdate(void){

  if (WiFi.status() == WL_CONNECTED) {
    WebServer.handleClient();
  }

}

/* Utility function for converting a car State to a string */
void carStateToStr(car_states in, char * out){
  switch(in){
    case CAR_NOT_PRESENT:
      strcpy(out, "Away");
      break;
    case CAR_PRESENT:
      strcpy(out, "Home");
      break;
    case CAR_UNKNOWN:
    default:
      strcpy(out, "???");
  }
}

/* Utility function for converting an output door State to a string */
void doorStateToStr(door_states in, char * out){
  switch(in){
    case DOOR_CLOSED:
      strcpy(out, "Closed");
      break;
    case DOOR_IN_TRANSIT:
      strcpy(out, "...");
      break;
    case DOOR_OPEN:
      strcpy(out, "Open");
      break;
    case DOOR_UNKNOWN:
    default:
      strcpy(out, "???");
      break;
  }
}

void MQTTSubscriptionCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if(strcmp("EPSILON/DoorSwitch/CycleStartCmd", topic) == 0){
    //handle cycle start command
    if(length >= 4 && payload[0] == 'T'&& payload[1] == 'R'&& payload[2] == 'U'&& payload[3] == 'E'){
      handleTrigger();
    }
  }

}

void MQTTTaskUpdate(void){

  char doorStr[15] = {0};
  char carStr[15] = {0};

  digitalWrite ( LED_PIN, LED_ON );

  doorStateToStr(in_door_state_dbn, doorStr);
  carStateToStr(in_car_state_dbn, carStr);


  if (WiFi.status() == WL_CONNECTED) {

    if (!MQTT_client.connected()) {
      MQTT_client.setServer(MQTT_server, MQTT_SERVER_PORT);
      if (MQTT_client.connect("EPSILON")) {
        Serial.println("MQTT Connected");
        MQTT_client.subscribe("EPSILON/DoorSwitch/CycleStartCmd");
        MQTT_client.setCallback(MQTTSubscriptionCallback);
      } 
    }

    if (MQTT_client.connected()){
      MQTT_client.publish("EPSILON/Door", doorStr);
      MQTT_client.publish("EPSILON/Car",carStr);
      MQTT_client.publish("EPSILON/DoorSwitch/CycleActive",(door_sm_state==DSM_RELAY_IDLE)?"FALSE":"TRUE");
      MQTT_client.loop();
    }

  }

  digitalWrite ( LED_PIN, LED_OFF );

}

void StatesUpdateTaskUpdate(void){

  static unsigned long state_entry_timestamp = 0;

  door_sm_states next_state = door_sm_state;

  //Calc next-state
  switch(door_sm_state){
    case DSM_RELAY_IDLE:
      if(relay_user_cmd == true){
        //User wants to trigger the relay
        state_entry_timestamp = millis();
        relay_user_cmd = false;
        next_state = DSM_RELAY_ACTIVE;
      }
      break;
    case DSM_RELAY_ACTIVE:
      if(millis() > (state_entry_timestamp + RELAY_ON_TIME_MS)){
        //On cycle complete
        state_entry_timestamp = millis();
        next_state = DSM_RELAY_INACTIVE_WAIT;
      }
      break;
    case DSM_RELAY_INACTIVE_WAIT:
      if(millis() > (state_entry_timestamp + RELAY_OFF_MIN_TIME_MS)){
        //Off cycle complete
        next_state = DSM_RELAY_IDLE;
      }
      break;
    default:
      next_state = DSM_RELAY_IDLE;
  }

  door_sm_state = next_state;

  //Calc output
  if(door_sm_state == DSM_RELAY_ACTIVE){
    digitalWrite(RELAY_PIN, RELAY_ON);
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }
  

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
  WebServer.on ( "/TOGGLE", handleToggleReq );
  WebServer.onNotFound ( handleNotFound );
  WebServer.begin();
  Serial.println ( "HTTP WebServer started" );

  /* Init & Configure the relay & sensor */
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(DIST_SENSOR_PIN, INPUT);
  Serial.println ( "Sensor & Relay init finished" );

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

  if(States_update_prev_run_time_ms + STATES_UPDATE_RATE_MS < Loop_start_time_ms){
    /* Time to run the MQTT update task */
    StatesUpdateTaskUpdate();
    States_update_prev_run_time_ms = Loop_start_time_ms;
  }

  if(MQTT_task_prev_run_time_ms + MQTT_TASK_RATE_MS < Loop_start_time_ms){
    /* Time to run the MQTT update task */
    MQTTTaskUpdate();
    MQTT_task_prev_run_time_ms = Loop_start_time_ms;
  }


  
}

