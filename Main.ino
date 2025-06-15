#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Pins
#define ENC_A 0
#define ENC_B 1
#define ENC_SW 2
#define BTN_LEFT 3
#define BTN_RIGHT 4
#define LED_R 7
#define LED_G 6
#define LED_B 5
#define LED_WHITE 9

// System states
enum State {
  YEAR_SELECTION,
  STORY_GENERATION,
  CHOICE_PENDING,
  STORY_CONTINUATION
};

// Configuration
const char* ssid = "wifi ssid";
const char* password = "wifi password";
const char* Gemini_Token = "gemini api token";
const int MAX_YEAR = 2100;
const int MIN_YEAR = 0;
const int YEAR_STEP = 10;

// Global variables
State currentState = YEAR_SELECTION;
volatile int selectedYear = 2023;
String storyHistory = "";
String choiceA = "";
String choiceB = "";
volatile int encoderPos = 0;
int lastReportedPos = 0;
unsigned long lastEncoderChange = 0;
const long debounceDelay = 50;
unsigned long apiTimeout = 0;
unsigned long lastButtonPress = 0;

// Encoder state tracking
volatile uint8_t lastEncoded = 0;
volatile long encoderValue = 0;

// Function declarations
void updateEncoder();
void updateYearLED(int year);
void setSystemLED(int r, int g, int b);
void checkForRestart();
void resetSystem();
void handleYearSelection();
void startNewStory();
void generateStory();
void parseStory(String response);
void handleChoiceSelection();
void makeChoice(char choice);
void continueStory();
String askGemini(String prompt);

void updateEncoder() {
  uint8_t MSB = digitalRead(ENC_A);
  uint8_t LSB = digitalRead(ENC_B);
  uint8_t encoded = (MSB << 1) | LSB;
  uint8_t sum = (lastEncoded << 2) | encoded;

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue--;

  lastEncoded = encoded;
}

void updateYearLED(int year) {
  int r = constrain(map(year, 1050, MAX_YEAR, 0, 255), 0, 255);
  int g = constrain(map(year, MIN_YEAR, 1050, 0, 255) - map(year, 1050, MAX_YEAR, 0, 255), 0, 255);
  int b = constrain(map(year, MIN_YEAR, 1050, 255, 0), 0, 255);
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

void setSystemLED(int r, int g, int b) {
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_WHITE, OUTPUT);

  // Attach encoder interrupts
  attachInterrupt(digitalPinToInterrupt(ENC_A), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), updateEncoder, CHANGE);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Csatlakozás a Wi-Fi-hez");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nCsatlakozva! IP: " + WiFi.localIP().toString());
  
  // Set initial LED state
  updateYearLED(selectedYear);
}

// ... [rest of your existing loop() and other functions] ...

void loop() {
  // Handle encoder changes
  if (encoderValue != lastReportedPos) {
    int change = (encoderValue - lastReportedPos);
    
    if (currentState == YEAR_SELECTION) {
      selectedYear = constrain(selectedYear + (change * YEAR_STEP), MIN_YEAR, MAX_YEAR);
      updateYearLED(selectedYear);
      Serial.println("Év: " + String(selectedYear));
    } else {
      resetSystem();
    }
    
    lastReportedPos = encoderValue;
    lastEncoderChange = millis();
  }

  // Handle current state
  switch (currentState) {
    case YEAR_SELECTION:
      handleYearSelection();
      break;
      
    case STORY_GENERATION:
      if (millis() - apiTimeout > 20000) {
        Serial.println("Időtúllépés a történet generálásnál!");
        resetSystem();
      }
      break;
      
    case CHOICE_PENDING:
      handleChoiceSelection();
      break;
      
    case STORY_CONTINUATION:
      if (millis() - apiTimeout > 20000) {
        Serial.println("Időtúllépés a történet folytatásnál!");
        resetSystem();
      }
      break;
  }
  
  // Check for long press reset
  checkForRestart();
  
  delay(10);
}


