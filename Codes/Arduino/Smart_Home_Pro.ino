#include <picobricks.h>

// ---- Pins ----
#define IR_PIN       0
#define RGB_PIN      6     
#define LED_PIN      7
#define BUTTON_PIN   10
#define BUZZER_PIN   20
#define POT_PIN      26   
#define LDR_PIN      27     
#define SOIL_PIN     28    

// ---- OLED ----
#define SCREEN_ADDR   0x3C
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

//---SOIL---
#define DRY_RAW  810   // dry
#define WET_RAW  670   // wet

int ir_data = 0;
volatile bool data_rcvd = false;

int correct_password[4] = {1,2,3,4};
int password[4]         = {0,0,0,0};

int  lock_state       = 0;
int  buttonReleased   = 1;    
int  passIndex        = 0;
int  digitCounter     = 1;
int  user_soil        = 0;     // 0=auto, 1=manual
int  user_fan         = 0;     // 0=auto, 1=manual
int  oldDigit         = 0;
int  control          = 0;     // 0: dashboard (Temp/Hum), 1: password UI
unsigned long button_press_time = 0;

int doorClosed   = 80;
int doorOpen     = 0;
int windowClosed = 50;
int windowOpen   = 0;

int   tempThreshold = 26;
int   ldrThreshold  = 500;
int   soilThreshold = 50;

// Melody (Hz) and duration
int melody[] = {262, 330, 392, 392, 330, 262};
float duration_s = 0.3f;

#define RGB_COUNT 1         // Number of RGB LEDs connected

// ---- Objects ----
SSD1306 OLED(SCREEN_ADDR, SCREEN_WIDTH, SCREEN_HEIGHT);
SHTC3   shtc(0x70);
NeoPixel strip (RGB_PIN, RGB_COUNT);  // RGB LED strip instance
motorDriver motor;
IRPico  ir(IR_PIN);

// ---- IR ISR ----
volatile int  irCode = 0;
volatile bool irReceived = false;

void irInterruptHandler() {
  if (ir.decode()) {
    irCode = ir.getCode();
    irReceived = true;
  }
}

// ---- Helpers ----
bool passwordCheck(const int* definedPassword, const int* enteredPassword) {
  for (int i=0; i<4; i++) if (definedPassword[i] != enteredPassword[i]) return false;
  return true;
}

void playNote(int freq, int ms) {
  if (freq <= 0 || ms <= 0) return;
  long period = 1000000L / freq;
  long cycles = (long)freq * ms / 1000;
  for (long i=0; i<cycles; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(period/2);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(period/2);
  }
}

void play_melody() {
  int ms = (int)(duration_s * 1000.0f);
  for (unsigned i=0; i<sizeof(melody)/sizeof(melody[0]); i++) {
    playNote(melody[i], ms);
    delay(50);
  }
}

void lockTheSafe() {
  OLED.clear();
  OLED.setCursor(30, 20);
  OLED.print("Locking...");
  OLED.show();
  delay(300);
  motor.servo(4, doorClosed);
  motor.servo(2, windowClosed);
  digitalWrite(LED_PIN, LOW);
}

void unlockTheSafe() {
  digitalWrite(LED_PIN, HIGH);
  OLED.clear();
  OLED.setCursor(30, 20);
  OLED.print("Unlocked!");
  OLED.show();
  delay(300);
  motor.servo(4, doorOpen);
  motor.servo(2, windowOpen);
  delay(5000);
  lockTheSafe(); // relock after 5s
}

void drawPasswordUI(int digit) {
  OLED.clear();
  OLED.setCursor(30, 10); OLED.print("Password");

  // four slots (use underscores)
  OLED.setCursor(25, 40); OLED.print("_");
  OLED.setCursor(50, 40); OLED.print("_");
  OLED.setCursor(75, 40); OLED.print("_");
  OLED.setCursor(100,40); OLED.print("_");

  // typed digits
  for (int c=0; c<digitCounter-1; c++) {
    OLED.setCursor(25*(c+1), 30);
    char s[2]; s[0] = '0' + password[c]; s[1]=0;
    OLED.print(s);
  }

  // current preview
  OLED.setCursor(25*digitCounter, 30);
  char s2[2]; s2[0] = '0' + digit; s2[1]=0;
  OLED.print(s2);

  OLED.show();
}

int soilPercent(int raw) {
  float pct = (float)(DRY_RAW - raw) * 100.0f / (float)(DRY_RAW - WET_RAW);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (int)(pct + 0.5f);  // Rounding
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  analogReadResolution(10); 
  Wire.begin();
  OLED.init();
  OLED.clear(); OLED.show();

  shtc.begin();
  attachInterrupt(digitalPinToInterrupt(IR_PIN), irInterruptHandler, FALLING);

  // Close window and door initially
  motor.servo(4, doorClosed);
  motor.servo(2, windowClosed);
}

