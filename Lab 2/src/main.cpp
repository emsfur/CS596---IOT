#include <Arduino.h>

// LED light pins
#define RED_LED GPIO_NUM_25
#define YELLOW_LED GPIO_NUM_26
#define GREEN_LED GPIO_NUM_27

// passive buzzer light pin (any reg GPIO works)
#define BUZZER GPIO_NUM_2

// used datasheet to see that this is a touch pin
#define TOUCH GPIO_NUM_32
touch_value_t THRESHOLD = 70; // value chosen based on testing Serial.println(touchRead(TOUCH))

// used for tracking when touch is pressed
bool pressed = false;

// init functions
void touchPressed();
void greenToRed();
void redToGreen();

void setup() {
  // set up all LED pins as output
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // initialize lights in red (10 seconds) then go from red-yellow to green 
  redToGreen();

  // setup touch as interrupt so that we can still track presses when main loop has delays that 
  touchAttachInterrupt(TOUCH, touchPressed, THRESHOLD);
  Serial.begin(115200);
}

void loop() {
  // used for testing to see that touching wire would cause val to drop from 80 to ~65 or less based on where you hold it
  // Serial.println(touchRead(TOUCH));

  // if the pressed flag has been set
  if (pressed) {
    // go through the phases
    greenToRed();
    redToGreen();

    // added here to ensure any presses during changing phases wont cause it to do another cycle right away
    pressed = false;
  } 
  // if not pressed, play buzzer for green light
  else {
    // players 450 Hz tone for 500 ms
    tone(BUZZER, 450);
    delay(500);
    // off for 1500ms
    noTone(BUZZER); 
    delay(1500);
  }
}

void touchPressed() {
  // Set the pressed flag to true when the touch interrupt occurs
  pressed = true;
}

void greenToRed() {
  // yellow light for 2 seconds
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  delay(2000);

  // red light
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
}

void redToGreen() {
  // red light for 10 seconds
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  // delay(10000) is replaced with buzzer w/ delay since it's 500ms per iteration, it takes 20 iterations to get 10 seconds
  for (int i = 0; i < 20; i++) {
    // players 450 Hz tone for 250 ms
    tone(BUZZER, 450); 
    delay(250);
    // off for 250ms
    noTone(BUZZER); 
    delay(250);
  }

  // red/yellow light for 2 seconds
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  delay(2000);

  // green light for 6 seconds
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  // delay(6000) is replaced with buzzer that takes 2s per iteration meaning 3 iterations for 6 seconds
  for (int i = 0; i < 3; i++) {
    // players 450 Hz tone for 500 ms
    tone(BUZZER, 450);
    delay(500);
    // off for 1500ms
    noTone(BUZZER); 
    delay(1500);
  }
}