void checkForRestart() {
  if (digitalRead(ENC_SW) == LOW) {
    if (lastButtonPress == 0) {
      lastButtonPress = millis();
    } else if (millis() - lastButtonPress > 2000) {
      resetSystem();
    }
  } else {
    lastButtonPress = 0;
  }
}

void resetSystem() {
  Serial.println("\n--- ÚJ TÖRTÉNET INDÍTÁSA ---");
  currentState = YEAR_SELECTION;
  storyHistory = "";
  choiceA = "";
  choiceB = "";
  updateYearLED(selectedYear);
  digitalWrite(LED_WHITE, LOW);
  
  // Visual feedback
  for(int i = 0; i < 3; i++) {
    setSystemLED(255, 215, 0);
    delay(150);
    updateYearLED(selectedYear);
    delay(150);
  }
}

void handleYearSelection() {
  if (digitalRead(ENC_SW) == LOW && millis() - lastEncoderChange > debounceDelay) {
    if (millis() - lastButtonPress > 1000) {
      startNewStory();
    }
  }
}

void startNewStory() {
  currentState = STORY_GENERATION;
  setSystemLED(0, 0, 255);
  digitalWrite(LED_WHITE, LOW);
  Serial.println("Történet készítése...");
  Serial.println("Kiválasztott év: " + String(selectedYear)); // DEBUG
  apiTimeout = millis();
  generateStory();
  delay(300);
}

void generateStory() {
  String prompt = 
               
"Kérlek, hozz létre egy eredeti, körülbelül 30 szavas, gyerekek számára is könnyen befogadható kalandtörténetet, amely a " + String(selectedYear) + " évből származik."

"A történet főszereplője egy korának megfelelő, érdekes gyermek (fiú vagy lány, ahogy illik). Mutasd be röviden a korabeli élethelyzeteket és környezetet (pl. milyen volt egy tipikus nap, milyen játékokkal játszottak, milyen tárgyak vették körül őket, milyen kihívásokkal szembesülhettek, de kerüld a túl bonyolult történelmi eseményeket)."

"A narratíva legyen lebilincselő és logikusan építkezzen. A főszereplőnek egy konkrét problémával vagy kihívással kell szembenéznie, ami a történet mozgatórugója lesz. A történet szőjön bele egy időtlen tanulságot (például barátság, bátorság, kitartás, őszinteség, segítségnyújtás, találékonyság, együttérzés, a környezet szeretete, stb.), anélkül, hogy közvetlenül megnevezné azt. A történések következzenek egymásból, vezessenek egy fordulóponthoz."

"A történet végén, amikor a főszereplőnek döntést kell hoznia, pontosan az alábbi formátumban adj meg két választási lehetőséget: "

"A választási lehetőségek formátuma: "
"A: [Max 5 szóban, konkrét cselekvés] "
"B: [Max 5 szóban, alternatív cselekvés] ";

  Serial.println("Prompt küldése: " + prompt); // DEBUG
  String response = askGemini(prompt);
  
  if (response.length() > 0) {
    parseStory(response);
    currentState = CHOICE_PENDING;
    setSystemLED(255, 255, 0);
    digitalWrite(LED_WHITE, HIGH);
  } else {
    Serial.println("Nem sikerült történetet generálni!");
    resetSystem();
  }
}

