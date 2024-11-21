#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Firebase_ESP_Client.h>
#include <WiFi.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define Firebase credentials
#define WIFI_SSID "NTFiber_47B0_2.4G"
#define WIFI_PASSWORD "AAYUSHM@666"
#define API_KEY "AIzaSyAkvuzmGmXX0XezhSeOqltGKAVfwHHo5M4"
#define DATABASE_URL "https://newupdatedcanteen-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// RFID and Keypad setup
#define SS_PIN 5
#define RST_PIN 4
#define BUZZER_PIN 2
String tagID = "";
String password = "";
boolean isPasswordSet = false;
const int MAX_PASSWORD_LENGTH = 10;
float balance = 1000.00;  // Initial balance
float transactionAmount = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {33, 25, 26, 27};
byte colPins[COLS] = {14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  delay(50);  // Add a small delay after RFID initialization

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  displayWelcomeScreen();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign-up succeeded");
    signupOK = true;
  } else {
    Serial.printf("Firebase sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }
}

void loop() {
  displayMainMenu();

  char mode = keypad.getKey();

  if (mode == '1') {
    adminMode();
  } else if (mode == '2') {
    userMode();
  }
}

void displayWelcomeScreen() {
  display.clearDisplay();
  display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(0, 10);
  display.println(" Welcome!");
  display.display();
  delay(2000);
}

void displayMainMenu() {
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.fillRect(0, 0, SCREEN_WIDTH, 10, WHITE);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(20, 1);
  display.println("Main Menu");
  display.setTextColor(WHITE);
  display.setCursor(0, 15);
  display.println("Select Mode:");
  display.setCursor(0, 25);
  display.println("1: Admin Mode");
  display.setCursor(0, 35);
  display.println("2: User Mode");
  display.display();
}

boolean enterAdminPassword() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, WHITE);
  display.setCursor(10, 5);
  display.setTextColor(WHITE);
  display.println("Enter Admin Pass:");
  display.display();

  String adminPass = getPassword();

  if (adminPass == "1234") {
    return true;
  } else {
    displayMessage("Incorrect Pass", 2000);
    return false;
  }
}

void adminMode() {
  if (!enterAdminPassword()) {
    return;
  }

  while (true) {
    display.clearDisplay();
    display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, WHITE);
    display.setCursor(10, 5);
    display.setTextColor(WHITE);
    display.println("Admin Mode");
    display.setCursor(10, 15);
    display.println("Scan your card");
    display.display();

    if (waitForCard()) {
      playScanSound();

      if (!authenticateCard()) {
        displayMessage("Unauthorized Card", 2000);
        continue;
      }

      while (true) {  // New loop for admin options
        display.clearDisplay();
        display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, WHITE);
        display.setCursor(10, 5);
        display.setTextColor(WHITE);
        display.println("1: Add Balance");
        display.setCursor(10, 15);
        display.println("2: Set Password");
        display.setCursor(10, 25);
        display.println("3: Back to Main");
        display.display();

        char adminChoice = '\0';
        while (!adminChoice) {
          adminChoice = keypad.getKey();
        }

        if (adminChoice == '1') {
          addBalance();
        } else if (adminChoice == '2') {
          setUserPassword();
        } else if (adminChoice == '3') {
          break;  // Exit admin options loop
        } else {
          displayMessage("Invalid Option", 2000);
        }
      }

      if (exitPrompt("Exit Admin Mode?")) {
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        break;
      }
    }
  }
}

void addBalance() {
  // Read the current balance
  float previousBalance = readBalance();
  
  if (previousBalance < 0) {
    displayMessage("Error Reading Balance", 2000);
    return;
  }

  // Display the previous balance
  display.clearDisplay();
  display.setCursor(10, 5);
  display.setTextColor(WHITE);
  display.println("Add Balance");
  display.setCursor(10, 15);
  display.print("Prev Bal: ");
  display.println(previousBalance, 2);  // Display with 2 decimal points
  display.setCursor(10, 25);
  display.println("Enter Amount:");
  display.display();

  float amount = getAmount();
  
  if (amount > 0) {
    // Calculate the new total balance
    float newBalance = previousBalance + amount;
    
    // Write the new balance to Firebase
    if (writeBalance(newBalance)) {
      // Display the updated balance information
      display.clearDisplay();
      display.setCursor(10, 5);
      display.setTextColor(WHITE);
      display.println("Balance Updated");
      display.setCursor(10, 15);
      display.print("Prev Bal: ");
      display.println(previousBalance, 2);  // Show previous balance
      display.setCursor(10, 25);
      display.print("New Bal: ");
      display.println(newBalance, 2);       // Show new balance
      display.display();
      
      delay(2000);  // Delay to let the user see the updated balance
    } else {
      displayMessage("Error Adding Balance", 2000);
    }
  } else {
    displayMessage("Invalid Amount", 2000);
  }
}


