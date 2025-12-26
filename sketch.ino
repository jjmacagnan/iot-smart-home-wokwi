#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ===== CONFIGURAÇÕES WiFi =====
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ===== CONFIGURAÇÕES Firebase =====
const char* FIREBASE_HOST = "https://seu-projeto.firebaseio.com";
const char* DEVICE_ID = "device_001";

// ===== PINOS =====
#define DHT_PIN 15        // Sensor DHT22
#define LDR_PIN 34        // Sensor LDR (ADC)
#define FAN_PIN 26        // Relé do Ventilador
#define LIGHT_PIN 27      // Relé da Luz
#define LED_STATUS 2      // LED interno (status conexão)

// ===== CONFIGURAÇÕES SENSORES =====
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// ===== VARIÁVEIS GLOBAIS =====
unsigned long lastSensorRead = 0;
unsigned long lastActuatorCheck = 0;
const long SENSOR_INTERVAL = 3000;      // Lê sensores a cada 3s
const long ACTUATOR_INTERVAL = 1000;    // Verifica atuadores a cada 1s

bool wifiConnected = false;
bool firebaseInitialized = false;

// ===== ESTRUTURAS DE DADOS =====
struct SensorData {
  float temperature;
  int humidity;
  int light;
};

struct ActuatorState {
  bool fan;
  bool light;
  String fanMode;
  String lightMode;
};

ActuatorState currentState = {false, false, "manual", "manual"};
SensorData lastSensorData = {22.0, 60, 400};

// ========================================
// FUNÇÕES DE CONEXÃO
// ========================================
void setupWiFi() {
  Serial.println("\n[WiFi] Conectando...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WiFi] ✓ Conectado!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_STATUS, HIGH);
  } else {
    Serial.println("\n[WiFi] ✗ Falha na conexão");
    digitalWrite(LED_STATUS, LOW);
  }
}

// ========================================
// FUNÇÕES FIREBASE
// ========================================
String getFirebasePath(String path) {
  return String(FIREBASE_HOST) + "/" + path + ".json";
}

bool sendToFirebase(String path, String jsonData) {
  if (!wifiConnected) return false;
  
  HTTPClient http;
  String url = getFirebasePath(path);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.PUT(jsonData);
  bool success = (httpCode == 200 || httpCode == 201);
  
  if (success) {
    Serial.println("[Firebase] ✓ Enviado: " + path);
  } else {
    Serial.println("[Firebase] ✗ Erro: " + String(httpCode));
  }
  
  http.end();
  return success;
}

String getFromFirebase(String path) {
  if (!wifiConnected) return "{}";
  
  HTTPClient http;
  String url = getFirebasePath(path);
  
  http.begin(url);
  int httpCode = http.GET();
  
  String payload = "{}";
  if (httpCode == 200) {
    payload = http.getString();
  } else {
    Serial.println("[Firebase] ✗ Erro ao ler: " + String(httpCode));
  }
  
  http.end();
  return payload;
}

void initializeFirebaseDevice() {
  Serial.println("[Firebase] Inicializando dispositivo...");
  
  StaticJsonDocument<1024> doc;
  
  doc["name"] = "Sala de Estar";
  doc["location"] = "Casa";
  doc["status"] = "online";
  doc["lastUpdate"] = millis();
  
  // Sensores iniciais
  JsonObject sensors = doc.createNestedObject("sensors");
  JsonObject temp = sensors.createNestedObject("temperature");
  temp["value"] = 22;
  temp["unit"] = "°C";
  temp["timestamp"] = millis();
  
  JsonObject hum = sensors.createNestedObject("humidity");
  hum["value"] = 60;
  hum["unit"] = "%";
  hum["timestamp"] = millis();
  
  JsonObject light = sensors.createNestedObject("light");
  light["value"] = 400;
  light["unit"] = "lux";
  light["timestamp"] = millis();
  
  // Atuadores iniciais
  JsonObject actuators = doc.createNestedObject("actuators");
  JsonObject fan = actuators.createNestedObject("fan");
  fan["state"] = false;
  fan["mode"] = "manual";
  
  JsonObject lightAct = actuators.createNestedObject("light");
  lightAct["state"] = false;
  lightAct["mode"] = "manual";
  
  // Configurações
  JsonObject settings = doc.createNestedObject("settings");
  settings["tempThreshold"] = 26;
  settings["lightThreshold"] = 300;
  settings["autoMode"] = true;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  bool success = sendToFirebase("devices/" + String(DEVICE_ID), jsonData);
  
  if (success) {
    firebaseInitialized = true;
    Serial.println("[Firebase] ✓ Dispositivo inicializado");
  }
}

