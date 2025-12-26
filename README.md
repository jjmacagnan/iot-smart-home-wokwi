# ESP32 IoT - Smart Home (Wokwi)

**Descrição** ✅

Projeto de automação residencial com ESP32 que lê temperatura, umidade (DHT22) e luminosidade (LDR) e controla ventilador e iluminação via Firebase Realtime Database. Oferece modos **manual** e **auto** e inclui simulação Wokwi para testes rápidos.

---

## 🔧 Funcionalidades

- Leitura periódica de sensores: DHT22 (temp/umidade) e LDR (lux)
- Envio das leituras para Firebase Realtime Database
- Controle de atuadores (ventilador, luz) em modo manual ou automático
- Inicialização de estado e configurações no Firebase
- Simulação disponível no Wokwi

---

## 📋 Requisitos

Hardware
- ESP32 (ou similar)
- Sensor DHT22
- LDR + resistor para divisor de tensão
- Relés/transistores para controlar ventilador e luz (ou LEDs para testes)

Software / Bibliotecas
- Arduino IDE ou PlatformIO (ESP32 board package)
- Bibliotecas (instalar via Library Manager):
  - `DHT sensor library for ESPx` (ou `DHT sensor library`)
  - `ArduinoJson` (versão 6.x)

---

## 🧭 Mapeamento de pinos (padrão no `sketch.ino`)

- `DHT_PIN` → 15
- `LDR_PIN` → 34 (ADC)
- `FAN_PIN` → 26
- `LIGHT_PIN` → 27
- `LED_STATUS` → 2 (LED interno)

Ajuste conforme seu hardware.

---

## ⚙️ Configuração

No arquivo `sketch.ino`, edite as constantes no topo:

```cpp
const char* WIFI_SSID = "Seu_SSID";
const char* WIFI_PASSWORD = "Sua_Senha";
const char* FIREBASE_HOST = "https://SEU_PROJETO.firebaseio.com"; // URL do Realtime Database
const char* DEVICE_ID = "device_001"; // Identificador do dispositivo
```

- Use o URL do Realtime Database (ex.: `https://meu-projeto.firebaseio.com`).
- Garanta regras de leitura/escrita apropriadas ou token de autenticação conforme seu caso.

---

## 🔁 Estrutura esperada no Firebase

```json
{
  "devices": {
    "device_001": {
      "name": "Sala de Estar",
      "location": "Casa",
      "status": "offline",
      "lastUpdate": 0,
      "sensors": {
        "temperature": {
          "value": 22,
          "unit": "°C",
          "timestamp": 0
        },
        "humidity": {
          "value": 60,
          "unit": "%",
          "timestamp": 0
        },
        "light": {
          "value": 400,
          "unit": "lux",
          "timestamp": 0
        }
      },
      "actuators": {
        "fan": {
          "state": false,
          "mode": "manual"
        },
        "light": {
          "state": false,
          "mode": "manual"
        }
      },
      "settings": {
        "tempThreshold": 26,
        "lightThreshold": 300,
        "autoMode": true
      }
    }
  }
}
```

---

## ▶️ Como executar / testar

1. Instale as bibliotecas indicadas.
2. Abra `sketch.ino` no Arduino IDE (ou PlatformIO) e ajuste WiFi/Firebase.
3. Faça upload para o ESP32.
4. Abra o Serial Monitor a 115200 baud para ver logs.
5. Alternativamente, simule no Wokwi: https://wokwi.com/projects/451278796859126785

---

## 🩺 Dicas e resolução de problemas

- Se DHT falhar, verifique conexões e alimentação; use `dht.begin()` e logs no Serial.
- Leitura do LDR depende de divisor e do ADC do ESP32; ajuste `LDR_PIN` se necessário.
- Verifique se `FIREBASE_HOST` está correto e se as regras permitem leitura/escrita.