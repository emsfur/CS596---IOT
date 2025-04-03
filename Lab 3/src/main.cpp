// Dealing with internet connection
#include <secrets.h>
#include <WiFi.h>
#include <HttpClient.h>

// Dealing with DHT20 and gathering info using SPI
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Arduino.h>

// declare dht20 (temperature/humidity) sensor
Adafruit_AHTX0 dht;

void setup(){
  Serial.begin(115200);

  // connect dht20 sensor
  if (dht.begin()) {
    Serial.println("Found DHT20");
  } else {
    Serial.println("Didn't find DHT20");
  }  

  // We start by connecting to a WiFi network
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // connect to wifi (home wifi rn)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  // confirms that esp has internet connection
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());
}


// Name of the server we want to connect to (taken from code definition for http get)
// server IP is 128.85.32.135
IPAddress serverAddr = IPAddress(128,85,32,135);
uint16_t serverPort = 8080;
// Path to download (this is the bit after the hostname in the URL
// that you want to download
String url;

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

void loop() {


  // populate temp and humidity objects with fresh data
  sensors_event_t humidity, temp;
  dht.getEvent(&humidity, &temp);

  url = "/?temperature=" + String(temp.temperature) + "&humidity=" + String(humidity.relative_humidity);


  int err =0;
  
  WiFiClient c;
  HttpClient http(c);

  err = http.get(serverAddr, "Azure Server", serverPort, url.c_str());
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

  delay(5000);
}