// ========================================
// FUNÇÕES DOS SENSORES
// ========================================
SensorData readSensors() {
  SensorData data;
  
  // 1. Lê temperatura e umidade do DHT22
  data.temperature = dht.readTemperature();
  data.humidity = (int)dht.readHumidity();
  
  // 2. Lê luminosidade do LDR (Pino 34)
  int analogValue = analogRead(LDR_PIN);

  // --- CÁLCULO DE CONVERSÃO LUX (LOGARÍTMICO) ---
  // Constantes baseadas nas características do LDR padrão
  const float GAMMA = 0.7;
  const float RL10 = 50; 

  // Converte o valor do ADC (0-4095) para tensão
  float voltage = analogValue / 4095.0 * 3.3;
  
  // Calcula a resistência do LDR em Ohms
  // Considera um divisor de tensão com resistor de 10k
  float resistance = (10000 * voltage) / (3.3 - voltage);
  
  // Calcula o Lux usando a curva logarítmica
  float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));

  // 3. Validação e limites (0.1 a 100.000)
  if (isnan(lux) || isinf(lux)) lux = 0.1;
  if (lux > 100000) lux = 100000;
  if (lux < 0.1) lux = 0.1;

  data.light = (int)lux;
  
  // Valida leituras do DHT
  if (isnan(data.temperature)) {
    data.temperature = 22.0;
    Serial.println("[Sensor] ⚠ Erro ao ler temperatura");
  }
  if (isnan(data.humidity)) {
    data.humidity = 60;
    Serial.println("[Sensor] ⚠ Erro ao ler umidade");
  }
  
  // 4. Logs de Debug
  Serial.println("\n[Sensores] Leituras:");
  Serial.printf("  Temperatura: %.1f°C\n", data.temperature);
  Serial.printf("  Umidade: %d%%\n", data.humidity);
  Serial.printf("  Luminosidade: %d lux (ADC: %d, Voltagem: %.2fV)\n",
                data.light, analogValue, voltage);
  
  return data;
}

void sendSensorsToFirebase(SensorData data) {
  unsigned long timestamp = millis();
  
  StaticJsonDocument<512> doc;
  
  JsonObject temp = doc.createNestedObject("temperature");
  temp["value"] = data.temperature;
  temp["unit"] = "°C";
  temp["timestamp"] = timestamp;
  
  JsonObject hum = doc.createNestedObject("humidity");
  hum["value"] = data.humidity;
  hum["unit"] = "%";
  hum["timestamp"] = timestamp;
  
  JsonObject light = doc.createNestedObject("light");
  light["value"] = data.light;
  light["unit"] = "lux";
  light["timestamp"] = timestamp;
  
  String jsonData;
  serializeJson(doc, jsonData);
  
  sendToFirebase("devices/" + String(DEVICE_ID) + "/sensors", jsonData);
  sendToFirebase("devices/" + String(DEVICE_ID) + "/lastUpdate", String(timestamp));
  
  // Salva última leitura
  lastSensorData = data;
}

