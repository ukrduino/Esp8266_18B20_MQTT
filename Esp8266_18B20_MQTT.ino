#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Update these with values suitable for your network.
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "192.168.0.112";

const int GREEN_LED = 12;
const int RELAY_PIN = 16;
const int BUZZ_PIN = 5;

WiFiClient espClient;
PubSubClient client(espClient);
long lastTempMsg = 0;
long lastLightMsg = 0;
char msg[50];
int LDRLevel = 0;
long tempSensorRequestPeriod = 60000;

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
	pinMode(RELAY_PIN, OUTPUT);
	pinMode(BUZZ_PIN, OUTPUT);
	digitalWrite(RELAY_PIN, HIGH);
	Serial.begin(115200);
	setup_wifi();
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
}


void setup_wifi() {

	delay(50);
	// We start by connecting to a WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println("-----");
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
}


void reconnect() {
	// Loop until we're reconnected
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect("WittyCloud1")) {
			Serial.println("connected");
			// Once connected, publish an announcement...
			client.publish("WittyCloud1/status", "WittyCloud1 connected");
			// ... and resubscribe
			client.subscribe("WittyCloud1/relay");
			client.subscribe("WittyCloud1/setReadTempPeriod");
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void loop() {
	if (!client.connected()) {
		reconnect();
	}
	client.loop();
	sendTempLightToMqtt();
}

void sendTempLightToMqtt() {
	long now = millis();
	if (now - lastTempMsg > tempSensorRequestPeriod) {
		lastTempMsg = now;
		sensors.requestTemperatures();
		float temp = sensors.getTempCByIndex(0);
		Serial.print("Publish message temp: ");
		Serial.println(temp);
		client.publish("WittyCloud/temp", String(temp).c_str());
	}
	if (now - lastLightMsg > 2000) {
		lastLightMsg = now;
		int sensorRead = analogRead(0); // read the value from the sensor
		if (LDRLevel != sensorRead)
		{
			LDRLevel = sensorRead;
			Serial.print("Publish message light: ");
			Serial.println(LDRLevel);
			client.publish("WittyCloud/light", String(LDRLevel).c_str());
		}
	}
}
