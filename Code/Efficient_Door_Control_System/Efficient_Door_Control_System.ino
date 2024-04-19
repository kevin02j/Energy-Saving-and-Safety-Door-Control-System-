#include <esp_sleep.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

/* Definition of process states */
#define S_HOME 0
#define S_LOGIN 1
#define S_ACTION 2
#define S_NEW_PASSWORD 3
uint8_t state = S_HOME;  

/* Variables to track time */
unsigned long lastKeyPressed = 0;      /* Last time a key was pressed */
unsigned long lateSleep = 30000;       /* Time (ms) to wait before entering Deep Sleep mode */
unsigned long lastActionExecuted = 0;  /* Last time an action was executed */
unsigned long lateAction = 10000;      /* Time (ms) to wait if no action is executed */

/* Variables for password management */
int indice = 0;               /* Tracks the entered digits */
int password;                 /* User-entered PIN */
int currentPassword = 1234;   /* PIN stored in memory, assigns a default PIN if no saved information */
int passwordAdress = 0;       /* Address in EEPROM memory where the PIN is stored */
char key;                     /* Stores the value of the keypad key pressed */
String keyWord;               

/* Matrix keypad configuration */
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

/* Create LCD object */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* Pin definitions */
const int LED_PIN = 2;   /* Integrated LED GPIO */
const int BUTTON = 15;   /* Button GPIO to wake up the MCU */
const int GATE_1 = 34;   /* GPIO for gate sensor 1 */
const int GATE_2 = 35;   /* GPIO for gate sensor 2 */
const int DC_MOTOR = 23; /* GPIO to activate the DC motor */
bool stateMotor = false;

/* Display home interface */
void home() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("WELCOME!!!");
  lcd.setCursor(0, 1);
  lcd.print("Press the A key");
}

/* Login interface */
void login() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Enter password");
  lcd.setCursor(3, 1);
  lcd.print("PIN: ");
}

/* Interface to monitor executed actions */
void executeActions() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("G1: ");
  lcd.setCursor(6, 0);
  lcd.print("DC: ");
  lcd.setCursor(12, 0);
  lcd.print("G2: ");
  lcd.setCursor(1, 1);
  lcd.print("OFF");
  lcd.setCursor(6, 1);  
  lcd.print("OFF");
  lcd.setCursor(12, 1);
  lcd.print("OFF");
}

/* Interface for changing password */
void newPassword() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("New password");
  lcd.setCursor(3, 1);
  lcd.print("PIN: ");
}

/* Function for ESP32 to enter Deep Sleep mode */ 
void deepSleep() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.print("Deep Sleep...zzz");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1); 
  btStop();
  WiFi.mode(WIFI_OFF);
  esp_deep_sleep_start();
}

/* Delay function replacement */
void delayReplacement(unsigned long timeDelay) {
  unsigned long timeInit = millis();
  while (millis() - timeInit < timeDelay);
}

/*Setup loop */
void setup() {
  /* Serial Port Config */
  Serial.begin(115200);
  while (!Serial) {}

  /* Pin configuration */
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLDOWN);
  pinMode(GATE_1, INPUT);
  pinMode(GATE_2, INPUT);
  pinMode(DC_MOTOR, OUTPUT);
  digitalWrite(DC_MOTOR, HIGH);

  /* LCD I2C Config */
  lcd.init();
  lcd.backlight();
  home();

  /* EEPROM memory config */
  EEPROM.begin(512);
  EEPROM.get(passwordAdress, password);

  /* Check for a previously saved password */
  if (password != currentPassword && password > 0) {
    currentPassword = password;
  }
  Serial.println(currentPassword);
}

