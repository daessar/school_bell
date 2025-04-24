#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <time.h>
#include <ESP32Time.h>
#include <esp_task_wdt.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Wire.h"
#include "credentials.h"

// Definición de pines
#define RELAY_PIN 18
#define EMERGENCY_BUTTON_PIN 3
#define DISABLE_BUTTON_PIN 2

// Configuración DFPlayer
SoftwareSerial mySoftwareSerial(19, 20);  // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// Sync time co
const char* ntpServer = "co.pool.ntp.org";   //Servidor NTP para sincronizar el Time
const long  gmtOffset_sec = -18000;         // Establecer el GMT (3600 segundos = 1 hora) paa Colombia es GMT -5) es decir  -18000
const int   daylightOffset_sec = 0;

// Reloj RTC
ESP32Time rtc;

// Servidor web
AsyncWebServer server(80);

// Credenciales WiFi - Debes cambiarlas
const char *ssid = SSID;
const char *password = PASSWORD;

// Credenciales de acceso a la web
const char *http_username = HTTP_USERNAME;
const char *http_password = HTTP_PASSWORD;

// Variables para control del sistema
bool timbreEnabled = true;
int activeSchedule = 0;  // 0, 1, o 2 según la configuración activa

// Estructura para los horarios
struct BellTime {
  int hour;
  int minute;
  bool isEndOfDay;
};

//Tiempo encendido timbre
int bell_time_duration = 6000;  //Seis segundos
int mp3_time_duration = 60000;  //Un minuto

//Pantalla oled 128x32
#define I2C_SDA 1
#define I2C_SCL 0

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Dirección I2C del display
#define OLED_ADDR 0x3C

// Crear objeto display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Arreglo para almacenar las tres configuraciones
std::vector<BellTime> schedules[3];
int scheduleCount[3] = { 0, 0, 0 };
String scheduleNames[3] = { "Horario 1", "Horario 2", "Horario 3" };

// Colombia holidays 2025
const byte holiday_count = 18;
const uint32_t holidays[holiday_count] = {
  20250101,  // Año Nuevo
  20250106,  // Reyes Magos
  20250324,  // San José
  20250417,  // Jueves Santo
  20250418,  // Viernes Santo
  20250501,  // Día del Trabajo
  20250512,  // Ascensión del Señor
  20250602,  // Corpus Christi
  20250613,  // Sagrado Corazón
  20250630,  // San Pedro y San Pablo
  20250720,  // Independencia de Colombia
  20250807,  // Batalla de Boyacá
  20250818,  // Asunción de la Virgen
  20251013,  // Día de la Raza
  20251103,  // Todos los Santos
  20251117,  // Independencia de Cartagena
  20251208,  // Inmaculada Concepción
  20251225   // Navidad
};


void setup() {
  esp_task_wdt_config_t config = {
    .timeout_ms = 120* 1000,  //  120 seconds
    .trigger_panic = true,     // Trigger panic if watchdog timer is not reset
  };
  esp_task_wdt_reconfigure(&config);
  Serial.begin(115200);

  // Inicializar pines
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(EMERGENCY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DISABLE_BUTTON_PIN, INPUT_PULLUP);

  // Inicializar DFPlayer
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Error al inicializar DFPlayer"));
  }
  myDFPlayer.setTimeOut(500);
  myDFPlayer.volume(30);  // Volumen (0-30)

  // Inicializar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
  }

  // Cargar configuraciones guardadas
  loadSchedules();

  // Pantalla oled
  Wire.begin(I2C_SDA, I2C_SCL);
  // Inicializar la pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Error al inicializar SSD1306"));
    for(;;); // No continuar si hay error
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Iniciando..."));
  display.display();

  delay(1000);

  // Conectar a WiFi
  setupWiFi();

  // Sincronizar hora con NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setLocalTime();

  // Configurar servidor web
  setupWebServer();

  Serial.println("Sistema listo!");
  Serial.println("Hora: " + getCurrentTimeString());
  Serial.println("Config activa: " + scheduleNames[activeSchedule]);
}

void loop() {
  static unsigned long lastDisplayUpdate = 0;
  // Verificar botones
  checkButtons();

  // Verificar si es hora de activar el timbre
  checkSchedule();

  // Actualizar pantalla cada 5 segundos
  if (millis() - lastDisplayUpdate > 5000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Mostrar IP
  display.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP().toString());
  } else {
    display.println("AP: TimbreEscolar");
  }
  
  // Mostrar horario activo
  display.setCursor(0, 11);
  display.println(scheduleNames[activeSchedule]);

   // Mostrar hora actual
  display.setCursor(0, 22);
  display.print("Hora: ");
  display.println(getCurrentTimeString());
  
  display.display();
}

void setupWiFi() {

  Serial.println("Conectando WiFi...");

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Conectado!");
    Serial.println(WiFi.localIP().toString());

  } else {

    Serial.println("Error WiFi!");
    Serial.println("Modo AP activado");

    // Configurar Access Point si no se puede conectar
    WiFi.softAP("TimbreEscolar", "password");
  }
}

void setLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error obteniendo hora");
    return;
  }

  rtc.setTimeStruct(timeinfo);
}

String getCurrentTimeString() {
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", rtc.getHour(true), rtc.getMinute(), rtc.getSecond());
  return String(timeStr);
}

bool weekend(int day) {
  // day: 0 = domingo, 6 = sábado
  return (day == 0 || day == 6);
}

bool holiday() {
  uint32_t fechaNum = (rtc.getYear() * 10000 + (rtc.getMonth() + 1) * 100 + rtc.getDay());

  for (int i = 0; i < holiday_count; i++) {
    if (holidays[i] == fechaNum) {
      Serial.println("¡Es festivo! (" + String(fechaNum) + ") Timbre Off");
      return true;
    }
  }
  return false;
}

void checkSchedule() {
  if (holiday()) {
    delay(bell_time_duration);
    timbreEnabled = !timbreEnabled;
  }
  if (weekend(rtc.getDayofWeek())) {
    Serial.println("Es fin de semana Timbre Off");
    delay(bell_time_duration);
    timbreEnabled = !timbreEnabled;
  }
  if (!timbreEnabled) return;

  int currentHour = rtc.getHour(true);
  int currentMinute = rtc.getMinute();
  int currentSecond = rtc.getSecond();

  // Solo revisar cuando los segundos sean 0 para evitar activaciones múltiples
  if (currentSecond != 0) return;

  for (int i = 0; i < scheduleCount[activeSchedule]; i++) {
    if (schedules[activeSchedule][i].hour == currentHour && schedules[activeSchedule][i].minute == currentMinute) {
      ringBell(schedules[activeSchedule][i]);
      break;
    }
  }
}

void ringBell(BellTime bellTime) {
  // Seleccionar canción aleatoria entre 1-15 (asumiendo que hay 10 mp3 en la tarjeta SD)
  int randomSong = random(1, 15);
  Serial.println("¡Timbre Sonando!");

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TIMBRE");
  display.setCursor(0, 16);
  display.println("SONANDO!");
  display.display();

  // Activar relé para el timbre
  digitalWrite(RELAY_PIN, LOW);

  //Reproducir mp3
  myDFPlayer.play(randomSong);

  // El timbre suena dos veces para fin de jornada
  if (bellTime.isEndOfDay) {
    delay(bell_time_duration);
    digitalWrite(RELAY_PIN, HIGH);
    delay(500);
    digitalWrite(RELAY_PIN, LOW);
  }

  // Apagar el relé después de 2 segundos
  delay(bell_time_duration);
  digitalWrite(RELAY_PIN, HIGH);
  delay(mp3_time_duration - bell_time_duration);
  myDFPlayer.stop();

  updateDisplay();
}

void checkButtons() {
  // Botón de emergencia
  if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
      BellTime emergencyBell = { 0, 0, false };
      ringBell(emergencyBell);
      while (digitalRead(EMERGENCY_BUTTON_PIN) == LOW)
        ;  // Esperar hasta que se suelte el botón
    }
  }

  // Botón de desactivar
  if (digitalRead(DISABLE_BUTTON_PIN) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(DISABLE_BUTTON_PIN) == LOW) {
      timbreEnabled = !timbreEnabled;
      updateDisplay();
      while (digitalRead(DISABLE_BUTTON_PIN) == LOW)
        ;  // Esperar hasta que se suelte el botón
    }
  }
}


void loadSchedules() {
  if (SPIFFS.exists("/config.json")) {
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);

      DynamicJsonDocument doc(2048);
      deserializeJson(doc, buf.get());

      activeSchedule = doc["activeSchedule"];

      for (int s = 0; s < 3; s++) {
        scheduleNames[s] = doc["scheduleNames"][s].as<String>();
        scheduleCount[s] = doc["schedules"][s].size();

        schedules[s].clear();
        for (int i = 0; i < scheduleCount[s]; i++) {
          BellTime bell;
          bell.hour = doc["schedules"][s][i]["hour"];
          bell.minute = doc["schedules"][s][i]["minute"];
          bell.isEndOfDay = doc["schedules"][s][i]["isEndOfDay"];
          schedules[s].push_back(bell);
        }
      }

      configFile.close();
    }
  } else {
    // Configuración por defecto (ejemplo)
    scheduleNames[0] = "Horario 1";
    scheduleCount[0] = 9;

    // Ejemplo de horario por defecto (se debe configurar mediante la web)
    schedules[0].push_back({ 7, 0, false });    // 7:00 AM
    schedules[0].push_back({ 8, 0, false });    // 8:00 AM
    schedules[0].push_back({ 9, 0, false });    // 9:00 AM
    schedules[0].push_back({ 10, 0, false });   // 10:00 AM
    schedules[0].push_back({ 10, 30, false });  // 10:30 AM
    schedules[0].push_back({ 11, 0, false });   // 11:00 AM
    schedules[0].push_back({ 12, 0, false });   // 12:00 PM
    schedules[0].push_back({ 13, 0, false });   // 1:00 PM
    schedules[0].push_back({ 14, 0, true });    // 2:00 PM - Fin de jornada

    saveSchedules();
  }
}

