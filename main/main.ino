#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <time.h>
#include <ESP32Time.h>
#include "credentials.h"

// Definición de pines
#define RELAY_PIN 18
#define EMERGENCY_BUTTON_PIN 3
#define DISABLE_BUTTON_PIN 2

// Configuración DFPlayer
SoftwareSerial mySoftwareSerial(20, 19); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// Reloj RTC
ESP32Time rtc;

// Servidor web
AsyncWebServer server(80);

// Credenciales WiFi - Debes cambiarlas
const char* ssid = SSID;
const char* password = PASSWORD;

// Credenciales de acceso a la web
const char* http_username = "admin";
const char* http_password = "admin";

// Variables para control del sistema
bool timbreEnabled = true;
int activeSchedule = 0; // 0, 1, o 2 según la configuración activa

// Estructura para los horarios
struct BellTime {
  int hour;
  int minute;
  bool isBreak;
  bool isEndOfDay;
};

// Arreglo para almacenar las tres configuraciones
std::vector<BellTime> schedules[3];
int scheduleCount[3] = {0, 0, 0};
String scheduleNames[3] = {"Normal", "Alternativo 1", "Alternativo 2"};


void setup() {
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
  if(!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
  }
  
  // Cargar configuraciones guardadas
  loadSchedules();
  
  // Conectar a WiFi
  setupWiFi();
  
  // Sincronizar hora con NTP
  configTime(-18000, 0, "pool.ntp.org");
  setLocalTime();
  
  // Configurar servidor web
  setupWebServer();

  Serial.println("Sistema listo!");
  Serial.println("Hora: " + getCurrentTimeString());
  Serial.println("Config activa: " + scheduleNames[activeSchedule]);
}

void loop() {
  // Verificar botones
  checkButtons();
  
  // Verificar si es hora de activar el timbre
  checkSchedule();

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

void checkSchedule() {
  if (!timbreEnabled) return;
  
  int currentHour = rtc.getHour(true);
  int currentMinute = rtc.getMinute();
  int currentSecond = rtc.getSecond();
  
  // Solo revisar cuando los segundos sean 0 para evitar activaciones múltiples
  if (currentSecond != 0) return;
  
  for (int i = 0; i < scheduleCount[activeSchedule]; i++) {
    if (schedules[activeSchedule][i].hour == currentHour && 
        schedules[activeSchedule][i].minute == currentMinute) {
      ringBell(schedules[activeSchedule][i]);
      break;
    }
  }
}

void ringBell(BellTime bellTime) {
  Serial.println("¡Timbre activado!");
  
  // Seleccionar canción aleatoria entre 1-10 (asumiendo que hay 10 mp3 en la tarjeta SD)
  int randomSong = random(1, 5);
  myDFPlayer.play(randomSong);
  
  // Activar relé para el timbre
  digitalWrite(RELAY_PIN, LOW);
  
  // El timbre suena dos veces para recreo o fin de jornada
  if (bellTime.isBreak) {
    delay(1000);
    digitalWrite(RELAY_PIN, LOW);
    delay(500);
    digitalWrite(RELAY_PIN, HIGH);
  } else if (bellTime.isEndOfDay) {
    delay(1000);
    digitalWrite(RELAY_PIN, HIGH);
    delay(500);
    digitalWrite(RELAY_PIN, LOW);
  }
  
  // Apagar el relé después de 2 segundos
  delay(2000);
  digitalWrite(RELAY_PIN, HIGH);
}

void checkButtons() {
  // Botón de emergencia
  if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
      BellTime emergencyBell = {0, 0, false, false};
      ringBell(emergencyBell);
      while(digitalRead(EMERGENCY_BUTTON_PIN) == LOW); // Esperar hasta que se suelte el botón
    }
  }
  
  // Botón de desactivar
  if (digitalRead(DISABLE_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(DISABLE_BUTTON_PIN) == LOW) {
      timbreEnabled = !timbreEnabled;
      // updateDisplay();
      while(digitalRead(DISABLE_BUTTON_PIN) == LOW); // Esperar hasta que se suelte el botón
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
          bell.isBreak = doc["schedules"][s][i]["isBreak"];
          bell.isEndOfDay = doc["schedules"][s][i]["isEndOfDay"];
          schedules[s].push_back(bell);
        }
      }
      
      configFile.close();
    }
  } else {
    // Configuración por defecto (ejemplo)
    scheduleNames[0] = "Jornada Normal";
    scheduleCount[0] = 9;
    
    // Ejemplo de horario por defecto (se debe configurar mediante la web)
    schedules[0].push_back({7, 0, false, false});   // 7:00 AM
    schedules[0].push_back({8, 0, false, false});   // 8:00 AM
    schedules[0].push_back({9, 0, false, false});   // 9:00 AM
    schedules[0].push_back({10, 0, false, false});   // 10:00 AM
    schedules[0].push_back({10, 30, false, false});  // 10:30 AM
    schedules[0].push_back({11, 0, false, false});  // 11:00 AM
    schedules[0].push_back({12, 0, false, false});  // 12:00 PM
    schedules[0].push_back({13, 0, false, false});  // 1:00 PM
    schedules[0].push_back({14, 0, false, true});   // 2:00 PM - Fin de jornada
    
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
      doc["schedules"][s][i]["isBreak"] = schedules[s][i].isBreak;
      doc["schedules"][s][i]["isEndOfDay"] = schedules[s][i].isEndOfDay;
    }
  }
  
  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
  }
}

String processor(const String& var) {
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
        doc["schedules"][s][i]["isBreak"] = schedules[s][i].isBreak;
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
    
    BellTime emergencyBell = {0, 0, false, false};
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
      newBell.isBreak = bell["isBreak"];
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
    rtc.setTime(0, minute, hour, 1, 1, 2023);  // segundos, minutos, horas, día, mes, año
    
    request->send(200, "text/plain", "Hora actualizada");
  });
  
  // Iniciar servidor
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}