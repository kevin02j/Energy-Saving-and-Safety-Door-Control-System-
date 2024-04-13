#include <esp_sleep.h>
#include <WiFi.h>
#include <Arduino.h>

const int LED_PIN = 2;       // GPIO del LED integrado
const int BUTTON = 15;       // GPIO del boton para despertar al MCU

// RTC_DATA_ATTR int contador = 0;
bool dormir = false;  // Variable para indicar si debe dormir o no

// Función para enviar el ESP32 a dormir
void enviarADormir() {
  Serial.println("Enviando a dormir...");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1);
  esp_deep_sleep_start();
  btStop();
  WiFi.mode(WIFI_OFF);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}              // Esperar a que inicie el puerto serie
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLDOWN);
}

void loop() {
  for (int i = 0; i <= 10; i++) {
    if (i == 5) {
      enviarADormir();
      digitalWrite(LED_PIN, LOW);
    }
    // Verificar si la ESP32 está despertando del modo de deep sleep
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_DEEPSLEEP) {
      Serial.println("La ESP32 está despertando del modo de deep sleep");
      // Llamar a la función que deseas ejecutar después del despertar
    }
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
  }
}
