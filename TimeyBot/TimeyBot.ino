#define AUDIO_NO_WEB    
#define AUDIO_NO_WAV    
#define AUDIO_NO_FLAC
#define AUDIO_NO_AAC
#define AUDIO_NO_M4A

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Audio.h" 
#include "LittleFS.h"

// ---------------- OLED SPI Pins ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI 23
#define OLED_CLK  18
#define OLED_DC   16
#define OLED_CS   5
#define OLED_RESET 17

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// ---------------- Audio & Button Pins ----------------
#define BUTTON_PIN 4
#define I2S_BCLK   26
#define I2S_LRC    25
#define I2S_DOUT   27

Audio audio;

// ---------------- UI States ----------------
enum UIState {
  STATE_SET_HOURS,      
  STATE_SET_MINUTES,    
  STATE_CONFIRM_SETUP,  
  STATE_CLOCK,
  STATE_MENU,
  STATE_TIMER,          // Fully coded
  STATE_ALARM_HOURS,    // Fully coded dynamic settings
  STATE_ALARM_MINUTES,  
  STATE_ALARM_ACTIVE,   
  STATE_STOPWATCH,      // Fully coded
  STATE_SOUND_PLAY
};

UIState currentState = STATE_SET_HOURS; 
int menuSelection = 0; 
int scrollOffset = 0; 
const int MENU_ITEMS_COUNT = 5;
const char* menuItems[] = {
  "Timer Mode", 
  "Alarm Set", 
  "Stopwatch", 
  "Songs (Earrings)", 
  "< Back to Clock"
};

int confirmSelection = 0; 

// ---------------- Button Timing Variables ----------------
unsigned long lastButtonPressTime = 0;
bool singleClickPending = false;
unsigned long doubleClickGap = 250; 

// ---------------- Clock Variables ----------------
unsigned long lastClockUpdate = 0;
int hours = 0;
int minutes = 0;
int seconds = 0; 

// ---------------- Mode Functional Variables ----------------
// Timer variables
int timerTargetMinutes = 1; 
unsigned long timerStartTime = 0;
unsigned long timerDurationMs = 0;
bool timerRunning = false;

// Alarm variables
int alarmHours = 0;
int alarmMinutes = 0;
bool alarmEnabled = false;
bool alarmRinging = false;

// Stopwatch variables
unsigned long stopwatchElapsedMs = 0;
unsigned long stopwatchLastPausedMs = 0;
unsigned long stopwatchStartTime = 0;
bool stopwatchRunning = false;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("OLED allocation failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  if(!LittleFS.begin()){
     Serial.println("An Error has occurred while mounting LittleFS");
  }

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(12); 
}

