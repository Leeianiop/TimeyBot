#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_DC    16
#define OLED_CS    5
#define OLED_RESET 17

#define BUTTON_PIN 32

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &SPI,
  OLED_DC,
  OLED_RESET,
  OLED_CS
);

// STATES
enum Screen {
  STARTUP,
  MENU,
  STOPWATCH,
  TIMER,
  ALARM
};

Screen currentScreen = STARTUP;
int menuIndex = 0;

// BUTTON
bool lastButtonState = HIGH;
bool waitingForDoubleClick = false;
unsigned long lastClickTime = 0;

// STOPWATCH
bool stopwatchRunning = false;
unsigned long stopwatchStart = 0;
unsigned long stopwatchElapsed = 0;

// TIMER
bool timerRunning = false;
unsigned long timerStartTime = 0;
const unsigned long timerDuration = 60000;

// -------------------- DRAW FUNCTIONS --------------------

void drawEyes() {
  display.clearDisplay();

  display.fillCircle(40, 32, 12, SSD1306_WHITE);
  display.fillCircle(88, 32, 12, SSD1306_WHITE);

  display.fillCircle(40, 32, 5, SSD1306_BLACK);
  display.fillCircle(88, 32, 5, SSD1306_BLACK);

  display.display();
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  const char* items[3] = {
    "Stopwatch",
    "Timer",
    "Alarm"
  };

  for (int i = 0; i < 3; i++) {
    if (i == menuIndex) display.print("> ");
    else display.print("  ");
    display.println(items[i]);
  }

  display.display();
}

void drawStopwatch() {
  if (stopwatchRunning) {
    stopwatchElapsed = millis() - stopwatchStart;
  }

  unsigned long sec = stopwatchElapsed / 1000;
  unsigned long min = sec / 60;
  sec %= 60;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.printf("%02lu:%02lu", min, sec);

  display.setTextSize(1);
  display.setCursor(10, 50);
  display.println(stopwatchRunning ? "RUNNING" : "STOPPED");

  display.display();
}

void drawTimer() {
  long remain;

  if (timerRunning) {
    remain = timerDuration - (millis() - timerStartTime);

    if (remain <= 0) {
      remain = 0;
      timerRunning = false;
    }
  } else {
    remain = timerDuration;
  }

  unsigned long sec = remain / 1000;
  unsigned long min = sec / 60;
  sec %= 60;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.printf("%02lu:%02lu", min, sec);

  display.setTextSize(1);
  display.setCursor(10, 50);
  display.println(timerRunning ? "RUNNING" : "READY");

  display.display();
}

void drawAlarm() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println("ALARM");

  display.setTextSize(1);
  display.setCursor(10, 50);
  display.println("RTC NOT READY");

  display.display();
}

// -------------------- BUTTON LOGIC --------------------

void singleClick() {

  if (currentScreen == MENU) {
    menuIndex = (menuIndex + 1) % 3;
  }

  else if (currentScreen == STOPWATCH) {
    if (!stopwatchRunning) {
      stopwatchRunning = true;
      stopwatchStart = millis() - stopwatchElapsed;
    } else {
      stopwatchRunning = false;
    }
  }

  else if (currentScreen == TIMER) {
    if (!timerRunning) {
      timerRunning = true;
      timerStartTime = millis();
    } else {
      timerRunning = false;
    }
  }
}

void doubleClick() {

  if (currentScreen == MENU) {

    if (menuIndex == 0) currentScreen = STOPWATCH;
    else if (menuIndex == 1) currentScreen = TIMER;
    else currentScreen = ALARM;

  } else {
    currentScreen = MENU;
  }
}

void handleButton() {

  bool button = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && button == LOW) {

    unsigned long now = millis();

    if (waitingForDoubleClick && now - lastClickTime < 350) {
      waitingForDoubleClick = false;
      doubleClick();
    } else {
      waitingForDoubleClick = true;
      lastClickTime = now;
    }
  }

  lastButtonState = button;

  if (waitingForDoubleClick &&
      millis() - lastClickTime > 350) {

    waitingForDoubleClick = false;
    singleClick();
  }
}

// -------------------- SETUP / LOOP --------------------

void setup() {

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    while (1);
  }

  drawEyes();
  delay(1500);

  currentScreen = MENU;
}

void loop() {

  handleButton();

  switch (currentScreen) {

    case MENU:
      drawMenu();
      break;

    case STOPWATCH:
      drawStopwatch();
      break;

    case TIMER:
      drawTimer();
      break;

    case ALARM:
      drawAlarm();
      break;
  }

  delay(20);
}