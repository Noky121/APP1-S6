#include <Wire.h>
#include <Adafruit_DPS310.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

Adafruit_DPS310 dps;

// ---------------- PINS ----------------
const int capteurPin =   34;  // Lumiere
const int pinWindDir =   35;  // Direction 
const int pinWindSpeed = 27;  // Vitesse 
const int pinRain =      23;  // Pluie 

// ---------------- VARIABLES ----------------
volatile int windClicks = 0;
volatile int rainClicks = 0;

unsigned long lastWindMeasure = 0;
unsigned long lastSensorRead = 0;

float luxValue = 0;
float humidifyValue = 0;
float temperatureValue = 0;
float pressureValue = 0;
float windDirValue = 0;
float windSpeedValue = 0;
float rainValue = 0;


// ----------BLE Configuration --------------
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {

    std::string value = pCharacteristic->getValue();
    Serial.println("Requête reçue du client");
    if (value.length() > 0 && value[0] == 1) {
      String dataToSend =
        "L:"  + String(luxValue, 1) + "lx" +
        "|H:" + String(humidifyValue, 1) + "%" +
        "|T:" + String(temperatureValue, 1) + "C" +
        "|P:" + String(pressureValue, 0) + "Pa" +
        "|WS:" + String(windSpeedValue, 1) + "m/s" +
        "|WD:" + String(windDirValue, 0) + "deg" +
        "|R:" + String(rainValue, 2) + "mm";

      Serial.println("Envoi BLE : " + dataToSend);

      pTxCharacteristic->setValue(dataToSend.c_str());
      pTxCharacteristic->notify();
    }
  }
};

void IRAM_ATTR countWind() { windClicks++; }
void IRAM_ATTR countRain() { rainClicks++; }

// =====================================================
// SETUP
// =====================================================
void setup(){
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initialisation station meteo...");

  // ---------- Pins ----------
  pinMode(capteurPin, INPUT);
  pinMode(pinWindDir, INPUT);

  pinMode(pinWindSpeed, INPUT); 
  pinMode(pinRain, INPUT);
  
  // ---------- Interruptions ----------
  attachInterrupt(digitalPinToInterrupt(pinWindSpeed), countWind, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinRain), countRain, FALLING);

  // ---------- I2C ----------
  Wire.begin();

  // ---------- DPS310 ----------
  if (!dps.begin_I2C()) {
    Serial.println("Capteur DPS310 non détecté !");
    while (1);
  }

  Serial.println("DPS310 prêt !");
  Serial.println("Station meteo termine...");

  Serial.println("Initialisation Sensors Station E26...");
  BLEDevice::init("UART Sensors Station E26");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // TX Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                      );

  pTxCharacteristic->addDescriptor(new BLE2902());

  // RX Characteristic
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_RX,
                        BLECharacteristic::PROPERTY_WRITE
                      );

  pService->start();

  pServer->getAdvertising()->start();

  Serial.println("BLE started.");

  Serial.println("Setup termine.");
}

void loop () {
   // Lecture générale toutes les 2 secondes
  if (millis() - lastSensorRead >= 2000) {

    Serial.println("\n==============================");

    lumino_reader();

    humidify_reader();

    barometer_reader();

    wind_water_orientation_sensor();

    Serial.println("==============================");

    if (deviceConnected) {
      pTxCharacteristic->setValue("Nouvelles données disponibles");
      pTxCharacteristic->notify();
    }

    lastSensorRead = millis();
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("Started advertising again...");
    oldDeviceConnected = false;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = true;
  }

}

// =====================================================
// Ensoleillement
// =====================================================
void lumino_reader() {
  int valeur = analogRead(capteurPin);
  
  Serial.print("Intensite lumineuse RAW: ");
  Serial.print(valeur);

  float voltage = (valeur / 4095.0) * 3.3;

  float resistance = 10000.0 * ((3.3 / voltage) - 1.0);

  luxValue = pow((500000.0 / resistance), 1.4);

  // // exemple de calibration
  // float lux = 0.1 * valeur;  
  Serial.print("Voltage : ");
  Serial.print(voltage);
  Serial.println(" V");

  Serial.print("Resistance LDR : ");
  Serial.print(resistance);
  Serial.println(" ohms");

  Serial.print("Ensoillement : " );
  Serial.print(luxValue);
  Serial.println(" lux");
}

// =====================================================
// HUMIDITE / TEMPERATURE (DHT11)
// =====================================================
void humidify_reader(){
    int i, j;
    int duree[42];
    unsigned long pulse;
    byte data[5];
    float humidite;
    float temperature;
    int broche = 16;

    delay(1000);
    
    pinMode(broche, OUTPUT_OPEN_DRAIN);
    digitalWrite(broche, HIGH);
    delay(250);
    digitalWrite(broche, LOW);
    delay(20);
    digitalWrite(broche, HIGH);
    delayMicroseconds(40);
    pinMode(broche, INPUT_PULLUP);
    
    while (digitalRead(broche) == HIGH);
    i = 0;

    do {
          pulse = pulseIn(broche, HIGH);
          duree[i] = pulse;
          i++;
    } while (pulse != 0);
  
    if (i != 42) 
      Serial.printf(" Erreur timing \n"); 

    for (i=0; i<5; i++) {
      data[i] = 0;
      for (j = ((8*i)+1); j < ((8*i)+9); j++) {
        data[i] = data[i] * 2;
        if (duree[j] > 50) {
          data[i] = data[i] + 1;
        }
      }
    }

    if ( (data[0] + data[1] + data[2] + data[3]) != data[4] ) 
      Serial.println(" Erreur checksum");

    humidifyValue = data[0] + (data[1] / 256.0);
    temperatureValue = data [2] + (data[3] / 256.0);
    Serial.printf(" Humidite = %4.0f \%%  Temperature = %4.2f degre \n", humidifyValue, temperatureValue);
}

// =====================================================
// DPS310
// =====================================================
void barometer_reader(){
  sensors_event_t temp_event, pressure_event;

  // Lecture des deux mesures
  dps.getEvents(&temp_event, &pressure_event);

  float temperature = temp_event.temperature;   // °C
  pressureValue = pressure_event.pressure * 100;     // Pa

  Serial.print("Température: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Pression: ");
  Serial.print(pressureValue);
  Serial.println(" Pa");
}

// =====================================================
// VENT + PLUIE
// =====================================================
void wind_water_orientation_sensor(){
    noInterrupts();

    int wind = windClicks;
    int rain = rainClicks;

    windClicks = 0;
    rainClicks = 0;

    interrupts();

    // Calculs
    windSpeedValue = (wind / 2.0) * 2.4; 

    rainValue = rain * 0.2794;

    int dirRaw = analogRead(pinWindDir);
    windDirValue = getWindDirection(dirRaw);
    // Affichage
    Serial.print("Vitesse Vent : "); Serial.print(windSpeedValue); Serial.println(" km/h");
    Serial.print("Direction (Raw) : "); Serial.println(dirRaw);
    Serial.print("Direction : "); Serial.println(windDirValue);
    Serial.print("Pluie : "); Serial.print(rainValue); Serial.println(" mm");
}

// =====================================================
// DIRECTION VENT
// =====================================================
String getWindDirection(int value) {
  if (value < 500) return "N";
  else if (value < 1000) return "NE";
  else if (value < 1500) return "E";
  else if (value < 2000) return "SE";
  else if (value < 2500) return "S";
  else if (value < 3000) return "SO";
  else if (value < 3500) return "O";
  else return "NO";
}
