#include <esp_sleep.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

/* Definicion de los estados del proceso */ 
#define S_HOME 0
#define S_LOGIN 1
#define S_ACTION 2
#define S_NEW_PASSWORD 3
uint8_t state = S_HOME;  

/* Variables para llevar control del tiempo */
unsigned long lastKeyPressed = 0;      /* Ultima vez en que se presionó una tecla */
unsigned long lateSleep = 30000;       /* Tiempo (ms) de espera para entrar al modo Deep Sleep */
unsigned long lastActionExecuted = 0;  /* Ultima ves que se ejecuto una accion */
unsigned long lateAction = 10000;      /* Tiempo (ms) de espera si no se ejecuta ninguna accion */

/* Varaibles para manejo de la contraseña */
int indice = 0;               /* Lleva el control de los digitos ingresados */
int password;                 /* Pin que ingresa el usuario */
int currentPassword = 1234;   /* Pin guardado en la memoria, si no hay informacion guardada asigna un PIN por defecto */
int passwordAdress = 0;       /* Direccion en la memoria EEPROM donde se encuentra el pin */
char key;                     /* Guarda el valor de la tecla del keypad que se presiona */
String keyWord;               

/* Configuracion del teclado matricial */
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

/* Crear objeto lcd */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* Definicion de pines */
const int LED_PIN = 2;   /* GPIO del LED integrado */
const int BUTTON = 15;   /* GPIO del boton para despertar al MCU */
const int GATE_1 = 34;   /* GPIO del sensor de final de carrera 1 */
const int GATE_2 = 35;   /* GPIO del sensor de final de carrera 2 */
const int DC_MOTOR = 4;  /* GPIO para activar el motor DC */
bool stateMotor = false;

/* Mostrar interfaz de inicio */
void home() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("WELCOME!!!");
  lcd.setCursor(0, 1);
  lcd.print("Press the A key");
}

/* Interfaz de inicio de sesion */
void login() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Enter password");
  lcd.setCursor(3, 1);
  lcd.print("PIN: ");
}

/* Interfaz para monitorear las acciones ejecutadas */
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

/* Interfaz para el cambio de contraseña */
void newPassword() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("New password");
  lcd.setCursor(3, 1);
  lcd.print("PIN: ");
}

/* Función que el ESP32 entre en modo de Deep Sleep */ 
void deepSleep() {
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.print("Deep Sleep...zzz");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1); 
  esp_deep_sleep_start();
  btStop();
  WiFi.mode(WIFI_OFF);
}

/* Funcion para reemplazar el delay */
void delayReplacement(unsigned long timeDelay) {
  unsigned long timeInit = millis();
  while (millis() - timeInit < timeDelay);
}

/*Bucle de configuracion */
void setup() {
  /* Config Serial Port */
  Serial.begin(115200);
  while (!Serial) {}

  /* Cofig Pin In/Out */
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLDOWN);
  pinMode(GATE_1, INPUT);
  pinMode(GATE_2, INPUT);
  pinMode(DC_MOTOR, OUTPUT);

  /* Config LCD I2C */
  lcd.init();
  lcd.backlight();
  home();

  /* Config EEPROM memory */
  EEPROM.begin(512);
  EEPROM.get(passwordAdress, password);

  /* Check for a previously saved password */
  if (password != currentPassword && password > 0) {
    currentPassword = password;
  }
  Serial.println(currentPassword);
}

/* Bucle principal */
void loop() {
  key = keypad.getKey();  /* Obtener la tecla presionada */
  bool gateFlag = true;

  /* Si no se presiona una tecla en 30s la ESP entra en modo Deep sleep */
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

  /* Funcionamiento de los estados del proceso */ 
  switch (state) {
    case S_HOME:
      {
        /* Ir al siguiente estado */
        if (key == 'A') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_LOGIN;
          login();
        }

        /* Enviar el MCU al modo Deep Sleep */
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
        /* Volver al estado anterior */
        if (key == 'B') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_HOME;
          home();
        }

        /* Enviar el MCU al modo Deep Sleep */
        if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }

        /* Comprobar si se presiona una tecla valida para el ingreso del PIN */
        if ((key != NO_KEY) && (isDigit(key) || (key == '*'))) 
        {
          /* Borrar el ultimo digito ingresado */
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

        /* El pin tiene 4 digitos */
        if (indice == 4) 
        {
          password = keyWord.toInt();  

          /* Comprobar si la contraseña es correcta */   
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

          //Limpiar para ingresar un nuevo PIN
          indice = 0;
          keyWord = "";
        }
      }  // FIN state LOGIN
      break;
    case S_ACTION:
      {
        /* Sino se ejecuta una accion en < lateAction se bloquea */
        lastActionExecuted = lastKeyPressed;
        if ((millis() - lastActionExecuted) > lateAction) 
        {
          state = S_LOGIN;
          gateFlag = false;
          login();
        }

        /* Ir al siguiente estado */
        if (key == 'A') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_NEW_PASSWORD;
          newPassword();
          gateFlag = false;
          lastActionExecuted = millis();
        }

        /* Volver al estado anterior */
        if (key == 'B') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          state = S_LOGIN;
          login();
          gateFlag = false;
          lastActionExecuted = millis();
        }
        
        /* Control on/off del motor DC */
        if (key == 'C') 
        {
          stateMotor = !stateMotor;
          if (!stateMotor) 
          {
            digitalWrite(DC_MOTOR, LOW);
            lcd.setCursor(6, 1);
            lcd.print("OFF");
          } 
          else 
          {
            digitalWrite(DC_MOTOR, HIGH);
            lcd.setCursor(6, 1);
            lcd.print("ON ");
          }
          lastActionExecuted = millis();
        }

        /* Enviar el MCU al modo Deep Sleep */
        if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }

        /* Comprobar si se cambio de estado en los bloques anteriores */
        if (gateFlag) 
        {
          /* Verifica el estado del primer sensor de final de carrera */
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

          /* Verifica el estado del segundo sensor de final de carrera */
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
        /* Enviar el MCU al modo Deep Sleep */
        if (key == 'D') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          deepSleep();
        }

        /* No guardar la contraseña */
        if (key == 'B') 
        {
          digitalWrite(LED_PIN, HIGH);
          delayReplacement(100);
          indice = 0;
          keyWord = "";
          state = S_LOGIN;
          login();
        }

        /* Leer y guardar el nuevo PIN */
        if ((key != NO_KEY) && (isDigit(key) || (key == '*'))) 
        {
          /* Borrar el ultimo caracter ingresado */
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
        
        /* Tamño del pin es correcto */
        if (indice == 4) 
        {
          password = keyWord.toInt();

          /* Guardar el nuevo pin */
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
      }  //FIN state new password
      break;
    default:
    {
      Serial.println("State not defined");
    }
    break;
  }  //Switch key FIN
}