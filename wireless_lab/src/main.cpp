#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

#define trigPin 9 
#define echoPin 8

#define SOUND_SPEED 0.034

long duration;
float distance;

const int filterSize = 5; // Number of readings to average
float readings[filterSize]; // Array to store readings
int currentIndex = 0;              // Index for the current reading
float total = 0;            // Total of the readings
float average = 0;          // Moving average

#define SERVICE_UUID        "b3c12f14-d21d-4e4e-b991-cf6cee41c208"
#define CHARACTERISTIC_UUID "598d1680-b338-4ad5-96c7-a355b21f4754"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    for (int i = 0; i < filterSize; i++) {
        readings[i] = 0;
    }

    BLEDevice::init("XIAO_ESP32S3 - Marjorie");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);
    distance = duration * SOUND_SPEED / 2;

    if (distance > 600) {
        distance = 0;
    }

    total = total - readings[currentIndex];
    readings[currentIndex] = distance;
    total = total + readings[currentIndex];

    currentIndex = (currentIndex + 1) % filterSize;
    average = total / filterSize;

    Serial.print("Raw Distance (cm): ");
    Serial.println(distance);
    Serial.print("Filtered Distance (cm): ");
    Serial.println(average);

    delay(1000);

    if (deviceConnected && average < 30) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;  // Update the previousMillis
            char buffer[50];
            pCharacteristic->setValue((uint8_t*)&average, sizeof(average));
            pCharacteristic->notify();
            Serial.print("Notify value: ");
            Serial.println(average);
        }
    }

    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    delay(1000);
}
