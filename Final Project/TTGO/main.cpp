// TTGO - handling data simulation (handling user inputs) and sending to devkitV1

#include <esp_now.h>
#include <WiFi.h>

// configures mac address (taken from devkitV1 - our receiver ESP32)
uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x79, 0x8B, 0x74};
// ec:e3:34:79:8b:74

// Structure to send data to receiver ESP32
// Must match the receiver structure
typedef struct send_struct {
  int engineLight = LOW;
  int tireLight = LOW;
  int oilLight = LOW;
  // int batteryLight = LOW;
  int speed = 0;
} send_struct;

// Create a struct for sending data over ESP-NOW
send_struct sendData;

// init peer to later get status info from receiver ESP32
esp_now_peer_info_t peerInfo;

// callback when data is sent (delivery confirmation)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

unsigned long lastDataSent = 0; // time stamp for last data send event
int sendThreshold = 5000; // send data to receiver ESP32 every 5 seconds

// struct for handling all data related to dashboard lights
typedef struct dash_light {
  String description;       // data later sent to cloud
  gpio_num_t button_pin;    // pin config for button
  gpio_num_t LED_pin;       // pin config for LED
  int lightState = LOW;     // helps for toggling ON/OFF state
  int prevBtnState = HIGH;  // last 3 help confirm button presses with debouncing
  int currBtnState;
  unsigned long lastUpdate = 0;
} dash_light;

dash_light engine;  // yellow button/LED
dash_light tire;    // blue button/LED
dash_light oil;     // red button/LED
// dash_light battery; // green button/LED

bool buttonPressed(dash_light &light);
int pressThreshold = 1000; // 1 second cooldown before allowing another button press

#define speedPin GPIO_NUM_36 // GPIO 4 is an ADC pin to read analog data

void setup() {
  Serial.begin(115200);

  // ******** SETTING UP INFO/PIN CONFIG ********
  engine.description = "engine_light";
  engine.button_pin = GPIO_NUM_32;
  engine.LED_pin = GPIO_NUM_22;

  tire.description = "tire_light";
  tire.button_pin = GPIO_NUM_12;
  tire.LED_pin = GPIO_NUM_2;

  oil.description = "oil_light";
  oil.button_pin = GPIO_NUM_13;
  oil.LED_pin = GPIO_NUM_26; 

  // battery.description = "battery_light";
  // battery.button_pin = GPIO_NUM_23;
  // battery.LED_pin = GPIO_NUM_15;

  // sets all buttons as inputs (HIGH = UNPRESSED / LOW = PRESSED)
  pinMode(engine.button_pin, INPUT_PULLUP);
  pinMode(tire.button_pin, INPUT_PULLUP);
  pinMode(oil.button_pin, INPUT_PULLUP);
  // pinMode(battery.button_pin, INPUT_PULLUP);

  // sets all LEDs as output
  pinMode(engine.LED_pin, OUTPUT);
  pinMode(tire.LED_pin, OUTPUT);
  pinMode(oil.LED_pin, OUTPUT);
  // pinMode(battery.LED_pin, OUTPUT);
  // ******** FINISHED SETTING UP INFO/PIN CONFIG ********

  // ensures all LEDs are off when system starts
  digitalWrite(engine.LED_pin, LOW);
  digitalWrite(tire.LED_pin, LOW);
  digitalWrite(oil.LED_pin, LOW);
  // digitalWrite(battery.LED_pin, LOW);

  // ******** SETTING UP ESP-NOW - Taken from Arduino Docs: https://docs.arduino.cc/tutorials/nano-esp32/esp-now ********
  // Set device as a Wi-Fi Station (used for getting channel for ESP-NOW)
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESP-NOW is successfully init, setup to get the status of transmitted packets
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  // ******** FINISHED SETTING UP ESP-NOW ********
}


void loop() {
  // if button correlating to engine dash light is pressed
  if (buttonPressed(engine)) {
    // toggle dash light state and update LED for easy visualization
    engine.lightState = !engine.lightState;
    digitalWrite(engine.LED_pin, engine.lightState);

    // include change in data being sent to receiver ESP32
    sendData.engineLight = engine.lightState;
  }

  if (buttonPressed(tire)) {
    tire.lightState = !tire.lightState;
    digitalWrite(tire.LED_pin, tire.lightState);

    sendData.tireLight = tire.lightState;
  }

  if (buttonPressed(oil)) {
    oil.lightState = !oil.lightState;
    digitalWrite(oil.LED_pin, oil.lightState);


    sendData.oilLight = oil.lightState;
  }

  // speed data will consistently be sent to receiver ESP32
  // Serial.println((int) (analogRead(speedPin) / 4095.0 * 100));
  sendData.speed = (int) (analogRead(speedPin) / 4095.0 * 100); // bound potentiometer to 0-100 mph

  // send data on a set interval: current threshold set as 5 seconds
  if (millis() - lastDataSent >= sendThreshold) {
    lastDataSent = millis();
    esp_now_send(broadcastAddress, (uint8_t *) &sendData, sizeof(sendData));
  }

}

// handles checking if button has been pressed and updating correlating data
bool buttonPressed(dash_light &light) {
  light.currBtnState = digitalRead(light.button_pin);

  // first two conditions ensure that button goes from unpressed state to pressed state
  // last condition only allows press if it occurs after threshold: currently 1 second (handles debouncing)
  if (light.currBtnState == LOW && light.prevBtnState == HIGH && (millis() - light.lastUpdate) >= pressThreshold) {
    // update button data and return pressed = true
    light.prevBtnState = LOW;
    light.lastUpdate = millis();
    return true;
  } else { 
    // push data and return pressed = false 
    light.prevBtnState = light.currBtnState;
    return false;
  }
}