// ================== LOOP ==================
void loop() {
  // Read SHTC3
  float temp = shtc.readTemperature();
  float hum  = shtc.readHumidity();

  // ===== Dashboard (control==0): show Temp/Hum =====
  if (control == 0) {
    OLED.clear();
    OLED.setCursor(10, 10);
    char buf1[20]; snprintf(buf1, sizeof(buf1), "Temp: %.1fC", temp);
    OLED.print(buf1);
    OLED.setCursor(10, 30);
    char buf2[20]; snprintf(buf2, sizeof(buf2), "Humidity: %.1f%%", hum);
    OLED.print(buf2);
    OLED.show();
  }

  // ===== Long-press to enter password mode =====
  static int prevBtn = HIGH;
  int nowBtn = digitalRead(BUTTON_PIN);

  if (nowBtn == LOW && control == 0) {
    if (button_press_time == 0) button_press_time = millis();
  } else if (nowBtn == HIGH) {
    if (button_press_time != 0) {
      unsigned long press_duration = millis() - button_press_time;
      if (press_duration >= 1000) {
        play_melody();
        control = 1; // enter password UI
      }
      button_press_time = 0;
    }
  }
  prevBtn = nowBtn;

  // LDR light-based lighting
  int ldr = analogRead(LDR_PIN);
  Serial.println(ldr);
  if (ldr < ldrThreshold){
    strip.Fill(0, 0, 0); // Turn OFF RGB light to white
  } else {
    strip.Fill(255, 255, 255);       // Turn ON RGB light
  }
  // ===== IR handling =====
  if (irReceived) {
    int code = irCode;
    irReceived = false;

    if (code == number_1) {            // Open door
      motor.servo(4, doorOpen);
    } else if (code == number_2) {     // Close door
      motor.servo(4, doorClosed);
    } else if (code == number_3) {     // Soil ON (manual)
      motor.dc(1, 50, 1);
      user_soil = 1;
    } else if (code == number_4) {     // Soil OFF (auto resumes)
      motor.dc(1, 0, 1);
      user_soil = 0;
    } else if (code == number_5) {     // Fan ON (manual)
      motor.dc(2, 50, 1);
      user_fan = 1;
    } else if (code == number_6) {     // Fan OFF (auto resumes)
      motor.dc(2, 0, 1);
      user_fan = 0;
    } else if (code == number_7) {     // Window open
      motor.servo(2, windowOpen);
    } else if (code == number_8) {     // Window close
      motor.servo(2, windowClosed);
    } else if (code == number_9) {     // Play melody
      play_melody();
    }
  }

  // ===== Auto Fan by temperature (only if user_fan==0 -> auto) =====
  if (user_fan == 0) {
    if (temp >= tempThreshold) {
      motor.dc(2, 50, 1);
    } else {
      motor.dc(2, 0, 1);
    }
  }

  // ===== Soil moisture auto pump (only if user_soil==0 -> auto) =====
  int pct = constrain(map(analogRead(SOIL_PIN), 810, 670, 0, 100), 0, 100);
  if (user_soil == 1) {
    if (pct <= soilThreshold) motor.dc(1, 50, 1);
    else motor.dc(1, 0, 1);
  }

  // ===== Password mode (control==1) =====
  if (control == 1) {
    if (lock_state) {
      lockTheSafe();
      lock_state = 0;
    } else {
      int digit = constrain(map(analogRead(POT_PIN), 0, 1023, 0, 9), 0, 9);
      Serial.println(analogRead(POT_PIN));
      if (digit != oldDigit) {
        oldDigit = digit;
        drawPasswordUI(digit);
      }

      // latch logic: capture on button release edge
      static int prev = HIGH;
      int now = digitalRead(BUTTON_PIN);

      if (prev == LOW && now == HIGH) { // released
        password[passIndex] = digit;
        digitCounter += 1;
        oldDigit = 0;

        if (passIndex >= 3) {
          passIndex    = 0;
          digitCounter = 1;

          if (passwordCheck(correct_password, password)) {
            unlockTheSafe();
            lock_state = 1;
            control    = 0; // back to dashboard
          } else {
            OLED.clear();
            OLED.setCursor(30, 20);
            OLED.print("Try Again");
            OLED.show();
            control = 0;
          }
        } else {
          passIndex += 1;
        }
      }
      prev = now;
    }
  }

  delay(15);
}
