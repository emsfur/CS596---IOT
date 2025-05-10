// devkitV1 - receives simulated data from TTGO and sends to cloud

// libs to setup devkitV1 as ESP-NOW receiver
#include <esp_now.h>
#include <WiFi.h>

// used for conneting to wifi and sending HTTP requests to cloud
#include <HttpClient.h>
#include <secrets.h> // contains WiFi SSID and PASS

// connecting to cloud server
IPAddress serverAddr = IPAddress(128,85,32,135); // server IP is 128.85.32.135
uint16_t serverPort = 8080;

// segment of URL following the domain (http://128.85.32.135:8080 in this case)
String parameters = "";

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

// Structure example to receive data
// Must match the sender structure
typedef struct receive_struct {
  int engineLight = LOW;
  int tireLight = LOW;
  int oilLight = LOW;
  // int batteryLight = LOW;
  int speed = 0;
} receive_struct;

// Create a struct_message for handling data received from sender ESP32
receive_struct receivedData;

String prev_engine_light = "OFF";
String engine_light = "OFF";

String prev_tire_light = "OFF";
String tire_light = "OFF";

String prev_oil_light = "OFF";
String oil_light = "OFF";


// Setting up state management to switch between ESP-NOW and WiFi Station (module can only work with one at a time)
// This works in this case since we only send data at 5 second intervals, allowing enough time to avoid data loss
enum State {
  IDLE,             // waits to receive data over ESP-NOW
  CONNECT_WIFI,     // establishes wifi connection
  SEND_HTTP,        // sends data to cloud
  DISCONNECT_WIFI,  // stops WiFi
  START_ESPNOW      // begins to listen on ESP-NOW
};

volatile bool dataReceived = false;
State currentState = IDLE;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  engine_light = (receivedData.engineLight == LOW) ? "OFF" : "ON";
  tire_light = (receivedData.tireLight == LOW) ? "OFF" : "ON";
  oil_light = (receivedData.oilLight == LOW) ? "OFF" : "ON";
  // battery_light = (receivedData.batteryLight == LOW) ? "OFF" : "ON";

  parameters = "/?"; 

  // only send to cloud if value has changed since lights don't change often
  if (engine_light != prev_engine_light) {
    parameters += "engine_light=" + engine_light + "&";
  }
  if (tire_light != prev_tire_light) {
    parameters += "tire_light=" + tire_light + "&";
  }
  if (oil_light != prev_oil_light) {
    parameters += "oil_light=" + oil_light + "&";
  }
  // if (battery_light != prev_battery_light) {
  //   parameters += "battery_light=" + battery_light + "&";
  // }

  prev_engine_light = engine_light;
  prev_tire_light = tire_light;
  prev_oil_light = oil_light;
  // prev_battery_light = battery_light;

  // speed value will constantly be sent to cloud
  parameters += "speed=" + String(receivedData.speed);
  dataReceived = true;
  Serial.println("Received");
} 


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // WiFi.mode(WIFI_AP_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  switch (currentState) {
    // simply waits until it receives data from TTGO
    case IDLE:
      if (dataReceived) {
        // switch to wifi mode to send the received data
        dataReceived = false;
        currentState = CONNECT_WIFI;
        Serial.println("Data received, preparing to connect Wi-Fi...");
        esp_now_deinit(); // stop ESP-NOW to allow WiFi mode
        delay(10);
      }
      break;
    
    // reestablishes a wifi conenction based on (currently) hardcoded credentials
    case CONNECT_WIFI:
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      Serial.print("Connecting to Wi-Fi");
      

      while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWi-Fi Connected!");
        // once connection established, prepare to send the data to cloud
        currentState = SEND_HTTP;
      }
      break;

    // makes a request to cloud server (flask) to send data using HTTP
    case SEND_HTTP:
      {
        // HTTP code taken from Lab 3
        int err=0;
        
        WiFiClient c;
        HttpClient http(c);

        err = http.get(serverAddr, "Azure Server", serverPort, parameters.c_str());
        if (err == 0)
        {
          Serial.println("\nstartedRequest ok");

          err = http.responseStatusCode();
          if (err >= 0)
          {
            Serial.println("\nResponse status: " + String(err));

            err = http.skipResponseHeaders();
            if (err >= 0)
            {
              int bodyLen = http.contentLength();
              Serial.println("HTTP Response Body:");
            
              // Now we've got to the body, so we can print it out
              unsigned long timeoutStart = millis();
              char c;
              // Whilst we haven't timed out & haven't reached the end of the body
              while ( (http.connected() || http.available()) && ((millis() - timeoutStart) < kNetworkTimeout) )
              {
                  if (http.available())
                  {
                      c = http.read();
                      // Print out this character
                      Serial.print(c);
                    
                      bodyLen--;
                      // We read something, reset the timeout counter
                      timeoutStart = millis();
                  }
                  else
                  {
                      // We haven't got any data, so let's pause to allow some to arrive
                      delay(kNetworkDelay);
                  }
              }

              Serial.println();
            }
            else
            {
              Serial.println("Failed to skip response headers: " + err);
            }
          }
          else
          {    
            Serial.println("Getting response failed: " + err);
          }
        }
        else
        {
          Serial.println("Connect failed: " + err);
        }
        http.stop();
        currentState = DISCONNECT_WIFI; // once data has been sent, stop the wifi to begin listening over ESP-NOW again
      }
      break;

    // simply stops WiFi
    case DISCONNECT_WIFI:
      WiFi.disconnect(true);
      Serial.println("Wi-Fi disconnected");
      delay(100);
      // reenables ESP-NOW for next data sent by TTGO
      currentState = START_ESPNOW;
      break;

    // starts ESP-NOW mode 
    case START_ESPNOW:
      WiFi.mode(WIFI_STA);
      delay(50);
      if (esp_now_init() != ESP_OK) {
        Serial.println("Failed to restart ESP-NOW!");
      }

      // function needs to be assigned every time ESP-NOW is initialized
      esp_now_register_recv_cb(OnDataRecv);
      currentState = IDLE; // swaps to stay idle (listening on ESP-NOW)
      break;
  }
}