void saveSchedules() {
  DynamicJsonDocument doc(2048);

  doc["activeSchedule"] = activeSchedule;

  for (int s = 0; s < 3; s++) {
    doc["scheduleNames"][s] = scheduleNames[s];

    for (int i = 0; i < scheduleCount[s]; i++) {
      doc["schedules"][s][i]["hour"] = schedules[s][i].hour;
      doc["schedules"][s][i]["minute"] = schedules[s][i].minute;
      doc["schedules"][s][i]["isEndOfDay"] = schedules[s][i].isEndOfDay;
    }
  }

  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
  }
}

String processor(const String &var) {
  if (var == "ACTIVE_SCHEDULE") {
    return String(activeSchedule);
  }
  if (var == "SCHEDULE1_NAME") {
    return scheduleNames[0];
  }
  if (var == "SCHEDULE2_NAME") {
    return scheduleNames[1];
  }
  if (var == "SCHEDULE3_NAME") {
    return scheduleNames[2];
  }
  if (var == "TIMBRE_STATUS") {
    return timbreEnabled ? "Activado" : "Desactivado";
  }
  return String();
}

void setupWebServer() {
  // Ruta para la página principal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Ruta para archivos estáticos (CSS, JS)
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/script.js", "application/javascript");
  });

  // API para obtener datos actuales
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    DynamicJsonDocument doc(2048);
    doc["activeSchedule"] = activeSchedule;
    doc["timbreEnabled"] = timbreEnabled;
    doc["currentTime"] = getCurrentTimeString();

    for (int s = 0; s < 3; s++) {
      doc["scheduleNames"][s] = scheduleNames[s];

      for (int i = 0; i < scheduleCount[s]; i++) {
        doc["schedules"][s][i]["hour"] = schedules[s][i].hour;
        doc["schedules"][s][i]["minute"] = schedules[s][i].minute;
        doc["schedules"][s][i]["isEndOfDay"] = schedules[s][i].isEndOfDay;
      }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // API para cambiar configuración activa
  server.on("/api/setActive", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    if (request->hasParam("schedule", true)) {
      int newSchedule = request->getParam("schedule", true)->value().toInt();
      if (newSchedule >= 0 && newSchedule < 3) {
        activeSchedule = newSchedule;
        saveSchedules();
        request->send(200, "text/plain", "Configuración cambiada");
      } else {
        request->send(400, "text/plain", "Valor de configuración inválido");
      }
    } else {
      request->send(400, "text/plain", "Falta parámetro schedule");
    }
  });

  // API para activar/desactivar timbre
  server.on("/api/toggleTimbre", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    timbreEnabled = !timbreEnabled;
    request->send(200, "text/plain", timbreEnabled ? "Timbre activado" : "Timbre desactivado");
  });

  // API para activar timbre manualmente
  server.on("/api/ringNow", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    BellTime emergencyBell = { 0, 0, false };
    ringBell(emergencyBell);
    request->send(200, "text/plain", "Timbre activado manualmente");
  });

  // API para actualizar configuraciones
  server.on("/api/updateSchedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    // Verificar parámetros requeridos
    if (!request->hasParam("data", true)) {
      request->send(400, "text/plain", "Faltan datos de configuración");
      return;
    }

    String jsonData = request->getParam("data", true)->value();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error) {
      request->send(400, "text/plain", "Error en formato JSON");
      return;
    }

    int scheduleIndex = doc["scheduleIndex"];
    if (scheduleIndex < 0 || scheduleIndex > 2) {
      request->send(400, "text/plain", "Índice de configuración inválido");
      return;
    }

    // Actualizar nombre de la configuración
    scheduleNames[scheduleIndex] = doc["name"].as<String>();

    // Actualizar horarios
    JsonArray bells = doc["bells"];
    scheduleCount[scheduleIndex] = bells.size();
    schedules[scheduleIndex].clear();

    for (JsonObject bell : bells) {
      BellTime newBell;
      newBell.hour = bell["hour"];
      newBell.minute = bell["minute"];
      newBell.isEndOfDay = bell["isEndOfDay"];
      schedules[scheduleIndex].push_back(newBell);
    }

    saveSchedules();
    request->send(200, "text/plain", "Configuración actualizada");
  });

  // API para configurar fecha y hora
  server.on("/api/setTime", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    if (!request->hasParam("hour", true) || !request->hasParam("minute", true)) {
      request->send(400, "text/plain", "Faltan parámetros de hora");
      return;
    }

    int hour = request->getParam("hour", true)->value().toInt();
    int minute = request->getParam("minute", true)->value().toInt();

    // Ajustar el RTC interno
    rtc.setTime(0, minute, hour, 1, 1, 2025);  // segundos, minutos, horas, día, mes, año

    request->send(200, "text/plain", "Hora actualizada");
  });

  // Iniciar servidor
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}