#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEClient* pClient;
BLERemoteCharacteristic* pRxCharacteristic;
bool connected = false;

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
  Serial.println("=======================");
}


void connectToServer(BLEAddress pAddress) {
  pClient = BLEDevice::createClient();
  Serial.println("Connecting to server...");
  
  pClient->connect(pAddress);
  pClient->setMTU(200);
  
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Failed to find our service UUID");
    return;
  }

  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Failed to find TX characteristic");
    return;
  }

  if(pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  pRxCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
  if (pRxCharacteristic == nullptr) {
    Serial.println("Failed to find RX characteristic");
    return;
  }

  connected = true;
  Serial.println("Connected to BLE server!");
}


void setup() {
  Serial.begin(115200);
  BLEDevice::init("Base Station E26");
  BLEDevice::setMTU(200);
}

void loop() {

  if (!connected) {

    Serial.println("Scan BLE...");

    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);

    BLEScanResults* results = scan->start(2, false);

    for (int i = 0; i < results->getCount(); i++) {

      BLEAdvertisedDevice device = results->getDevice(i);

      if (device.getName() == "UART Sensors Station E26") {

        connectToServer(device.getAddress());
        break;
      }
    }

    delay(2000);
  }
  else {

    // ===============================
    // REQUÊTE UART VERS CAPTEUR
    // ===============================
    uint8_t request = 1;  // commande simple "GET DATA"

    pRxCharacteristic->writeValue(&request, 1);

    Serial.println("Requête envoyée au capteur");

    delay(2000); // période de polling
  }
}