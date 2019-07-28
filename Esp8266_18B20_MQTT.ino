#include "Credentials.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

WiFiClient espClient;
unsigned long reconnectionPeriod = 60000; //miliseconds
unsigned long lastBrokerConnectionAttempt = 0;
unsigned long lastWifiConnectionAttempt = 0;

const int GREEN_LED = 12;
const int BLUE_LED = 13;
const int RED_LED = 15;
const int RELAY_PIN = 16;
int green = 0;
int blue = 0;
int red = 0;
bool playLed = false;

PubSubClient client(espClient);
long lastTempMsg = 0;
long lastLightMsg = 0;
char msg[50];
int LDRLevel = 0;
long tempSensorRequestPeriod = 600000; //miliseconds

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 14

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
// arrays to hold device address
DeviceAddress thermometer;


void setup() {
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  client.setServer(SERVER_IP, MQTT_PORT);
  client.setCallback(callback);
  startWiFi();
}

void loop() {
  if (!client.connected()) {
    reconnectToBroker();
  }
  client.loop();
  sendTempLightToMqtt();
  if (playLed) {
    RGB_color(red, green, blue);
  }
}

void reconnectToBroker() {
  long now = millis();
  if (now - lastBrokerConnectionAttempt > reconnectionPeriod) {
    lastBrokerConnectionAttempt = now;
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        if (!client.connected()) {
          connectToBroker();
        }
      }
      else
      {
        reconnectWifi();
      }
    }
  }
}


//Connection to MQTT broker
void connectToBroker() {
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect("WittyCloud1")) {
    Serial.println("connected");
    // Once connected, publish an announcement...
    client.publish("WittyCloud1/status", "WittyCloud1 connected");
    // ... and resubscribe
    client.subscribe("WittyCloud1/relay");
    client.subscribe("WittyCloud1/setReadTempPeriod");
    client.subscribe("WittyCloud1/green");
    client.subscribe("WittyCloud1/blue");
    client.subscribe("WittyCloud1/red");
    client.subscribe("WittyCloud1/playLed");
  }
  else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 60 seconds");
  }
}


void startWiFi() {
  int connStartSec = 0;
  Serial.print(F("\r\nConnecting to: "));
  Serial.println(String(SSID));
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); Serial.print(".");
    if (connStartSec > 5) {
      WiFi.disconnect();
      Serial.println("Cant connect WiFi :(");
      return;
    }
    connStartSec++;
  }
  Serial.println("WiFi connected");
}


void reconnectWifi() {
  long now = millis();
  if (now - lastWifiConnectionAttempt > reconnectionPeriod) {
    lastWifiConnectionAttempt = now;
    startWiFi();
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("<<<");
  if (strcmp(topic, "WittyCloud1/relay") == 0) {
    // Switch on the RELAY if an 1 was received as first character
    if ((char)payload[0] == '1') {
      digitalWrite(RELAY_PIN, LOW);   // Turn the RELAY on
      digitalWrite(GREEN_LED, HIGH);  // Turn the RELAY off
    }
    if ((char)payload[0] == '0') {
      digitalWrite(RELAY_PIN, HIGH);  // Turn the RELAY off
      digitalWrite(GREEN_LED, LOW);  // Turn the RELAY off
    }
  }
  if (strcmp(topic, "WittyCloud1/setReadTempPeriod") == 0) {
    String myString = String((char*)payload);
    tempSensorRequestPeriod = myString.toInt();
    Serial.println(tempSensorRequestPeriod);
  }
  if (strcmp(topic, "WittyCloud1/green") == 0) {
    String myString = String((char*)payload);
    green = payloadToInt(payload, length);
    Serial.print("Green - ");
    Serial.println(green);
  }
  if (strcmp(topic, "WittyCloud1/blue") == 0) {
    String myString = String((char*)payload);
    blue = payloadToInt(payload, length);
    Serial.print("Blue - ");
    Serial.println(blue);
  }
  if (strcmp(topic, "WittyCloud1/red") == 0) {
    String myString = String((char*)payload);
    red = payloadToInt(payload, length);
    Serial.print("Red - ");
    Serial.println(red);
  }
  if (strcmp(topic, "WittyCloud1/playLed") == 0) {
    // Switch on the RELAY if an 1 was received as first character
    if ((char)payload[0] == '1') {
      playLed = true;
    }
    if ((char)payload[0] == '0') {
      playLed = false;
      RGB_color(0, 0, 0);
    }
  }
}

int payloadToInt(byte *payload, unsigned int len) {
  char buff_p[len];
  for (int i = 0; i < len; i++)
  {
    Serial.print((char)payload[i]);
    buff_p[i] = (char)payload[i];
  }
  buff_p[len] = '\0';
  String msg_p = String(buff_p);
  int val = msg_p.toInt(); // to Int
  return val;
}


void sendTempLightToMqtt() {
  long now = millis();
  if (now - lastTempMsg > tempSensorRequestPeriod) {
    lastTempMsg = now;
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    Serial.print("Publish message temp: ");
    Serial.println(temp);
    if (-20 < temp && temp < 50)
    {
      client.publish("WittyCloud1/temp", String(temp).c_str());
    }
  }
  if (now - lastLightMsg > 2000) {
    lastLightMsg = now;
    int sensorRead = analogRead(0); // read the value from the sensor
    if (LDRLevel != sensorRead)
    {
      LDRLevel = sensorRead;
      Serial.print("Publish message light: ");
      Serial.println(LDRLevel);
      client.publish("WittyCloud1/light", String(LDRLevel).c_str());
    }
  }
}

void RGB_color(int red_light_value, int green_light_value, int blue_light_value) {
  analogWrite(RED_LED, red_light_value);
  analogWrite(GREEN_LED, green_light_value);
  analogWrite(BLUE_LED, blue_light_value);
}
