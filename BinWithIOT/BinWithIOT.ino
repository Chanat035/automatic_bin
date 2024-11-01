#include <WiFiS3.h>
#include <MQTTClient.h>
#include <Servo.h>

// WiFi and MQTT configurations
const char WIFI_SSID[] = "your_wifi_ssid";
const char WIFI_PASSWORD[] = "your_wifi_password";
const char MQTT_BROKER_ADRRESS[] = "mqtt-dashboard.com";
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "arduino-uno-r4-client";
const char MQTT_USERNAME[] = "";
const char MQTT_PASSWORD[] = "";
const char PUBLISH_TOPIC[] = "phycom66070069";
const char SUBSCRIBE_TOPIC[] = "phycom66070069";

// Pin definitions for ultrasonic sensor and servo
const int TRIG_PIN = 10;
const int ECHO_PIN = 9;
const int SERVO_PIN = 8;
const int WASTE_TRIG_PIN = 7;
const int WASTE_ECHO_PIN = 6;

// Variables
long data;
int distance_cm;
int waste_level;
unsigned long lastPublishTime = 0;
const int PUBLISH_INTERVAL = 5000;  // Publish every 5 seconds

// Objects
WiFiClient network;
MQTTClient mqtt = MQTTClient(256);
Servo servo_bin;

void setup() {
  Serial.begin(9600);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(WASTE_TRIG_PIN, OUTPUT);
  pinMode(WASTE_ECHO_PIN, INPUT);
  
  // Initialize servo
  servo_bin.attach(SERVO_PIN);
  servo_bin.write(0);  // Initial position - closed
  
  // Connect to WiFi
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(10000);
  }
  
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  connectToMQTT();
}

void loop() {
  mqtt.loop();
  
  // Lid sensor reading
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  data = pulseIn(ECHO_PIN, HIGH);
  distance_cm = data * 0.034 / 2;
  Serial.print("Distance: ");
  Serial.println(distance_cm);

  // Control servo based on distance
  static unsigned long lidCloseTimer = 0; // Timer for closing the lid
  static bool lidOpen = false;            // Track if the lid is open

  if (distance_cm <= 10) {  // If an object is detected within 10 cm
    servo_bin.write(70);    // Open lid
    lidOpen = true;         // Set lid status as open
    lidCloseTimer = millis(); // Reset the timer
  } else if (lidOpen && millis() - lidCloseTimer >= 2000) {  // No object, wait 2 seconds
    servo_bin.write(0);     // Close lid after 2 sec
    lidOpen = false;        // Reset lid status
  }

  // Check waste level and publish to MQTT periodically
  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    checkWasteLevel();
    publishData();
    lastPublishTime = millis();
  }
}

void checkWasteLevel() {
  digitalWrite(WASTE_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(WASTE_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(WASTE_TRIG_PIN, LOW);
  
  long waste_data = pulseIn(WASTE_ECHO_PIN, HIGH);
  
  // Adjust the multiplier if needed based on your sensor's specifications
  waste_level = waste_data * 0.034 / 2;  // Distance in cm
  Serial.print("Waste Level Distance: ");
  Serial.println(waste_level);
  
  // Small delay to allow the sensor to stabilize
  delay(50);
}


void connectToMQTT() {
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);
  mqtt.onMessage(messageHandler);

  Serial.print("Connecting to MQTT broker");
  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("MQTT broker Timeout!");
    return;
  }

  if (mqtt.subscribe(SUBSCRIBE_TOPIC)) {
    Serial.println("Subscribed to: " + String(SUBSCRIBE_TOPIC));
  } else {
    Serial.println("Failed to subscribe to: " + String(SUBSCRIBE_TOPIC));
  }
  
  Serial.println("MQTT broker Connected!");
}

void publishData() {
  // Only publish waste_level distance to MQTT
  String message = "{\"waste_level\":" + String(waste_level) + "}";
  
  mqtt.publish(PUBLISH_TOPIC, message.c_str());
  Serial.println("Published: " + message);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("Received from MQTT: topic: " + topic + " | payload: " + payload);
  
  // Handle remote control if needed
  if (payload == "OPEN") {
    servo_bin.write(70);
  } else if (payload == "CLOSE") {
    servo_bin.write(0);
  }
}
