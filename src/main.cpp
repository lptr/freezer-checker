#include <Arduino.h>

#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <I2CScanner.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (4096)
const char* mqtt_server = "10.180.1.194";

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();

    // Switch on the LED if an 1 was received as first character
    if ((char) payload[0] == '1') {
        digitalWrite(LED_BUILTIN, LOW);    // Turn the LED on (Note that LOW is the voltage level
                                           // but actually the LED is on; this is because
                                           // it is active low on the ESP-01)
    } else {
        digitalWrite(LED_BUILTIN, HIGH);    // Turn the LED off by making the voltage HIGH
    }
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "freezer-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("freezer/connect", "{ \"connected\": true }");
            // ... and resubscribe
            client.subscribe("freezer/config");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("Booting...");

    Wire.begin();

    I2CScanner scanner;
    scanner.Scan();

    if (!bme.begin(BME280_ADDRESS_ALTERNATE)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x");
        Serial.println(bme.sensorID(), 16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
    }

    WiFi.mode(WIFI_STA);

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //reset settings - wipe credentials for testing
    //wifiManager.resetSettings();

    if (!wifiManager.autoConnect("freezer")) {
        Serial.println("Failed to connect");
    } else {
        Serial.println("connected...yeey :)");
    }

    randomSeed(micros());
    pinMode(LED_BUILTIN, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    float humidity = bme.readHumidity();
    float temperature = bme.readTemperature();
    float pressure = bme.readPressure();
    float altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);

    Serial.printf("Temperature: %.3f%%, humidity: %.3f, pressure: %.3f, altitude: %.3f\n", temperature, humidity, pressure, altitude);

    DynamicJsonDocument doc(2048);
    String payload;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["pressure"] = pressure;
    doc["altitude"] = altitude;
    serializeJson(doc, payload);

    client.publish("freezer/telemetry", payload.c_str());

    delay(5000);
}