void loop() {
  audio.loop();

  // --- Dynamic Double Click Adjustment ---
  if (currentState == STATE_SET_HOURS || currentState == STATE_SET_MINUTES || currentState == STATE_ALARM_HOURS || currentState == STATE_ALARM_MINUTES) {
    doubleClickGap = 180; 
  } else {
    doubleClickGap = 350; 
  }

  // --- Button Input Logic ---
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);
  bool actionScroll = false;
  bool actionEnter = false;

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long clickTime = millis();
    if (clickTime - lastButtonPressTime < doubleClickGap) {
      actionEnter = true;          
      singleClickPending = false;  
    } else {
      singleClickPending = true;   
    }
    lastButtonPressTime = clickTime;
    delay(60); 
  }
  lastButtonState = currentButtonState;

  if (singleClickPending && (millis() - lastButtonPressTime > doubleClickGap)) {
    actionScroll = true;
    singleClickPending = false;
  }

  // --- Background Clock Tracker ---
  if (currentState != STATE_SET_HOURS && currentState != STATE_SET_MINUTES && currentState != STATE_CONFIRM_SETUP) {
    if (millis() - lastClockUpdate >= 1000) {
      lastClockUpdate = millis();
      seconds++;
      if (seconds >= 60) {
        seconds = 0;
        minutes++;
        if (minutes >= 60) {
          minutes = 0;
          hours++;
          if (hours >= 24) hours = 0;
        }
      }
      
      // Check active alarm match once per minute change
      if (alarmEnabled && seconds == 0 && hours == alarmHours && minutes == alarmMinutes) {
        alarmRinging = true;
        currentState = STATE_SOUND_PLAY;
        audio.connecttoFS(LittleFS, "/Earrings.mp3");
      }
    }
  }

  // --- UI Logic State Machine ---
  switch (currentState) {
    case STATE_SET_HOURS:
      if (actionScroll) { hours++; if (hours >= 24) hours = 0; }
      if (actionEnter) { currentState = STATE_SET_MINUTES; delay(150); }
      break;

    case STATE_SET_MINUTES:
      if (actionScroll) { minutes++; if (minutes >= 60) minutes = 0; }
      if (actionEnter) { currentState = STATE_CONFIRM_SETUP; confirmSelection = 0; delay(150); }
      break;

    case STATE_CONFIRM_SETUP:
      if (actionScroll) confirmSelection = (confirmSelection == 0) ? 1 : 0;
      if (actionEnter) {
        if (confirmSelection == 0) { lastClockUpdate = millis(); seconds = 0; currentState = STATE_CLOCK; }
        else { currentState = STATE_SET_HOURS; }
        delay(150);
      }
      break;

    case STATE_CLOCK:
      if (actionScroll || actionEnter) { currentState = STATE_MENU; menuSelection = 0; scrollOffset = 0; }
      break;

    case STATE_MENU:
      if (actionScroll) {
        menuSelection++;
        if (menuSelection >= MENU_ITEMS_COUNT) { menuSelection = 0; scrollOffset = 0; }
        if (menuSelection - scrollOffset >= 2) scrollOffset++;
      }
      if (actionEnter) {
        if (menuSelection == 0)      { currentState = STATE_TIMER; timerRunning = false; }
        else if (menuSelection == 1) { currentState = STATE_ALARM_HOURS; alarmHours = 0; }
        else if (menuSelection == 2) { currentState = STATE_STOPWATCH; }
        else if (menuSelection == 3) {
          currentState = STATE_SOUND_PLAY;
          alarmRinging = false;
          audio.connecttoFS(LittleFS, "/Earrings.mp3");
        } 
        else if (menuSelection == 4) currentState = STATE_CLOCK;
        delay(150);
      }
      break;

    case STATE_TIMER:
      if (!timerRunning) {
        if (actionScroll) { timerTargetMinutes++; if (timerTargetMinutes > 60) timerTargetMinutes = 1; }
        if (actionEnter) { timerRunning = true; timerStartTime = millis(); timerDurationMs = (unsigned long)timerTargetMinutes * 60000; }
      } else {
        if (actionEnter) { timerRunning = false; } // Interrupted/Cancel
        if (millis() - timerStartTime >= timerDurationMs) {
          timerRunning = false;
          currentState = STATE_SOUND_PLAY;
          audio.connecttoFS(LittleFS, "/Earrings.mp3");
        }
      }
      if (actionScroll && timerRunning) { currentState = STATE_MENU; timerRunning = false; } // Exit if scrolled while running
      break;

    case STATE_ALARM_HOURS:
      if (actionScroll) { alarmHours++; if (alarmHours >= 24) alarmHours = 0; }
      if (actionEnter) { currentState = STATE_ALARM_MINUTES; alarmMinutes = 0; delay(150); }
      break;

    case STATE_ALARM_MINUTES:
      if (actionScroll) { alarmMinutes++; if (alarmMinutes >= 60) alarmMinutes = 0; }
      if (actionEnter) { alarmEnabled = true; currentState = STATE_ALARM_ACTIVE; delay(150); }
      break;

    case STATE_ALARM_ACTIVE:
      if (actionScroll) { alarmEnabled = false; currentState = STATE_MENU; } // Disable alarm
      if (actionEnter) { currentState = STATE_MENU; } // Keep active, exit menu view
      break;

    case STATE_STOPWATCH:
      if (actionScroll) { // Pause / Unpause toggle
        if (!stopwatchRunning) {
          stopwatchRunning = true;
          stopwatchStartTime = millis() - stopwatchElapsedMs;
        } else {
          stopwatchRunning = false;
          stopwatchElapsedMs = millis() - stopwatchStartTime;
        }
      }
      if (actionEnter) { // Reset or Back control
        if (stopwatchElapsedMs == 0 && !stopwatchRunning) {
          currentState = STATE_MENU;
        } else {
          stopwatchRunning = false;
          stopwatchElapsedMs = 0;
        }
      }
      break;

    case STATE_SOUND_PLAY:
      if (actionEnter || actionScroll) { 
        audio.stopSong(); 
        currentState = alarmRinging ? STATE_ALARM_ACTIVE : STATE_MENU;
        alarmRinging = false;
      }
      break;
  }

  // --- Render to Display ---
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); 

  if (currentState == STATE_SET_HOURS) {
    display.setTextSize(1); display.setCursor(16, 6); display.print("SETUP: SET HOUR");
    display.setTextSize(4); display.setCursor(40, 20);
    if(hours < 10) display.print("0"); display.print(hours);
    display.setTextSize(1); display.setCursor(6, 54); display.print("Clk: +1 | Dbl: Next");

  } else if (currentState == STATE_SET_MINUTES) {
    display.setTextSize(1); display.setCursor(10, 6); display.print("SETUP: SET MINUTE");
    display.setTextSize(4); display.setCursor(40, 20);
    if(minutes < 10) display.print("0"); display.print(minutes);
    display.setTextSize(1); display.setCursor(6, 54); display.print("Clk: +1 | Dbl: Verify");

  } else if (currentState == STATE_CONFIRM_SETUP) {
    display.setTextSize(1); display.setCursor(14, 4); display.print("2FA TIME VERIFY");
    display.setCursor(12, 16); display.print("Time: ");
    if(hours < 10) display.print("0"); display.print(hours); display.print(":");
    if(minutes < 10) display.print("0"); display.print(minutes);

    int yOpt1 = 34, yOpt2 = 48;
    if (confirmSelection == 0) { display.fillRect(2, yOpt1 - 2, 124, 13, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    display.setCursor(6, yOpt1); display.print("> CONFIRM & START"); display.setTextColor(SSD1306_WHITE);

    if (confirmSelection == 1) { display.fillRect(2, yOpt2 - 2, 124, 13, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    display.setCursor(6, yOpt2); display.print("> GO BACK & EDIT"); display.setTextColor(SSD1306_WHITE);

  } else if (currentState == STATE_CLOCK) {
    display.setTextSize(1); display.setCursor(40, 6); display.print("TimeyBot");
    display.setTextSize(4); display.setCursor(4, 22);
    if(hours < 10) display.print("0"); display.print(hours); display.print(":");
    if(minutes < 10) display.print("0"); display.print(minutes);
    display.setTextSize(1); display.setCursor(34, 54); display.print("Click=Menu");

  } else if (currentState == STATE_MENU) {
    display.setTextSize(1); display.setCursor(50, 4); display.print("MENU");
    display.drawFastHLine(0, 14, 128, SSD1306_WHITE);

    for (int i = scrollOffset; i < MENU_ITEMS_COUNT && i < scrollOffset + 2; i++) {
      int yPos = 20 + ((i - scrollOffset) * 16);
      if (i == menuSelection) { display.fillRect(0, yPos - 3, 128, 15, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
      else { display.setTextColor(SSD1306_WHITE); }
      display.setCursor(8, yPos); display.print(menuItems[i]);
    }
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 54); display.print("Clk=Down  Dbl=OK");

  } else if (currentState == STATE_TIMER) {
    display.setTextSize(1); display.setCursor(48, 4); display.print("TIMER");
    if (!timerRunning) {
      display.setCursor(20, 20); display.print("Set: "); display.print(timerTargetMinutes); display.print(" Mins");
      display.setCursor(6, 54); display.print("Clk: +1m  Dbl: Start");
    } else {
      unsigned long remainingMs = timerDurationMs - (millis() - timerStartTime);
      int remMin = remainingMs / 60000;
      int remSec = (remainingMs % 60000) / 1000;
      display.setTextSize(3); display.setCursor(24, 20);
      if (remMin < 10) display.print("0"); display.print(remMin); display.print(":");
      if (remSec < 10) display.print("0"); display.print(remSec);
      display.setTextSize(1); display.setCursor(14, 54); display.print("Dbl Click to Stop");
    }

  } else if (currentState == STATE_ALARM_HOURS || currentState == STATE_ALARM_MINUTES) {
    display.setTextSize(1); display.setCursor(32, 4); display.print("ALARM SETUP");
    display.setTextSize(3); display.setCursor(24, 20);
    if(alarmHours < 10) display.print("0"); display.print(alarmHours); display.print(":");
    if(alarmMinutes < 10) display.print("0"); display.print(alarmMinutes);
    display.setTextSize(1); display.setCursor(4, 54);
    if (currentState == STATE_ALARM_HOURS) display.print("Clk:Hour+1 Dbl:MinSet");
    else display.print("Clk:Min+1  Dbl:Activate");

  } else if (currentState == STATE_ALARM_ACTIVE) {
    display.setTextSize(1); display.setCursor(32, 4); display.print("ALARM ACTIVE");
    display.setCursor(14, 24); display.print("Target -> ");
    if(alarmHours < 10) display.print("0"); display.print(alarmHours); display.print(":");
    if(alarmMinutes < 10) display.print("0"); display.print(alarmMinutes);
    display.setCursor(4, 54); display.print("Clk:Dismiss Dbl:Menu");

  } else if (currentState == STATE_STOPWATCH) {
    display.setTextSize(1); display.setCursor(32, 4); display.print("STOPWATCH");
    unsigned long currentElapsed = stopwatchElapsedMs;
    if (stopwatchRunning) currentElapsed = millis() - stopwatchStartTime;
    int displayMin = currentElapsed / 60000;
    int displaySec = (currentElapsed % 60000) / 1000;
    int displayMs  = (currentElapsed % 1000) / 10;
    
    display.setTextSize(2); display.setCursor(16, 22);
    if (displayMin < 10) display.print("0"); display.print(displayMin); display.print(":");
    if (displaySec < 10) display.print("0"); display.print(displaySec); display.print(".");
    if (displayMs < 10) display.print("0"); display.print(displayMs);
    
    display.setTextSize(1); display.setCursor(4, 54);
    display.print(stopwatchRunning ? "Clk:Pause | Dbl:Reset" : "Clk:Start | Dbl:Exit");

  } else if (currentState == STATE_SOUND_PLAY) {
    display.setTextSize(1); display.setCursor(22, 6);
    display.print(alarmRinging ? "!!! ALARM !!!" : "TimeyBot Music");
    display.setCursor(6, 26); display.print("Playing:");
    display.setCursor(6, 40); display.print("Earrings.mp3");
    display.setCursor(20, 54); display.print("Tap Key to Dismiss");
  }

  display.display(); 
}

void audio_info(const char *info){
    Serial.print("audio_info: ");
    Serial.println(info);
}