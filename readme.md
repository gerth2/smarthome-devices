# Smarthome Devices 

This is some of the code for the custom smarthome devices I've been working on for my home. There are multiple projects here, one per node in the house - I have grouped them since they form a single functioning unit. 

# Current 

## Alpha 
Alpha is the upstairs ESP8266-based thermometer. It provides a simple web interface for debugging, and hooks to the MQTT server for display in HomeAssistant.

## Beta
Alpha is the main-floor ESP8266-based thermometer. It provides a simple web interface for debugging, and hooks to the MQTT server for display in HomeAssistant.

## Gamma 
Alpha is the downstairs ESP8266-based thermometer. It provides a simple web interface for debugging, and hooks to the MQTT server for display in HomeAssistant.

# Future/Planned 

## TBD 1
An ESP8266 with two accelerometers attached. The accelerometers are affixed to my washer and dryer, and detect vibration.  Based on the vibration pattern, they report a state to the MQTT server, which HomeAssistant can use to trigger actions (like sending me a text message).

## TBD 2
An ESP8266 connected to two split-core current sensors, placed around the Mains input to the house. At a minimum, track current/power usage in the house. More advanced usage might be identifying different power-consumption signatures.... to be seen...


# Project General Overview 

I have no idea where this system will evolve to in the future. The things I do know as of now:

  1) I like the ESP8266 platform - dirt cheap, easy to program, lots of Arduino-style library support.
  2) We'll continue to create nodes in the house, probably using the 8266 or similar. Each node will get its own project folder
  3) I have a rough idea of what I want on some of the nodes, but am not really sure yet. We'll leave the nodes with generic names for that reason.
  4) Each node will likely host a simple web interface for debugging the node itself.
  5) MQQT Seems pretty darn flexible. I'll likely use this as the mechanism for broadcasting data to other systems.
  6) HomeAssistant seems like an awesome choice for coordinating inter-device interaction. The nodes will contain only the logic needed to do extract the desired data from sensors, or perform final output. Inter-node coordination, or extra outside-world interaction (ex: text messages) will be done via HomeAssistant.


# Copyright/License/Stuff

Thanks to all the folks who've contributed software already that I was able to use to get this up and going. I've attempted to capture all your copyright notices in copyrights.txt. Let me know if I missed anyone. Thank you all so much! If we ever meet in person, I will buy you a beer.

Here's a quick list of the resources I've found useful online so far:

  * [WEMOS Arduino DHT library](https://github.com/wemos/WEMOS_DHT12_Arduino_Library)
  * [James Lewis's MQTT on ESP8266 tutorial](https://www.baldengineer.com/mqtt-tutorial.html)
  * [WEMOS Arduino setup](https://wiki.wemos.cc/tutorials:get_started:get_started_in_arduino)