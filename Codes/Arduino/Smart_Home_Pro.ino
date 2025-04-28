// Libraries
#include <picobricks.h>     
#include <IRremote.h>       

// Pin Definitions
#define IR_PIN 0            // IR remote pin
#define RGB_PIN 6           // RGB LED control pin
#define LED_PIN 7           // Status LED pin
#define BUTTON_PIN 10       // Push button pin
#define BUZZER_PIN 20       // Buzzer pin
#define POT_PIN 26          // Potentiometer pin
#define LDR_PIN 27          // Light-dependent resistor pin
#define SOIL_PIN 28         // Soil moisture sensor pin

// Protocol Definition
#define DECODE_NEC          // NEC IR decoding protocol
#define RGB_COUNT 1         // Number of RGB LEDs connected

// OLED Screen Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

// Device Channels
#define soil 1
#define fan 2
#define window 2
#define door 4
#define speed 50            // Motor speed for soil & fan motors

// Global Variables
float temp;                // Temperature reading
float hum;                 // Humidity reading
int ldr;                   // Light sensor reading
int noteDuration;          // Duration of a tone
int button = 0;            // Button press flag
char correct_password[] = {1, 2, 3, 4}; // Preset 4-digit password
char password[] = {0, 0, 0, 0};         // Entered password
int oldDigit = 0;
int lock_state = 0;        // 0: locked, 1: unlocked
int buttonReleased = 1;
int passIndex = 0;
int digit = (analogRead(POT_PIN) * 9 / 1023); // Potentiometer reading mapped to 0-9
int digitCounter = 1;
unsigned long buttonPressTime = 0;
char str[10];              // String buffer for displaying text
int control = 0;           // Controls UI state
int user_soil = 1;         // User override for soil motor
int user_fan = 1;          // User override for fan motor
int soil_value = 0;
int soil_moisture = 0;

// Melody array for doorbell (frequency and note length)
unsigned long door_bell[][2] = {
  {262, 1}, // C4
  {330, 1}, // E4
  {392, 1}, // G4
  {523, 1}  // C5
};

// Servo angle settings for open/close
int windowOpen = 0;
int windowClose = 90;
int doorOpen = 0;
int doorClose = 90;

// Threshold values for environment control
int tempThreshold = 25;    // Temperature threshold (°C) to open the window
int ldrThreshold = 95;     // Light threshold (%) to activate lighting
int soilThreshold = 50;    // Soil moisture threshold

// Function & Object Declarations
SSD1306 OLED(SCREEN_ADDRESS, SCREEN_WIDTH, SCREEN_HEIGHT);  // OLED screen
NeoPixel strip (RGB_PIN, RGB_COUNT);  // RGB LED strip instance
SHTC3 shtc(0x70); // SHTC3 sensor
motorDriver motor;  // Motor driver

// Interrupt to detect button press and set flag
void buttonInterruptHandler() {
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH) {
    button = 1; // Set the flag to trigger doorbell routine
  }
}

// Function to play a note using the buzzer
void playNote(int frequency, int duration) {
  long period = 1000000L / frequency;               // Calculate the period of the wave
  long cycles = (long)frequency * duration / 1000;  // Number of wave cycles
  for (long i = 0; i < cycles; i++) {
    digitalWrite(BUZZER_PIN, HIGH);                 // Turn buzzer ON
    delayMicroseconds(period / 2);
    digitalWrite(BUZZER_PIN, LOW);                  // Turn buzzer OFF
    delayMicroseconds(period / 2);
  }
}

// Function to play the doorbell melody
void play_melody() {
  for (int thisNote = 0; thisNote < 4; thisNote++) {
    playNote(door_bell[thisNote][0], noteDuration);   // Play note frequency
    noteDuration = 110 * melody[thisNote][1];         // Set duration
    delay(noteDuration * 0.2);                        // Short pause between notes
  }
}

// Lock mechanism and display message
void lockTheSafe() {
  OLED.clear();
  OLED.setCursor(2, 2);
  OLED.print("Locking...");
  delay(300);
  motor.servo(door,doorClose);
  motor.servo(window,windowClose);
  digitalWrite(LED_PIN, LOW);
  oldDigit = 0;
  OLED.clear();
}

// Unlock mechanism and auto re-lock
void unlockTheSafe() {
  digitalWrite(LED_PIN, HIGH);
  OLED.clear();
  OLED.setCursor(2, 2);
  OLED.print("Opening...");
  delay(300);
  motor.servo(door, doorOpen); 
  motor.servo(window,windowOpen);
  delay(5000);
  OLED.clear();
  lockTheSafe();
}

// Check if entered password matches the predefined one
bool passwordCheck(char* definedPassword, char* enteredPassword) {
  for (int i = 0; i < 4 ; i++) {
    if (definedPassword[i] != enteredPassword[i])
      return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);

  // Configure pin modes
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  // Move servos to default (closed) positions
  motor.servo(door, doorClose);
  motor.servo(window, windowClose);

  // Set up interrupt for button press
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonInterruptHandler, CHANGE);
}

