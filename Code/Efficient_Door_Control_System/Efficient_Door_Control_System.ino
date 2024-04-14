#include <esp_sleep.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define S_HOME 0
#define S_LOGIN 1
#define S_ACTION 2
#define S_NEW_PASSWORD 3
#define S_DEEP_SLEEP 4
uint8_t state = S_HOME;  // Estado inicial

unsigned long lastKey = 0;   //Ultimo tiempo en que se presionó una tecla
unsigned long late = 15000;  // En ms

//Caracteristicas del PIN guardado en memoria
const int passwordLength = 4;
const int passwordAddress = 0;
const char passwordSentinel = '#';  // Valor centinela para indicar que hay una contraseña guardada
char pin[passwordLength + 1];  // PIN ingresado
int indice = 0;    // Índice para llevar el control de los dígitos ingresados
char passwordSaved[passwordLength + 1];

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
uint8_t colPins[COLS] = { 26, 25, 33, 32 };
uint8_t rowPins[ROWS] = { 13, 12, 14, 27 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int LED_PIN = 2;  // GPIO del LED integrado
const int BUTTON = 15;  // GPIO del boton para despertar al MCU

void home() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("WELCOME!!!");
  lcd.setCursor(0, 1);
  lcd.print("Press the A key");
}

void login() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Enter password");
  lcd.setCursor(1, 1);
  lcd.print("PIN: ");
}

void executeActions() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("M1: ");
  lcd.print("M2: ");
  lcd.print("M3: ");
  lcd.setCursor(1, 1);
  lcd.print("ON- ");
  lcd.print("OFF ");
  lcd.print("ON- ");
}

void newPassword() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("New password");
  lcd.setCursor(1, 1);
  lcd.print("PIN: ");
}
// Función para enviar el ESP32 a dormir
void deepSleep() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.print("Deep Sleep...zzz");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1);  // Modo de salida
  esp_deep_sleep_start();
  btStop();
  WiFi.mode(WIFI_OFF);
}

//Funcion para reemplazar el delay
void delayReplacement(unsigned long timeDelay) {
  unsigned long timeInit = millis();
  while (millis() - timeInit < timeDelay);
}

void savePassword(char* pinSave) {
  for (int i = 0; i < passwordLength; i++) {
    EEPROM.write(passwordAddress + i, pinSave[i]);
    Serial.println("WRITE");
    Serial.println(pinSave[i]);
  }
  EEPROM.write(passwordAddress + passwordLength, passwordSentinel);
  EEPROM.commit();
}

void readPassword() {
  for (int i = 0; i < passwordLength; i++) {
    passwordSaved[i] = char(EEPROM.read(passwordAddress + i));
    Serial.println("READ");
    Serial.println(passwordSaved[i]);
  }
  passwordSaved[passwordLength] = '\0';
}

bool shouldAssignDefaultPassword() {
  bool passwordFound = false;
  for (int i = 0; i < passwordLength; i++) {
    if (passwordSaved[i] != '\0') {
      passwordFound = true;
      break;
    }
  }

  if (!passwordFound || EEPROM.read(passwordAddress + passwordLength) != passwordSentinel) {
    return true;  // Asignar contraseña por defecto
  }
  return false;  // No asignar contraseña por defecto
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM.length());
  while (!Serial) {}  // Esperar a que inicie el puerto serie
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLDOWN);
  lcd.init();
  lcd.backlight();
  readPassword();
  if (shouldAssignDefaultPassword()) {
    strcpy(passwordSaved, "1234");
    savePassword(passwordSaved);
  }
  home();
}

void loop() {
  char key = keypad.getKey();
  if (key != NO_KEY) {
    lastKey = millis();
  }
  if ((millis() - lastKey) > late) {
    digitalWrite(LED_PIN, HIGH);
    delayReplacement(100);
    deepSleep();
  }
  switch (state) {
    case S_HOME:
      {
        if (key == 'A') {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_LOGIN;
          login();
        }
      }
      break;
    case S_LOGIN:
      {
        if (key == 'B') {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_HOME;
          home();
        }
        //Comprobar si se presiono un numero
        if (key != NO_KEY && isDigit(key)) {
          if (indice < 4) {
            pin[indice++] = key;
            lcd.print(key);
          }
        }
        if (indice == 4) {
          pin[indice] = '\0';
          if (strcmp(pin, passwordSaved) == 0) {
            digitalWrite(LED_PIN, HIGH);
            delayReplacement(100);
            lcd.clear();
            lcd.print("PIN correct");
            delayReplacement(1000);
            state = S_ACTION;
            executeActions();
          } else {
            lcd.clear();
            lcd.print("PIN incorrect");
            delayReplacement(1000);
            login();
          }
          //Reiniciar para ingresar un nuevo PIN
          indice = 0;
          memset(pin, 0, sizeof(pin));  // Limpiar el arreglo de pin
        }
      }  // FIN state LOGIN
      break;
    case S_ACTION:
      {
        if (key == 'A') {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_NEW_PASSWORD;
          newPassword();
        }
        if (key == 'B') {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_LOGIN;
          login();
        }
      }
      break;
    case S_NEW_PASSWORD:
      {
        if (key != NO_KEY && isDigit(key)) {
          if (indice < 4) {
            pin[indice++] = key;
            lcd.print(key);
          }
        }
        if (indice == 4) {
          pin[indice] = '\0';  //Pin diponible para guardar
          if (key == 'A') {
            digitalWrite(LED_PIN, HIGH);
            delayReplacement(100);
            Serial.println(pin);
            savePassword(pin);
            delayReplacement(200);
            readPassword();
            indice = 0;
            memset(pin, 0, sizeof(pin));
            state = S_LOGIN;
            login();
          }
        }
        if (key == 'B') {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          indice = 0;
          memset(pin, 0, sizeof(pin));
          state = S_LOGIN;
          login();
        }
      }  //FIN state new password
      break;
  }  //Switch key FIN
}