/* Main loop */
void loop() {
  key = keypad.getKey();  /* Get the pressed key */
  bool gateFlag = true;

  /* If no key is pressed within 30s, ESP enters Deep Sleep mode */
  if (key != NO_KEY) 
  {
    lastKeyPressed = millis();
  }
  if ((millis() - lastKeyPressed) > lateSleep) 
  {
    digitalWrite(LED_PIN, HIGH);
    delayReplacement(100);
    deepSleep();
  }

  /* Process state operation */ 
  switch (state) {
    case S_HOME:
      {
        /* Go to the next state */
        if (key == 'A') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_LOGIN;
          login();
        }

        /* Send MCU to Deep Sleep mode */
        else if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }
      }
      break;
    case S_LOGIN:
      {
        /* Return to previous state */
        if (key == 'B') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_HOME;
          home();
        }

        /* Send MCU to Deep Sleep mode */
        if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }

        /* Check if a valid key is pressed to enter the PIN */
        if ((key != NO_KEY) && (isDigit(key) || (key == '*'))) 
        {
          /* Delete the last entered digit */
          if (key == '*') 
          {
            if (indice > 0) 
            {
              indice--;
              keyWord.remove(indice, 1);  
              lcd.setCursor(indice + 8, 1);   
              lcd.print(" ");             
            }
          } 
          else 
          {
            if (indice < 4) 
            {
              keyWord += key;
              lcd.setCursor(indice + 8, 1);
              lcd.print(key);
              indice++;
            }
          }
        } 

        /* PIN has 4 digits */
        if (indice == 4) 
        {
          password = keyWord.toInt();  

          /* Check if the password is correct */   
          if (password == currentPassword) 
          {
            digitalWrite(LED_PIN, HIGH);
            delayReplacement(100);
            lcd.clear();
            lcd.print("PIN correct");
            delayReplacement(1000);
            state = S_ACTION;
            executeActions();
          } 
          else 
          {
            lcd.clear();
            lcd.print("PIN incorrect");
            delayReplacement(1000);
            login();
          }

          //Clear for entering a new PIN
          indice = 0;
          keyWord = "";
        }
      }  // END state LOGIN
      break;
    case S_ACTION:
      {
        /* If no action is executed in < lateAction, it locks */
        lastActionExecuted = lastKeyPressed;
        if ((millis() - lastActionExecuted) > lateAction) 
        {
          state = S_LOGIN;
          gateFlag = false;
          login();
        }

        /* Go to the next state */
        if (key == 'A') 
        {
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(DC_MOTOR, HIGH);
          delayReplacement(100);
          state = S_NEW_PASSWORD;
          newPassword();
          gateFlag = false;
          lastActionExecuted = millis();
        }

        /* Return to previous state */
        if (key == 'B') 
        {
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(DC_MOTOR, HIGH);
          delayReplacement(100);
          state = S_LOGIN;
          login();
          gateFlag = false;
          lastActionExecuted = millis();
        }
        
        /* DC motor on/off control */
        if (key == 'C') 
        {
          stateMotor = !stateMotor;
          if (!stateMotor) 
          {
            digitalWrite(DC_MOTOR, HIGH);
            lcd.setCursor(6, 1);
            lcd.print("OFF");
          } 
          else 
          {
            digitalWrite(DC_MOTOR, LOW);
            lcd.setCursor(6, 1);
            lcd.print("ON ");
          }
          lastActionExecuted = millis();
        }

        /* Send MCU to Deep Sleep mode */
        if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }

        /* Check if the state changed in the previous blocks */
        if (gateFlag) 
        {
          /* Check the state of the first gate sensor */
          if (digitalRead(GATE_1) == 0) 
          {
            lcd.setCursor(1, 1);
            lcd.print("ON ");
            lastKeyPressed = millis();
            lastActionExecuted = millis();
          } 
          else 
          {
            lcd.setCursor(1, 1);
            lcd.print("OFF");
          }

          /* Check the state of the second gate sensor */
          if (digitalRead(GATE_2) == 0) 
          {
            lcd.setCursor(12, 1);
            lcd.print("ON ");
            lastKeyPressed = millis();
            lastActionExecuted = millis();
          } 
          else 
          {
            lcd.setCursor(12, 1);
            lcd.print("OFF");
          }
        }
      }
      break;
    case S_NEW_PASSWORD:
      {
        /* Send MCU to Deep Sleep mode */
        if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }

        /* Don't save the password */
        if (key == 'B') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          indice = 0;
          keyWord = "";
          state = S_LOGIN;
          login();
        }

        /* Read and save the new PIN */
        if ((key != NO_KEY) && (isDigit(key) || (key == '*'))) 
        {
          /* Delete the last entered character */
          if (key == '*') 
          {
            if (indice > 0) 
            {
              indice--;
              keyWord.remove(indice, 1);  
              lcd.setCursor(indice + 8, 1);   
              lcd.print(" ");             
            }
          } 
          else 
          {
            if (indice < 4) 
            {
              keyWord += key;
              lcd.setCursor(indice + 8, 1);
              lcd.print(key);
              indice++;
            }
          }
        } 
        
        /* Password size is correct */
        if (indice == 4) 
        {
          password = keyWord.toInt();

          /* Save the new password */
          if (key == 'A') 
          {
            digitalWrite(LED_PIN, HIGH);
            delayReplacement(100);
            EEPROM.put(passwordAdress, password);
            EEPROM.commit();
            delayReplacement(200);
            currentPassword = password;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("PIN saved!!");
            delayReplacement(1000);
            indice = 0;
            keyWord = "";
            state = S_LOGIN;
            login();
          }
        }
      }  //END state new password
      break;
    default:
    {
      Serial.println("State not defined");
    }
    break;
  }  //Switch key END
}
