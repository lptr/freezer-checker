#include <Arduino.h>

#include <ArduinoJson.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

#define DHTPIN D1
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
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

    dht.begin();

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

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float humidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float temperature = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
    }

    Serial.printf("Humidity: %.2f%% temperature: %.2f\n", humidity, temperature);

    DynamicJsonDocument doc(2048);
    String payload;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    serializeJson(doc, payload);

    client.publish("freezer/telemetry", payload.c_str());

    // Wait a few seconds between measurements.
    delay(5000);
}
