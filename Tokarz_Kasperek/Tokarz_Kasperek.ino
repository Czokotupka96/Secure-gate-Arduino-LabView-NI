#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Servo.h>
#include <ctype.h>

bool isNumber(const String &s) {
  if (s.length() == 0) return false;

  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i])) {
      return false;
    }
  }
  return true;
}

String rot13_encode(String input) {
  String output = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c >= 'A' && c <= 'Z') {           // A-Z
      c = ((c - 'A' + 13) % 26) + 'A';
    } else if (c >= 'a' && c <= 'z') {    // a-z
      c = ((c - 'a' + 13) % 26) + 'a';
    } else if (c >= '0' && c <= '9') {    // 0-9
      c = ((c - '0' + 13) % 10) + '0';
    } else {                              // other symbols
      c = c;
    }
    output += c;
  }
  return output;
}

String rot13_decode(String input) {
  String output = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c >= 'A' && c <= 'Z') {           // A-Z
      c = ((c - 'A' + 13) % 26) + 'A';
    } else if (c >= 'a' && c <= 'z') {    // a-z
      c = ((c - 'a' + 13) % 26) + 'a';
    } else if (c >= '0' && c <= '9') {    // 0-9
      c = ((c - '0' + 7) % 10) + '0';     // reverse of +13 mod10
    } else {                              // other symbols
      c = c;
    }
    output += c;
  }
  return output;
}


// --- LCD ---
LiquidCrystal lcd(10, 11, 12, A3, A4, A5);
//LiquidCrystal lcd(21, 22, 19, 18, 17, 16);



// --- LED + buzzer ---
const int ledPinCzerw = A2; //36;
const int ledPinZiel = A0; //36;
const int buzzerPin = 13; //39;

// --- Serwo ---
Servo myservo;

// --- Klawiatura ---
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};
//byte rowPins[ROWS] = {34, 35, 25, 26};
//byte colPins[COLS] = {13, 14, 23, 4};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Kod ---
String SECRET = "1111";      // Domyślny kod
String enteredCode = "";     // Kod wpisywany z klawiatury
boolean block = 0;
unsigned int lastBlockTime = 0;

//init zmienne
int wrongAttempts;     // licznik błędnych prób
  unsigned long lockUntil;

// --------------------------
// KOMUNIKACJA LABVIEW → ZMIANA KODU
// --------------------------
void processSerial() {

  if (Serial.available()) {
    String cmd = rot13_decode(Serial.readStringUntil('\n'));
    cmd.trim();

    //Serial.print(cmd);
    // SET:xxxx → ustaw nowy kod
    if (cmd.startsWith("SET:")) {
      String newCode = cmd.substring(4);

      // wymagamy 4 cyfr
      if (newCode.length() == 4 && isNumber(newCode)) {
        SECRET = newCode;
        //Serial.println("SETOK");
        Serial.println(rot13_encode(": Ustawiono prawidlowo kod na " + newCode));
      } else {
        Serial.println(rot13_encode(": ERROR Nie ustawiono nowego kodu"));
      }
    }

    if(cmd == "OPEN"){
      openDoor();
      Serial.println(rot13_encode(": Otwarto bramke zdalnie"));
    }
    if(cmd == "BLOCK"){
      if(!block){Serial.println(rot13_encode(": Zablokowano bramke zdalnie"));}
      blockDoor();
    }
  }
}

// --------------------------
// SYGNALIZACJA BŁĘDU
// --------------------------
void playError() {
  //digitalWrite(buzzerPin, HIGH);
  tone(buzzerPin, 1000);
  analogWrite(ledPinCzerw, 255);
  delay(1000);
  //digitalWrite(buzzerPin, LOW);
  noTone(buzzerPin);
  analogWrite(ledPinCzerw, 0);
}

// --------------------------
// OTWIERANIE
// --------------------------
void openDoor() {
  lcd.clear();
  lcd.print("Otwieranie...");

  analogWrite(ledPinZiel, 255);
  tone(buzzerPin, 440);
  delay(200);
  tone(buzzerPin, 494);
  delay(200);
  tone(buzzerPin, 554);
  delay(200);
  noTone(buzzerPin);
  analogWrite(ledPinZiel, 0);

  myservo.write(180);   // otwórz
  delay(1500);
  myservo.write(0);     // zamknij
  delay(1500);
  myservo.write(90);    // pozycja neutralna
  
  lcd.clear();
  lcd.print("Wpisz kod:");
  lcd.setCursor(0, 1);
}

