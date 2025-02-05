#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#define WIFI_SSID "UW MPSK"
#define WIFI_PASSWORD "XXXXXX"

#define DATABASE_SECRET "XXXXXX"
#define DATABASE_URL "https://power-management-lab-default-rtdb.firebaseio.com/"

#define TRIG_PIN 5   // ultrasonic trigger 
#define ECHO_PIN 18  // ultrasonic echo 

// firebase setup
WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
LegacyToken dbSecret(DATABASE_SECRET);

// states
enum State {
    IDLE,
    ULTRASONIC_ONLY,
    ULTRASONIC_WIFI,
    ULTRASONIC_WIFI_FIREBASE,
    DEEP_SLEEP
};

State currentState = IDLE;
float distance;

void printError(int code, const String &msg);
void connectToWiFi();
void disconnectWiFi();
float ultrasonic();
void sendDataToFirebase(float distance);
void enterDeepSleep();
void idleStage();
void ultrasonicOnlyStage();
void ultrasonicWiFiStage();
void ultrasonicWiFiFirebaseStage();
void deepSleepStage();

void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    Serial.println("Serial initialized");

    // initialize Firebase
    initializeApp(client, app, getAuth(dbSecret));
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
    client.setAsyncResult(result);

    // deep sleep timer (5 seconds)
    esp_sleep_enable_timer_wakeup(5000000);
}

void loop() {
    // wake up from deep sleep and take an ultrasonic reading
    float newDistance = ultrasonic();
    Serial.print("Measured Distance: ");
    Serial.println(newDistance);

    // check if an object (or person) is detected
    if (abs(newDistance - distance) > 20) { // detects significant movement
        Serial.println("Movement Detected! Connecting to WiFi...");
        connectToWiFi();
        sendDataToFirebase(newDistance);
        disconnectWiFi();
    } else {
        Serial.println("No movement detected. Going back to sleep.");
    }

    distance = newDistance; // update last measured distance

    // enter deep sleep for 5 seconds before next check
    Serial.println("Entering Deep Sleep...");
    esp_sleep_enable_timer_wakeup(15000000); // Sleep for 15 seconds
    esp_deep_sleep_start();
}


void idleStage() {
    // **1. IDLE Mode (5s)**
    Serial.println("1. IDLE Mode");

    // ensure WiFi is disconnected
    if (WiFi.status() == WL_CONNECTED) {
        disconnectWiFi();
    }

    delay(5000); // Stay idle for 5 seconds

    // transition to the next stage
    currentState = ULTRASONIC_ONLY;
}

void ultrasonicOnlyStage() {
    // **2. ULTRASONIC_ONLY Mode (5s)**
    Serial.println("2. ULTRASONIC Sensor Active");
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // run for 5 seconds
        distance = ultrasonic();
        Serial.print("Measured Distance: ");
        Serial.println(distance);
        delay(50);
    }

    // transition to the next stage
    currentState = ULTRASONIC_WIFI;
}

void ultrasonicWiFiStage() {
    // **3. ULTRASONIC_WIFI Mode (5s)**
    Serial.println("3. Ultrasonic + WiFi Active");

    // connect to WiFi
    connectToWiFi();

    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // run for 5 seconds
        distance = ultrasonic();
        Serial.print("Measured Distance (WiFi Mode): ");
        Serial.println(distance);
        delay(50);
    }

    // transition to the next stage
    currentState = ULTRASONIC_WIFI_FIREBASE;
}

void ultrasonicWiFiFirebaseStage() {
    Serial.println("4. Ultrasonic + WiFi + Sending Data to Firebase (Optimized)");

    // initial ultrasonic sensor reading
    float newDistance = ultrasonic();
    Serial.print("Measured Distance: ");
    Serial.println(newDistance);

    // check if there's significant movement 
    if (abs(newDistance - distance) > 20) { // threshold for movement detection
        Serial.println("Movement detected! Connecting to WiFi...");

        connectToWiFi(); // only connect if movement is detected
        sendDataToFirebase(newDistance);
        disconnectWiFi(); // immediately disconnect after sending data

        Serial.println("Data sent. Going to Deep Sleep...");
    } else {
        Serial.println("No movement detected. Going to Deep Sleep...");
    }

    // update stored distance
    distance = newDistance;

    // enter deep sleep to save power
    esp_sleep_enable_timer_wakeup(5000000); // 5 seconds deep sleep
    esp_deep_sleep_start();
}

void deepSleepStage() {
    // **5. DEEP_SLEEP Mode**
    Serial.println("5. Entering Deep Sleep...");
    enterDeepSleep();
}

float ultrasonic() {
    const int numReadings = 5; // number of readings to average
    float totalDistance = 0;

    for (int i = 0; i < numReadings; i++) {
        // send pulse to ultrasonic sensor
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);

        // measure duration of echo pulse with a timeout
        long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout after 30ms
        // calculate distance in cm
        float distance = duration * 0.034 / 2;
        // ignore invalid readings
        if (duration > 0 && distance > 0 && distance < 400) { 
            totalDistance += distance;
        } else {
            i--; 
        }
        delay(20);
    }

    // calculate average distance
    float averageDistance = totalDistance / numReadings;

    // print the distance to serial monitor
    Serial.print("Filtered Distance: ");
    Serial.println(averageDistance);

    return averageDistance;
}

void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) { // 10s timeout
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected");
    } else {
        Serial.println("\nWiFi Connection Failed. Continuing...");
    }
}

void disconnectWiFi() {
    Serial.println("Disconnecting WiFi...");
    WiFi.disconnect();
    while (WiFi.status() == WL_CONNECTED) {
        delay(100); // wait until WiFi is fully disconnected
    }
    Serial.println("WiFi Disconnected");
}

void sendDataToFirebase(float distance) {
    ssl.setInsecure();

    Serial.println("Attempting to upload data to Firebase...");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, skipping Firebase upload.");
        return;
    }

    Serial.print("Uploading Distance: ");
    Serial.println(distance);

    bool status = Database.set<float>(client, "/ESP32/Distance", distance);

    if (status) {
        Serial.println("Distance uploaded to Firebase successfully!");
    } else {
        printError(client.lastError().code(), client.lastError().message());
    }
}

void enterDeepSleep() {
    Serial.println("Entering Deep Sleep...");
    esp_deep_sleep_start();
}

void printError(int code, const String &msg) {
    Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}

