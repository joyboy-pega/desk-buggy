#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

// ---------- PIN DEFINITIONS (Standard ESP32 VSPI) ----------
#define TFT_CS          5    // Chip Select
#define TFT_DC          2    // Data / Command
#define TFT_RST         4    // Reset
#define TFT_MOSI       23    // VSPI MOSI (SDA)
#define TFT_SCLK       18    // VSPI SCK (SCL)
#define TFT_BL         32    // Backlight PWM pin (LED)

#define FP_RX          16    // ESP32 RX2 <- Fingerprint Sensor TX
#define FP_TX          17    // ESP32 TX2 -> Fingerprint Sensor RX

#define BUZZER_PIN     13    // Buzzer pin
#define BOOT_BUTTON     0    // On-board BOOT button for simulation

// ---------- COLOR PALETTE (RGB565) ----------
#define C_BG          0x0000 // Deep Black (#000000)
#define C_WHITE       0xFFFF // Crisp White (#FFFFFF)
#define C_GREEN       0x07E0 // Bright Emerald Green (#00FF66)
#define C_RED         0xF904 // Crimson Red (#E01A22)
#define C_CARD        0x10A2 // Slate Card (#12151A)
#define C_BORDER      0x2965 // Outline Gray (#282C34)
#define C_MUTED       0x7BEF // Subtitle Gray (#7A808C)
#define C_AMBER       0xFD20 // Amber Orange (#FFAA00)

Preferences prefs;

struct Employee {
  uint16_t id;
  char name[24];
  bool isCheckedIn;
};

#define MAX_EMPLOYEES 16
Employee employees[MAX_EMPLOYEES];
int employeeCount = 0;

void loadEmployeeDatabase() {
  prefs.begin("avian-db", false);
  employeeCount = prefs.getInt("count", 0);

  if (employeeCount <= 0) {
    employeeCount = 3;
    employees[0] = {1, "Joyboy", false};
    employees[1] = {2, "Alice", false};
    employees[2] = {3, "Pega", false};

    prefs.putInt("count", employeeCount);
    for (int i = 0; i < employeeCount; i++) {
      char keyId[32], keyName[32];
      snprintf(keyId, sizeof(keyId), "id_%d", i);
      snprintf(keyName, sizeof(keyName), "name_%d", i);
      prefs.putUShort(keyId, employees[i].id);
      prefs.putString(keyName, employees[i].name);
    }
  } else {
    for (int i = 0; i < employeeCount && i < MAX_EMPLOYEES; i++) {
      char keyId[32], keyName[32];
      snprintf(keyId, sizeof(keyId), "id_%d", i);
      snprintf(keyName, sizeof(keyName), "name_%d", i);
      employees[i].id = prefs.getUShort(keyId, i + 1);
      String n = prefs.getString(keyName, "Worker");
      strlcpy(employees[i].name, n.c_str(), sizeof(employees[i].name));
      employees[i].isCheckedIn = false;
    }
  }
}

void saveNewEmployee(uint16_t slot, const char* name) {
  int foundIdx = -1;
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == slot) { foundIdx = i; break; }
  }
  if (foundIdx >= 0) {
    strlcpy(employees[foundIdx].name, name, sizeof(employees[foundIdx].name));
    char keyName[32];
    snprintf(keyName, sizeof(keyName), "name_%d", foundIdx);
    prefs.putString(keyName, name);
  } else if (employeeCount < MAX_EMPLOYEES) {
    int idx = employeeCount;
    employees[idx].id = slot;
    strlcpy(employees[idx].name, name, sizeof(employees[idx].name));
    employees[idx].isCheckedIn = false;
    employeeCount++;

    prefs.putInt("count", employeeCount);
    char keyId[32], keyName[32];
    snprintf(keyId, sizeof(keyId), "id_%d", idx);
    snprintf(keyName, sizeof(keyName), "name_%d", idx);
    prefs.putUShort(keyId, slot);
    prefs.putString(keyName, name);
  }
}

Employee* getEmployeeBySlot(uint16_t slot) {
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == slot) return &employees[i];
  }
  return nullptr;
}

void setup() { loadEmployeeDatabase(); }
void loop() {}