// ========================================
// FUNÇÕES DOS ATUADORES
// ========================================
void updateActuators() {
  // Lê configurações e thresholds
  String settingsData = getFromFirebase("devices/" + String(DEVICE_ID) + "/settings");
  StaticJsonDocument<256> settingsDoc;
  deserializeJson(settingsDoc, settingsData);
  
  float tempThreshold = settingsDoc["tempThreshold"] | 26.0;
  int lightThreshold = settingsDoc["lightThreshold"] | 300;
  
  // Lê estado e modo dos atuadores
  String fanData = getFromFirebase("devices/" + String(DEVICE_ID) + "/actuators/fan");
  String lightData = getFromFirebase("devices/" + String(DEVICE_ID) + "/actuators/light");
  
  StaticJsonDocument<128> fanDoc;
  deserializeJson(fanDoc, fanData);
  String fanMode = fanDoc["mode"] | "manual";
  bool fanState = fanDoc["state"] | false;
  
  StaticJsonDocument<128> lightDoc;
  deserializeJson(lightDoc, lightData);
  String lightMode = lightDoc["mode"] | "manual";
  bool lightState = lightDoc["state"] | false;
  
  // ===== VENTILADOR =====
  if (fanMode == "auto") {
    // Modo automático: controla baseado na temperatura
    bool shouldFanBeOn = lastSensorData.temperature > tempThreshold;
    
    if (shouldFanBeOn != fanState) {
      // Atualiza estado no Firebase
      sendToFirebase("devices/" + String(DEVICE_ID) + "/actuators/fan/state", 
                     shouldFanBeOn ? "true" : "false");
      fanState = shouldFanBeOn;
      
      Serial.printf("[AUTO] Ventilador %s (Temp: %.1f°C, Limite: %.1f°C)\n", 
                    shouldFanBeOn ? "LIGADO" : "DESLIGADO",
                    lastSensorData.temperature, tempThreshold);
    }
  }
  
  // ===== ILUMINAÇÃO =====
  if (lightMode == "auto") {
    // Modo automático: controla baseado na luminosidade
    // Quanto MENOS luz (valor baixo), LIGA a iluminação
    bool shouldLightBeOn = lastSensorData.light < lightThreshold;
    
    if (shouldLightBeOn != lightState) {
      // Atualiza estado no Firebase
      sendToFirebase("devices/" + String(DEVICE_ID) + "/actuators/light/state", 
                     shouldLightBeOn ? "true" : "false");
      lightState = shouldLightBeOn;
      
      Serial.printf("[AUTO] Luz %s (Luminosidade: %d lux, Limite: %d lux)\n", 
                    shouldLightBeOn ? "LIGADA" : "DESLIGADA",
                    lastSensorData.light, lightThreshold);
    }
  }
  
  // ===== ATUALIZA HARDWARE (sempre atualiza baseado no estado atual) =====
  if (fanState != currentState.fan) {
    currentState.fan = fanState;
    digitalWrite(FAN_PIN, fanState ? HIGH : LOW);
    Serial.printf("[Hardware] Ventilador → %s\n", fanState ? "LIGADO" : "DESLIGADO");
  }
  
  if (lightState != currentState.light) {
    currentState.light = lightState;
    digitalWrite(LIGHT_PIN, lightState ? HIGH : LOW);
    Serial.printf("[Hardware] Luz → %s\n", lightState ? "LIGADA" : "DESLIGADA");
  }
  
  // Atualiza modos salvos
  if (fanMode != currentState.fanMode) {
    currentState.fanMode = fanMode;
    Serial.printf("[Modo] Ventilador: %s\n", fanMode.c_str());
  }
  
  if (lightMode != currentState.lightMode) {
    currentState.lightMode = lightMode;
    Serial.printf("[Modo] Luz: %s\n", lightMode.c_str());
  }
}

// ========================================
// SETUP E LOOP
// ========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("   ESP32 IoT - Firebase Auto/Manual");
  Serial.println("========================================\n");
  
  // Configuração dos pinos
  pinMode(LED_STATUS, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(LED_STATUS, LOW);
  
  // Inicializa DHT22
  dht.begin();
  
  // Conecta WiFi
  setupWiFi();
  
  // Inicializa Firebase
  if (wifiConnected) {
    delay(1000);
    initializeFirebaseDevice();
  }
  
  Serial.println("\n[Sistema] ✓ Pronto!");
  Serial.println("========================================\n");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Verifica conexão WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("[WiFi] ✗ Conexão perdida");
      wifiConnected = false;
      digitalWrite(LED_STATUS, LOW);
    }
    setupWiFi();
    return;
  }
  
  // Lê e envia sensores periodicamente
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    
    if (firebaseInitialized) {
      SensorData data = readSensors();
      sendSensorsToFirebase(data);
    }
  }
  
  // Verifica e atualiza atuadores periodicamente
  if (currentMillis - lastActuatorCheck >= ACTUATOR_INTERVAL) {
    lastActuatorCheck = currentMillis;
    
    if (firebaseInitialized) {
      updateActuators();
    }
  }
  
  // Pisca LED de status
  static unsigned long lastBlink = 0;
  if (currentMillis - lastBlink >= 1000) {
    lastBlink = currentMillis;
    if (wifiConnected) {
      digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    }
  }
  
  delay(100);
}