void parseStory(String response) {
  // Extract choices
  choiceA = "";
  choiceB = "";
  
  int choiceStart = response.indexOf("A:");
  if (choiceStart == -1) choiceStart = response.indexOf("a:");
  
  if (choiceStart != -1) {
    String choices = response.substring(choiceStart);
    storyHistory = response.substring(0, choiceStart);
    
    // Parse choices
    int bPos = choices.indexOf("B:");
    if (bPos == -1) bPos = choices.indexOf("b:");
    
    if (bPos != -1) {
      choiceA = choices.substring(0, bPos);
      choiceB = choices.substring(bPos);
      
      // Clean up choice strings
      choiceA.replace("A:", "");
      choiceA.replace("a:", "");
      choiceA.trim();
      
      choiceB.replace("B:", "");
      choiceB.replace("b:", "");
      choiceB.trim();
    }
  } else {
    // Fallback if choices not found
    storyHistory = response;
    choiceA = "Igen";
    choiceB = "Nem";
  }
  
  Serial.println("\n=== TÖRTÉNET ===");
  Serial.println(storyHistory);
  Serial.println("\n=== VÁLASZTÁSI LEHETŐSÉGEK ===");
  Serial.println("BAL GOMB: " + choiceA);
  Serial.println("JOBB GOMB: " + choiceB);
  
}

void handleChoiceSelection() {
  if (digitalRead(BTN_LEFT) == LOW) {
    makeChoice('A');
  } 
  else if (digitalRead(BTN_RIGHT) == LOW) {
    makeChoice('B');
  }
}

void makeChoice(char choice) {
  Serial.println("Választás: " + String(choice));
  digitalWrite(LED_WHITE, LOW);
  currentState = STORY_CONTINUATION;
  setSystemLED(0, 0, 255);  // Blue - Processing
  apiTimeout = millis();
  
  String choiceText = (choice == 'A') ? choiceA : choiceB;
  storyHistory += "\n\nVálasztás: " + String(choice) + ") " + choiceText + "\n";
  
  continueStory();
  delay(300);
}

void continueStory() {
  String prompt = "Folytasd a történetet a választás alapján! "
                  "Írj még 1-2 rövid mondatot, amely logikusan következik a választás alapján az előző történetből "
                  "majd ajánlj fel két új választási lehetőséget pontosan ugyanabban a formátumban:\n"
                  "A: [Az A lehetőség leírása]\nB: [A B lehetőség leírása]";
  
  String response = askGemini(storyHistory + "\n" + prompt);
  
  if (response.length() > 0) {
    parseStory(response);
    currentState = CHOICE_PENDING;
    setSystemLED(255, 255, 0);  // Yellow - Choice pending
    digitalWrite(LED_WHITE, HIGH);
  } else {
    Serial.println("Nem sikerült folytatni a történetet!");
    resetSystem();
  }
}

String askGemini(String prompt) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi kapcsolat megszakadt! Újracsatlakozás...");
    WiFi.reconnect();
    
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
      delay(500);
      Serial.print(".");
      retryCount++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nSikertelen újracsatlakozás!");
      return "";
    }
    Serial.println("\nÚjracsatlakozva!");
  }
  
  HTTPClient https;
  String responseText = "";
  
  // Escape special characters for JSON
  prompt.replace("\"", "\\\"");
  prompt.replace("\n", "\\n");
  
  String payload = String("{\"contents\":[{\"parts\":[{\"text\":\"" + prompt + "\"}]}],") +
                   "\"generationConfig\":{\"maxOutputTokens\":1000}}";
  
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(Gemini_Token);
  
  Serial.println("Gemini API hívása...");
  
  if (https.begin(url)) {
    https.addHeader("Content-Type", "application/json");
    https.setTimeout(15000);
    
    int httpCode = https.POST(payload);
    
    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      DynamicJsonDocument doc(12288);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        if (doc.containsKey("candidates") && doc["candidates"].size() > 0) {
          responseText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
          responseText.replace("\\n", "\n");
          Serial.println("API válasz fogadva!");
        } else {
          Serial.println("Hibás API válasz formátum");
          if (doc.containsKey("error")) {
            Serial.println("Hiba: " + doc["error"]["message"].as<String>());
          }
        }
      } else {
        Serial.print("JSON hiba: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.printf("HTTP hiba: %d - %s\n", httpCode, https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("HTTPS kapcsolat sikertelen");
  }
  
  return responseText;
}
