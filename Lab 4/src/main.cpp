#include <Arduino.h>
#include <BLEDevice.h>
#include <BLE2902.h> // assists in automatic updates rather than having phone confirm sync for each change

// for using the accelerometer
#include <SparkFunLSM6DSO.h>
LSM6DSO myIMU; //Default constructor is I2C, addr 0x6B

// LED pin
#define LED GPIO_NUM_26

// var for actually counting steps
int stepCount = 0;


// calculated during callibration phase, helps determine threshold
float restingAvg;
float sd;
float maxMag = 0;
float totalMag = 0;
float threshold;

bool stepped = false;
bool callibrating = false;


// vars for bluetooth connection
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

void callibrate_accelerometer();
float get_magnitude();
 
// handles deliverable A that'll process data received
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    // if incoming data starts with 0, turn LED off (only when program isn't callibrating accelerometer)
    if (value[0] == '0' && !callibrating) {
      Serial.println("Received 0. Turning LED off.");
      digitalWrite(LED, LOW);
    }
    // if incoming data starts with 1, turn LED on (only when program isn't callibrating accelerometer)
    else if (value[0] == '1' && !callibrating) {
      Serial.println("Received 1. Turning LED on.");
      digitalWrite(LED, HIGH);
    }
  }
};
 
void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);

  // Setup for accelerometer and getting data through I2C (taken from lib examples)
  Wire.begin();
  delay(10);
  if( myIMU.begin() )
    Serial.println("Ready.");
  else { 
    Serial.println("Could not connect to IMU.");
    Serial.println("Freezing");
  }

  if( myIMU.initialize(BASIC_SETTINGS) ) {
    Serial.println("Loaded Settings.");
  }

  // goes through process of determining threshold 
  callibrate_accelerometer();

  // setting up the bluetooth connection + handling notifications (mostly taken from Lab 4 Information)
  BLEDevice::init("CSGroup5"); // setups device name as CSGroup5
  pServer = BLEDevice::createServer();
  
  pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                                          CHARACTERISTIC_UUID,
                                          BLECharacteristic::PROPERTY_READ |
                                          BLECharacteristic::PROPERTY_WRITE |
                                          BLECharacteristic::PROPERTY_NOTIFY
                                        );
  
  pCharacteristic->setCallbacks(new MyCallbacks());
  
  // will allow client to "subscribe" to notifications and check flag values before notifs
  pCharacteristic->addDescriptor(new BLE2902()); 
  /*
  BLE2902 is a descriptor updating client characteristics configuration 
    - notification is fire and forget
    - indication is check if received
  */
  
  pService->start();
  
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}
 
void loop() {
  float mag = get_magnitude();

  // only consider it a step if the magnitude passes threshold and a step isn't in motion already
  if (mag > threshold && !stepped) {
    stepped = true;
    stepCount++;
    Serial.println("Step taken!");

    // send value to subscribed devices
    pCharacteristic->setValue(std::to_string(stepCount));
    pCharacteristic->notify();
  } 
  // ensure that step is only finished when magnitude falls below the threshold (prevents multiple step counts at once)
  else if (mag < threshold && stepped) {
    stepped = false;
  }

  delay(100);
}

void callibrate_accelerometer() {
  Serial.println("Callibrating: Stand still for 5 seconds...");
  callibrating = true;
  digitalWrite(LED, HIGH); // when LED turns on, phase A is signified (stand still to get resting value)

  // PHASE A: 500 iterations with 20ms delays = 5s of calibration still
  for (int i = 0; i < 500; i++) {
    float mag = get_magnitude();

    maxMag = max(maxMag, mag);
    totalMag += mag;
    delay(20);
  }

  // gives us an avg for the resting magnitude
  restingAvg = totalMag / 500;

  Serial.println("Callibrating: Take a few steps for 5 seconds...");
  digitalWrite(LED, LOW); // turns LED off to signify phase B (take steps)
  

  // PHASE B: 500 iterations with 20ms delays = 5s of calibration walking
  for (int i = 0; i < 500; i++) {
    float mag = get_magnitude();

    // rather than just taking the max, avg out between the two magnitudes to avoid having
    // one hard step resulting in a high threshold
    if (mag > maxMag) {
      maxMag = (mag + maxMag) / 2;
    }
    
    delay(20);
  }

  // rough std dev that assumes max is 3 standard deviations above mean
  sd = (maxMag - restingAvg) / 3; 
  
  // can tweak the multiplier to modify threshold
  // here it allows ~16% of the higher end of the bell curve 
  threshold = restingAvg + (0.9 * sd);
  

  Serial.printf("Callibration Done! Threshold: %.3f\n", threshold);
  callibrating = false;

  digitalWrite(LED, HIGH);
  delay(250);
  digitalWrite(LED, LOW);
  delay(250);
  digitalWrite(LED, HIGH);
  delay(250);
  digitalWrite(LED, LOW);
  delay(250);
}

// uses all 3 axes and combines to get a magnitude value moving in any direction
float get_magnitude() {
  float ax = myIMU.readFloatAccelX();
  float ay = myIMU.readFloatAccelY();
  float az = myIMU.readFloatAccelZ();

  return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}

