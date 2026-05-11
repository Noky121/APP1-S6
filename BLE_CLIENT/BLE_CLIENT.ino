#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

HardwareSerial UARTLink(2);

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRxCharacteristic = nullptr;

bool connected = false;
volatile bool newDataAvailable = false;

// =====================================================
// NOTIFICATION CALLBACK
// =====================================================
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  String data = "";

  for (int i = 0; i < length; i++) {
    data += (char)pData[i];
  }

  Serial.println(data);
  if (data.indexOf("NEW_DATA") >= 0){
    newDataAvailable = true;
  }
  else {

    Serial.println("=======================");
  }
}

// =====================================================
// CLIENT CALLBACKS
// =====================================================
class MyClientCallback : public BLEClientCallbacks {

  void onConnect(BLEClient* pclient) {

    Serial.println("BLE Connected");
  }

  void onDisconnect(BLEClient* pclient) {

    Serial.println("BLE Disconnected");

    connected = false;
  }
};

// =====================================================
// CONNECT TO SERVER
// =====================================================
void connectToServer(BLEAddress pAddress) {

  Serial.println("Connecting to server...");

  pClient = BLEDevice::createClient();

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect
  if (!pClient->connect(pAddress)) {

    Serial.println("Connection failed");

    connected = false;

    return;
  }

  // Increase MTU
  pClient->setMTU(200);

  // Get BLE service
  BLERemoteService* pRemoteService =
    pClient->getService(SERVICE_UUID);

  if (pRemoteService == nullptr) {

    Serial.println("Failed to find service UUID");

    pClient->disconnect();

    return;
  }

  // TX characteristic (notifications from server)
  BLERemoteCharacteristic* pTxCharacteristic =
    pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);

  if (pTxCharacteristic == nullptr) {

    Serial.println("Failed to find TX characteristic");

    pClient->disconnect();

    return;
  }

  // Enable notifications
  if (pTxCharacteristic->canNotify()) {

    pTxCharacteristic->registerForNotify(notifyCallback);

    Serial.println("Notifications enabled");
  }

  // RX characteristic (write requests to server)
  pRxCharacteristic =
    pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);

  if (pRxCharacteristic == nullptr) {

    Serial.println("Failed to find RX characteristic");

    pClient->disconnect();

    return;
  }

  connected = true;

  Serial.println("Connected to BLE server!");
}

// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  BLEDevice::init("Base Station E26");

  BLEDevice::setMTU(200);

  Serial.println("BLE Client Started");

  UARTLink.begin(115200, SERIAL_8N1, 25, 26);
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  // ==========================================
  // NOT CONNECTED -> SCAN
  // ==========================================
  if (!connected) {

    Serial.println("Scanning BLE...");

    BLEScan* scan = BLEDevice::getScan();

    scan->setActiveScan(true);

    BLEScanResults* results = scan->start(2, false);

    for (int i = 0; i < results->getCount(); i++) {

      BLEAdvertisedDevice device =
        results->getDevice(i);

      Serial.print("Found device: ");
      Serial.println(device.getName().c_str());

      // Find target server
      if (device.getName() == "UART Sensors Station E26") {

        Serial.println("Target device found!");

        connectToServer(device.getAddress());

        break;
      }
    }

    delay(2000);
  }

  // ==========================================
  // CONNECTED -> SEND REQUESTS
  // ==========================================
  else {
     if (newDataAvailable) {

      UARTLink.println("GET_DATA");
      Serial.println("Request sent to get Data");
      unsigned long start = millis();

      while (!UARTLink.available()) {
        if (millis() - start > 3000) {
          Serial.println("UART Timeout");
          return;
        }
      }

      String response = UARTLink.readStringUntil('\n');

      Serial.print("UART DATA RECEIVED:");
      Serial.println(response);

      newDataAvailable = false;
    }
  }
}
 



  //  // Safety check
  //   if (!pClient->isConnected()) {

  //     Serial.println("Connection lost");

  //     connected = false;

  //     return;
  //   }

  //   if (newDataAvailable) {

  //     uint8_t request = 1;

  //     pRxCharacteristic->writeValue(&request, 1);

  //     Serial.println("Request sent to sensors station");

  //     newDataAvailable = false;
  //   }
  // UARTLink.println("GET_DATA");

  // Serial.println("Request sent");

  // unsigned long start = millis();

  //   while (!UARTLink.available()) {

  //     if (millis() - start > 2000) {

  //       Serial.println("Timeout");

  //       return;
  //     }
  //   }

  //   String response = UARTLink.readStringUntil('\n');

  //   Serial.println("Received:");
  //   Serial.println(response);

  //   delay(3000);

  // }
