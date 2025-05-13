#include <Arduino.h>
#include <BLEDevice.h>
#include <BLE2902.h> // assists in automatic updates rather than having phone confirm sync for each change

// for using the accelerometer
#include <SparkFunLSM6DSO.h>
LSM6DSO myIMU; //Default constructor is I2C, addr 0x6B

// var for actually counting steps/jumps
int stepCount = 0;
int jumpCount = 0;

bool stepping = false;
bool jumping = false;

// calculated during callibration phase, helps determine threshold
float restingAvg;
float sd;
float maxMag = 0;
float totalMag = 0;
float stepThreshold;
float jumpThreshold;

// float grav_x = 0.0;
// float grav_y = 0.0;
// float grav_z = 0.0;
bool calibrated = false;

// vars for bluetooth connection
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

// void calibrateGravityDirection();
// float getVerticalAcceleration();

void callibrate_accelerometer();
float get_magnitude();
 
// handles deliverable A that'll process data received
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
  }
};
 
void setup() {
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
  // calibrateGravityDirection();

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

  Serial.println("Begin testing");
}
 
void loop() {
  float mag = get_magnitude();

  if (mag > jumpThreshold && !jumping) {
    jumping = true;
    jumpCount++;
    Serial.println("Jumped!");

    delay(500);
  }
  // only consider it a step if the magnitude passes threshold and a step isn't in motion already
  else if (mag > stepThreshold && !stepping) {
    stepping = true;
    stepCount++;
    Serial.println("Step taken!");

    // send value to subscribed devices
    pCharacteristic->setValue("Steps: " + std::to_string(stepCount) + " Jumps: " + std::to_string(jumpCount));
    pCharacteristic->notify();
  } 
  else if (mag < jumpThreshold && jumping) {
    jumping = false;
  }
  // ensure that step is only finished when magnitude falls below the threshold (prevents multiple step counts at once)
  else if (mag < stepThreshold && stepping) {
    stepping = false;
  }

  delay(100);
}

void callibrate_accelerometer() {
  Serial.println("Callibrating: Stand still for 10 seconds...");
  calibrated = false;

  // PHASE A: 500 iterations with 20ms delays = 10s of calibration still
  for (int i = 0; i < 500; i++) {
    float mag = get_magnitude();

    maxMag = max(maxMag, mag);
    totalMag += mag;
    delay(20);
  }

  // gives us an avg for the resting magnitude
  restingAvg = totalMag / 500;

  Serial.println("Callibrating: Take a few steps for 10 seconds...");

  // PHASE B: 500 iterations with 20ms delays = 10s of calibration walking
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
  // here it allows the higher end of the bell curve 
  stepThreshold = restingAvg + (0.7 * sd);

  Serial.println("Callibrating: Jump 3-5 times in the next 10 seconds...");
  
  // PHASE C: Jump calibration
  float maxMag = 0;
  for (int i = 0; i < 500; i++) {
    float mag = get_magnitude();
    if (mag > maxMag) {
      maxMag = (mag + maxMag) / 2;
    }
    delay(20);
  }

  // relies on counting jump on the heavy magnitude impact when landing
  sd = (maxMag - restingAvg) / 3; 
  jumpThreshold = restingAvg + (0.8 * sd);
  
  Serial.printf("Callibration Done!\n");
  Serial.printf("Resting Avg: %.3f\n", restingAvg);
  Serial.printf("Step Threshold: %.3f\n", stepThreshold);
  Serial.printf("Jump Threshold: %.3f\n", jumpThreshold);
  calibrated = true;

  delay(5000);
}

// uses all 3 axes and combines to get a magnitude value moving in any direction
float get_magnitude() {
  float ax = myIMU.readFloatAccelX();
  float ay = myIMU.readFloatAccelY();
  float az = myIMU.readFloatAccelZ();

  return sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
}



// void calibrateGravityDirection() {
//   float sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;

//   Serial.println("Callibrating: Stand still for 3 seconds...");
  
//   for (int i = 0; i < 300; i++) {
//     sum_x += myIMU.readFloatAccelX();
//     sum_y += myIMU.readFloatAccelY();
//     sum_z += myIMU.readFloatAccelZ();

//     delay(10);
//   }
  
//   // Average the samples
//   grav_x = sum_x / 300;
//   grav_y = sum_y / 300;
//   grav_z = sum_z / 300;
  
//   // Normalize the gravity vector
//   float gravity_magnitude = sqrt(pow(grav_x, 2) + pow(grav_y, 2) + pow(grav_z, 2));
  
//   grav_x /= gravity_magnitude;
//   grav_y /= gravity_magnitude;
//   grav_z /= gravity_magnitude;
  
//   calibrated = true;
// }

// float getVerticalAcceleration() {
//   float ax = myIMU.readFloatAccelX();
//   float ay = myIMU.readFloatAccelY();
//   float az = myIMU.readFloatAccelZ();

//   // takes acceration data and isolates vertical accel using gravity
//   float vertical_component = ax * grav_x + ay * grav_y + az * grav_z;
  
//   return vertical_component;
// }