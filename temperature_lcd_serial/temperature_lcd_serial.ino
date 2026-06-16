/*
  Temperature Reading + LCD Display + Serial Transmission
  ---------------------------------------------------------
  Hardware : Arduino Uno, LM35 analog temperature sensor, 16x2 character LCD
  Behaviour:
    Row 1 (top)    -> candidate name (scrolls left automatically if > 16 chars)
    Row 2 (bottom) -> live temperature reading
  Every reading is also sent to the PC over USB serial as a single text line:
    TEMP:<value>\n        e.g.  TEMP:23.45

  Wiring (16x2 LCD with PCF8574 I2C backpack - only 4 pins: GND, VCC, SDA, SCL):
    LCD GND -> Arduino GND
    LCD VCC -> Arduino 5V
    LCD SDA -> Arduino A4   (SDA)
    LCD SCL -> Arduino A5   (SCL)
    (no contrast pot or backlight resistor needed - the backpack handles both;
     most backpacks have a small trim potentiometer on the board itself for contrast)

    LM35 VCC -> 5V       LM35 GND -> GND      LM35 OUT -> A0

  Library required (install via Library Manager): "LiquidCrystal I2C" by Frank de Brabander
  If the screen stays blank, your I2C address might not be 0x27 - run an I2C scanner
  sketch (search "Arduino I2C scanner") to find the correct address and change it below.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ----------------------------------------------------------------------
// CONFIGURATION - edit these two lines for your exam/candidate setup
// ----------------------------------------------------------------------
const char* CANDIDATE_NAME = "ganwa";  // shown on row 1, scrolls if >16 chars
const int   TEMP_SENSOR_PIN = A0;                 // LM35 analog output pin
const int   LCD_I2C_ADDRESS = 0x27;               // common addresses: 0x27 or 0x3F

// I2C LCD: address, columns, rows. SDA/SCL wiring is fixed (A4/A5 on Uno),
// nothing to configure in code for those.
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);

// ----------------------------------------------------------------------
// Timing constants
// ----------------------------------------------------------------------
const unsigned long TEMP_READ_INTERVAL_MS = 1000;  // how often to sample + send temperature
const unsigned long SCROLL_INTERVAL_MS    = 400;    // how often the name shifts left
const int LCD_COLUMNS = 16;

// ----------------------------------------------------------------------
// Scrolling state for row 1
// ----------------------------------------------------------------------
String scrollBuffer;          // candidate name + gap, used as a circular buffer
bool   nameNeedsScrolling;
int    scrollIndex   = 0;
unsigned long lastScrollTime = 0;
bool   staticNamePrinted = false;

// ----------------------------------------------------------------------
// Temperature state
// ----------------------------------------------------------------------
unsigned long lastTempReadTime = 0;

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  nameNeedsScrolling = strlen(CANDIDATE_NAME) > LCD_COLUMNS;
  if (nameNeedsScrolling) {
    // 3-space gap so the scroll reads as one continuous loop instead of
    // the end of the name butting straight into its own start
    scrollBuffer = String(CANDIDATE_NAME) + "   ";
  }

  lcd.setCursor(0, 1);
  lcd.print("Reading...");
}

void loop() {
  updateNameDisplay();

  if (millis() - lastTempReadTime >= TEMP_READ_INTERVAL_MS) {
    lastTempReadTime = millis();
    float tempC = readTemperatureC();
    updateTemperatureDisplay(tempC);
    sendTemperatureOverSerial(tempC);
  }
}

// Reads the LM35 several times and averages it for a steadier value.
// LM35 outputs 10mV per degree C, so degC = voltage * 100.
float readTemperatureC() {
  const int samples = 8;
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(TEMP_SENSOR_PIN);
    delay(2);
  }
  float avgReading = (float)total / samples;
  float voltage = avgReading * (5.0 / 1023.0);
  float tempC = voltage * 100.0;
  return tempC;
}

// Row 1: prints the name once if it fits, or scrolls it left on a timer if it doesn't.
void updateNameDisplay() {
  if (!nameNeedsScrolling) {
    if (!staticNamePrinted) {
      lcd.setCursor(0, 0);
      String padded = String(CANDIDATE_NAME);
      while (padded.length() < LCD_COLUMNS) padded += ' ';
      lcd.print(padded.substring(0, LCD_COLUMNS));
      staticNamePrinted = true;
    }
    return;
  }

  if (millis() - lastScrollTime >= SCROLL_INTERVAL_MS) {
    lastScrollTime = millis();
    lcd.setCursor(0, 0);
    String window = "";
    int bufLen = scrollBuffer.length();
    for (int i = 0; i < LCD_COLUMNS; i++) {
      window += scrollBuffer.charAt((scrollIndex + i) % bufLen);
    }
    lcd.print(window);
    scrollIndex = (scrollIndex + 1) % bufLen;
  }
}

// Row 2: "Temp:23.45 C", padded/truncated to exactly 16 characters so old
// digits never linger on screen when the new reading is shorter.
void updateTemperatureDisplay(float tempC) {
  String row2 = "Temp:" + String(tempC, 2);
  row2 += (char)223;  // degree symbol on HD44780 character set
  row2 += "C";
  while (row2.length() < LCD_COLUMNS) row2 += ' ';
  lcd.setCursor(0, 1);
  lcd.print(row2.substring(0, LCD_COLUMNS));
}

// Sends one line per reading to the PC, e.g. "TEMP:23.45"
void sendTemperatureOverSerial(float tempC) {
  Serial.print("TEMP:");
  Serial.println(tempC, 2);
}
