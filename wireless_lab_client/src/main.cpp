#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// TODO: change the service UUID to match your server's.
static BLEUUID serviceUUID("b3c12f14-d21d-4e4e-b991-cf6cee41c208");
// TODO: change the characteristic UUID to match your server's.
static BLEUUID charUUID("598d1680-b338-4ad5-96c7-a355b21f4754");

static boolean doConnect    = false;
static boolean connected    = false;
static boolean doScan       = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice*  myDevice;

// Global variables for data collection
float currentDistance = 0.0;
float minDistance     = 99999.0;    // start with a very large minimum
float maxDistance     = -99999.0;   // start with a very small maximum

// Update min and max with newly received distance
void updateDistanceData(float newDistance) {
  if (newDistance > maxDistance) {
    maxDistance = newDistance;
  }
  if (newDistance < minDistance) {
    minDistance = newDistance;
  }
}

// Notification callback
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify)
{
  // Since the server is sending a float in binary, we expect 4 bytes
  if (length == sizeof(float)) {
    float distanceVal;
    // Copy the raw bytes into our float variable
    memcpy(&distanceVal, pData, sizeof(distanceVal));

    // Update global currentDistance
    currentDistance = distanceVal;
    updateDistanceData(currentDistance);

    // Print current, min, and max
    Serial.println("=====================================");
    Serial.print("New Distance Received: ");
    Serial.println(distanceVal);
    Serial.print("  Minimum Distance: ");
    Serial.println(minDistance);
    Serial.print("  Maximum Distance: ");
    Serial.println(maxDistance);
    Serial.println("=====================================");
  } else {
    // If the length is not 4, it's not the float we expect
    Serial.print("Received data of unexpected length ");
    Serial.println(length);
  }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    // Called when connection is established
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());
  Serial.print(" - ");
  Serial.println(myDevice->getName().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remote BLE Server
  pClient->connect(myDevice);
  Serial.println(" - Connected to server");
  pClient->setMTU(517); // Request max MTU

  // Obtain a reference to the service we want
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the characteristic in the service
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Optional: read the initial value of the characteristic
  if (pRemoteCharacteristic->canRead()) {
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("Initial characteristic value: ");
    Serial.println(value.c_str());
  }

  // Register for notifications
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}

// Called for each BLE advertising device found
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if it has the service we want
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice  = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan    = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback for new devices
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false); // Scan for 5 seconds initially
}

void loop() {
  // If we found a device with our desired service, attempt connection
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothing more we will do.");
    }
    doConnect = false;
  }

  // If we're connected, we could do something else, 
  // but let's not overwrite the characteristic from the client side.
  if (connected) {
    // Example: we can still read the characteristic if needed
    // float, in this case, wouldn't parse well as a string, 
    // but you can do it in binary again if you prefer.
    
    // Delay to avoid spamming
    delay(1000); 
  } else if (doScan) {
    // Restart scanning after disconnect
    BLEDevice::getScan()->start(0);
  }

  delay(1000); // Delay a second between loops
}