void blockDoor(){
  lastBlockTime = millis(); // zapamiętaj kiedy ostatnio był BLOCK block = 1; lcd.clear(); lcd.print("ZABLOKOWANE");
  lcd.clear();
  lcd.print("ZABLOKOWANE");
  block = 1;
}

// --------------------------
// SETUP
// --------------------------
void setup() {
  pinMode(ledPinCzerw, OUTPUT);
  pinMode(ledPinZiel, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  int wrongAttempts = 0;         // licznik błędnych prób
  unsigned long lockUntil = 0;   // czas do którego klawiatura jest zablokowana


  myservo.attach(A1);
  //myservo.attach(12);
  myservo.write(90);

  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.print("Wpisz kod:");
  lcd.setCursor(0, 1);
}

// --------------------------
// LOOP
// --------------------------
void loop() {
  int delta1, delta2, delta3;
  /*
  Serial.print("Free RAM: ");
  Serial.println(freeMemory());
  delay(300); */
  processSerial();

     // obsługa komend LabVIEW
  // ---- BLOKADA PO 3 BŁĘDACH ----
  if (millis() < lockUntil) {
    lcd.setCursor(0, 0);
    lcd.print("Zablokowane!   ");
    lcd.setCursor(0, 1);
    lcd.print("Czekaj...      ");
    return;   // NIC NIE ROBIMY
}  else if (lockUntil != 0) {  // po zakończeniu timeout
    lcd.clear();
    lcd.print("Wpisz kod:");
    lcd.setCursor(0, 1);
    lockUntil = 0;  // reset flagi blokady
}

// jeśli minęło 300 ms od ostatniego BLOCK → odblokuj
if (block && millis() - lastBlockTime > 1000) {
    block = 0;
    lcd.clear();
    lcd.print("Wpisz kod:");
    lcd.setCursor(0, 1);
    Serial.println(rot13_encode(": Koniec zupelnej blokady bramki zdalnie"));
} else if(block){return;}


  char key = keypad.getKey();

  if (key) {

    // reset wpisywania
    if (key == '*') {
      enteredCode = "";
      lcd.clear();
      lcd.print("Wpisz kod:");
      lcd.setCursor(0, 1);
      delta1=delta2=delta3=0;
      return;
    }

    // tylko cyfry
    if (key >= '0' && key <= '9') {

      if (enteredCode.length() < 4) {
        enteredCode += key;
        lcd.print("*");
        switch(enteredCode.length()){
          case 1:
            delta1=millis();
            break;
          case 2:
            delta1 = millis()-delta1;
            delta2 = millis();
            break;
          case 3:
            delta2 = millis()-delta2;
            delta3 = millis();
            break;
        }
      }

      // sprawdzamy po 4 znakach
      if (enteredCode.length() == 4) {
        delta3 = millis()-delta3;
        if (enteredCode == SECRET) {
          wrongAttempts = 0;   // reset błędów
          openDoor();
          Serial.println(rot13_encode(": Otwarto bramke - " + enteredCode + ", Czasy klikan (ms): " + delta1 +" " + delta2 + " " + delta3));
        } else {
          wrongAttempts++;
          lcd.clear();
          lcd.print("Bledny kod!");
          Serial.println(rot13_encode(": Wprowadzono bledny kod - " + enteredCode + ", Czasy klikan (ms): " + delta1 +" " + delta2 + " " + delta3));
          playError();
          delay(1200);

          if (wrongAttempts >= 3) {
            lcd.clear();
            lcd.print("3 bledy!");
            lcd.setCursor(0, 1);
            lcd.print("Blokada 5s");

            lockUntil = millis() + 5000;  // blokada 5 sekund
            wrongAttempts = 0;             // reset licznika po wejściu w timeout
            delay(2000);
            enteredCode = "";
            Serial.println(rot13_encode(": Zablokowano dzialanie na 5 sekund"));
            return; // przerwij działanie loop na ten cykl
          }

          lcd.clear();
          lcd.print("Wpisz kod:");
          lcd.setCursor(0, 1);
        }

        enteredCode = "";
      }
    }
  }
  
}