void loop() {
  // Read temperature and humidity
  temp = shtc.readTemperature();
  hum = shtc.readHumidity();

  if (control == 0){
    OLED.clear();
    OLED.setCursor(0, 10); OLED.print("Temp: ");
    OLED.setCursor(80, 10); sprintf(str, "%.2f", temp); OLED.print(str); OLED.print("C");

    OLED.setCursor(0, 20); OLED.print("HUMIDITY: ");
    OLED.setCursor(80, 20); sprintf(str, "%.2f", hum); OLED.print(str); OLED.print("%");
    OLED.show();
  }

  // Detect long button press
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && control == 0) {
    if (buttonPressTime == 0) buttonPressTime = millis();   // Record the time when button is first pressed
  } 
  else if (buttonState == HIGH && buttonPressTime != 0) {   // If button is released
    unsigned long pressDuration = millis() - buttonPressTime;
    if (pressDuration >= 1000) {    // 1 second hold
      play_melody();
      control = 1;
    }
    buttonPressTime = 0;    // Reset press time
  }

  // LDR light-based lighting
  ldr = (100 - (analogRead(LDR_PIN) * 100) / 1023);
  if (ldr > ldrThreshold){
    strip.setPixelColor(0, 255, 255, 255); // Turn ON RGB light to white
  } else {
    strip.setPixelColor(0, 0, 0, 0);       // Turn OFF RGB light
  }

  IrReceiver.decodedIRData.command = 0; // Reset IR command
  if (IrReceiver.decode()) {
    Serial.println(IrReceiver.decodedIRData.command);
    IrReceiver.resume();  // Ready to receive next IR signal
  } 
  if (IrReceiver.decodedIRData.command == number_1) {   // Open door
    motor.servo(door,doorOpen);
  }
  if (IrReceiver.decodedIRData.command == number_2) {   // Close door
    motor.servo(door,doorClose);
  }
  if (IrReceiver.decodedIRData.command == number_3) {   // Activate soil moisture motor
    motor.dc(soil,speed,1);
    user_soil = 1;
  }
  if (IrReceiver.decodedIRData.command == number_4) {   // Deactivate soil moisture motor
    motor.dc(fan,0,1);
    user_fan = 1;
  }
  if (IrReceiver.decodedIRData.command == number_5) {   // Activate fan
    motor.dc(fan,speed,1);
    user_fan = 1;
  }
  if (IrReceiver.decodedIRData.command == number_6) {   // Deactivate fan
    motor.dc(fan,0,1);  
    user_fan = 0;
  }
  if (IrReceiver.decodedIRData.command == number_7) {   // Open window
      motor.servo(window,windowOpen);
  }
  if (IrReceiver.decodedIRData.command == number_8) {   // Close window
      motor.servo(window,windowClose);
  }
  if (IrReceiver.decodedIRData.command == number_9) {   //Play melody
      play_melody();
  }

  if ((temp >= tempThreshold) && (user_fan == 0)){  //Turn on fan
    motor.dc(fan,speed,1);
  } 
  else {   // Turn off fan
    motor.dc(fan,speed,1);   
  }

  // Read soil moisture level 
  soil_value = analogRead(SOIL_PIN);
  soil_moisture = (soil_value / 1023) * 100;
  
  if ((soil_moisture <= soilThreshold) && (user_soil == 0))
      motor.dc(soil,speed,1);
  else if ((soil_moisture > soilThreshold) && (user_soil == 0))
      motor.dc(soil,speed,0);

  // Password mode
  if (control == 1){
    if (lock_state) {
      lockTheSafe();
      lock_state = 0;
    } else {
      int digit = (analogRead(POT_PIN) * 9 / 1023);
      if (digit != oldDigit) {
        oldDigit = digit;
        OLED.setCursor(2, 4); OLED.print("Password");
        OLED.setCursor(4, 7); OLED.print(str);
      }

      if (digitalRead(BUTTON_PIN) == 0 && (buttonReleased == 0)) buttonReleased = 1;
      if (digitalRead(BUTTON_PIN) == 1 && (buttonReleased == 1)) {
        buttonReleased = 0;
        digitalWrite(LED_PIN, HIGH);
        delay(300);
        password[passIndex] = digit;
        digitCounter++;
        oldDigit = 0;

        if (passIndex >= 3) {
          passIndex = 0;
          digitCounter = 1;
          if (passwordCheck(&correct_password[0], &password[0])) {
            unlockTheSafe();
            lock_state = 1;
          } else {
            OLED.clear();
            OLED.setCursor(2, 2); OLED.print("Try Again");
            delay(1500);
            OLED.clear();
          }
        } else {
          passIndex++;
        }
      }
    }
  }
}