void setUserPassword() {
  display.clearDisplay();
  display.setCursor(10, 5);
  display.setTextColor(WHITE);
  display.println("Set Password");
  display.setCursor(10, 15);
  display.println("Enter Password:");
  display.display();

  String newPassword = getPassword();
  if (newPassword.length() > 0) {
    if (writePassword(newPassword)) {
      displayMessage("Password Set", 2000);
    } else {
      displayMessage("Error Setting Password", 2000);
    }
  } else {
    displayMessage("Invalid Password", 2000);
  }
}

boolean writePassword(String password) {
  if (Firebase.ready()) {
    String path = "/users/" + tagID + "/password";
    if (Firebase.RTDB.setString(&fbdo, path, password)) {
      return true;
    } else {
      Serial.println(fbdo.errorReason());
      return false;
    }
  }
  return false;
}

void userMode() {
  while (true) {
    display.clearDisplay();
    display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, WHITE);
    display.setCursor(10, 5);
    display.setTextColor(WHITE);
    display.println("User Mode");
    display.setCursor(10, 15);
    display.println("Scan your card");
    display.display();

    if (waitForCard()) {
      playScanSound();

      if (!authenticateCard()) {
        displayMessage("Unauthorized Card", 1000);
        continue;
      }

      display.clearDisplay();
      display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, WHITE);
      display.setCursor(10, 5);
      display.println("Enter Amount:");
      display.display();

      float amount = getAmount();
      if (amount > 0 && amount <= balance) {
        balance -= amount;
        if (writeBalance(balance)) {
          displayMessage("Transaction Success", 1000);
        } else {
          displayMessage("Transaction Error", 1000);
        }
      } else {
        displayMessage("Invalid Amount", 2000);
      }

      if (exitPrompt("Return to Main Menu?")) {
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        break;
      }
    }
  }
}

boolean writeBalance(float balance) {
  if (Firebase.ready()) {
    String path = "/users/" + tagID + "/balance";
    if (Firebase.RTDB.setFloat(&fbdo, path, balance)) {
      return true;
    } else {
      Serial.println(fbdo.errorReason());
      return false;
    }
  }
  return false;
}

String getPassword() {
  String enteredPassword = "";
  char key;
  while (true) {
    key = keypad.getKey();
    if (key) {
      if (key == '#') {
        return enteredPassword;
      } else if (key == '*') {
        enteredPassword = "";
      } else {
        enteredPassword += key;
      }

      display.clearDisplay();
      display.setCursor(10, 5);
      display.print("Password: ");
      for (int i = 0; i < enteredPassword.length(); i++) {
        display.print("*");
      }
      display.display();
    }
  }
}

float getAmount() {
  String enteredAmount = "";
  char key;
  while (true) {
    key = keypad.getKey();
    if (key) {
      if (key == '#') {
        return enteredAmount.toFloat();
      } else if (key == '*') {
        enteredAmount = "";
      } else {
        enteredAmount += key;
      }

      display.clearDisplay();
      display.setCursor(10, 5);
      display.print("Amount: ");
      display.print(enteredAmount);
      display.display();
    }
  }
}

void playScanSound() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void displayMessage(String message, int delayTime) {
  display.clearDisplay();
  display.setCursor(10, 5);
  display.setTextColor(WHITE);
  display.println(message);
  display.display();
  delay(delayTime);
}


boolean waitForCard() {
  unsigned long startTime = millis();
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    if (millis() - startTime > 10000) {
      return false;
    }
  }
  tagID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      tagID += "0";  // Pad with leading zero if necessary
    }
    tagID += String(mfrc522.uid.uidByte[i], HEX);
  }
  tagID.toUpperCase();
  Serial.print("Scanned tagID: ");
  Serial.println(tagID);
  return true;
}

boolean authenticateCard() {
  // List of authorized RFID card UIDs
  String authorizedUIDs[] = {
    "36395E32",
    "55FC0A2C",
    "A3E0A1FB",
    // Add more UIDs here, including those with different lengths
  };

  tagID.trim();
  
  // Remove the length check
  // if (tagID.length() != 8) {
  //   Serial.println("Invalid UID length");
  //   return false;
  // }

  for (int i = 0; i < sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]); i++) {
    if (tagID == authorizedUIDs[i]) {
      Serial.println("Card authenticated successfully");
      return true;
    }
  }

  Serial.print("Unauthorized card: ");
  Serial.println(tagID);
  return false;
}

float readBalance() {
  if (Firebase.ready()) {
    String path = "/users/" + tagID + "/balance";
    if (Firebase.RTDB.getFloat(&fbdo, path)) {
      return fbdo.floatData();
    } else {
      Serial.println(fbdo.errorReason());
      return -1;
    }
  }
  return -1;
}

boolean exitPrompt(String message) {
  display.clearDisplay();
  display.setCursor(10, 5);
  display.setTextColor(WHITE);
  display.println(message);
  display.setCursor(10, 15);
  display.println("1: Yes, 2: No");
  display.display();
  
  while (true) {
    char key = keypad.getKey();
    if (key == '1') {
      return true;
    } else if (key == '2') {
      return false;
    }
  }
}  
