#include <WiFi.h>
#include <WebServer.h>

// ========== CONFIGURACIÓN DE RED ==========
const char* ssid = "TU_SSID";           // Cambiar con tu red WiFi
const char* password = "TU_PASSWORD";   // Cambiar con tu contraseña

// ========== CONFIGURACIÓN DE PINES ==========
const int DUST_SENSOR_PIN = 35;  // GPIO 35 para la señal analógica del sensor
const int LED_PIN = 27;           // GPIO 27 para el LED digital

// ========== CONFIGURACIÓN DEL SENSOR ==========
const int SAMPLE_TIME = 280;      // Tiempo de lectura en ms
const int WAIT_TIME = 40;         // Tiempo de espera en ms
const float VOLTAGE_REF = 3.3;    // Voltaje de referencia del ADC (3.3V para ESP32)
const int ADC_MAX = 4095;         // Resolución máxima del ADC (12 bits)

// ========== VARIABLES GLOBALES ==========
WebServer server(80);
float dustDensity = 0.0;
float voltage = 0.0;
int rawADC = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastDebugTime = 0;
const unsigned long DISPLAY_INTERVAL = 10000; // Mostrar IP cada 10 segundos
const unsigned long DEBUG_INTERVAL = 2000;    // Debug cada 2 segundos

// ========== FUNCIÓN PARA LEER EL SENSOR ==========
void readDustSensor() {
  Serial.println("\n--- Iniciando lectura del sensor ---");
  
  // Encender LED
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ENCENDIDO");
  delay(10);
  
  // Esperar a que se estabilice
  delay(WAIT_TIME);
  Serial.print("Esperado: ");
  Serial.print(WAIT_TIME);
  Serial.println(" ms");
  
  // Leer múltiples muestras y promediar
  long sumValue = 0;
  int numSamples = 100;
  
  Serial.print("Leyendo ");
  Serial.print(numSamples);
  Serial.println(" muestras del ADC...");
  
  for (int i = 0; i < numSamples; i++) {
    int lectura = analogRead(DUST_SENSOR_PIN);
    sumValue += lectura;
    
    // Mostrar cada 10 muestras
    if (i % 10 == 0) {
      Serial.print("Muestra ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(lectura);
    }
    delay(1);
  }
  
  rawADC = sumValue / numSamples;
  
  Serial.print("Valor ADC promedio: ");
  Serial.println(rawADC);
  
  // Apagar LED
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED APAGADO");
  
  // Convertir lectura ADC a voltaje
  voltage = (rawADC / (float)ADC_MAX) * VOLTAGE_REF;
  
  Serial.print("Voltaje calculado: ");
  Serial.print(voltage);
  Serial.println(" V");
  
  // Convertir voltaje a densidad de polvo (µg/m³)
  // Fórmula del fabricante: densidad = (voltage - Vref) / 0.005
  // Vref típicamente es 0.8V
  float Vref = 0.8;
  dustDensity = (voltage - Vref) / 0.005;
  
  Serial.print("Vref: ");
  Serial.print(Vref);
  Serial.println(" V");
  Serial.print("Densidad de polvo: ");
  Serial.print(dustDensity);
  Serial.println(" µg/m³");
  
  // Asegurar que no sea negativo
  if (dustDensity < 0) {
    Serial.println("Valor negativo detectado, ajustando a 0");
    dustDensity = 0;
  }
  
  Serial.println("--- Lectura completada ---\n");
}

// ========== PÁGINA WEB PRINCIPAL ==========
void handleRoot() {
  String html = "<!DOCTYPE html>"
    "<html lang=\"es\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>Monitor de Polvo - GP2Y1010</title>"
    "<style>"
    "* { margin: 0; padding: 0; box-sizing: border-box; }"
    "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; justify-content: center; align-items: center; padding: 20px; }"
    ".container { background: white; border-radius: 15px; box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3); padding: 40px; max-width: 500px; width: 100%; }"
    "h1 { text-align: center; color: #333; margin-bottom: 10px; font-size: 28px; }"
    ".subtitle { text-align: center; color: #666; margin-bottom: 30px; font-size: 14px; }"
    ".sensor-card { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border-radius: 10px; padding: 30px; color: white; margin-bottom: 20px; text-align: center; }"
    ".metric { margin-bottom: 20px; }"
    ".metric-label { font-size: 14px; opacity: 0.9; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 1px; }"
    ".metric-value { font-size: 48px; font-weight: bold; font-family: 'Courier New', monospace; }"
    ".metric-unit { font-size: 18px; opacity: 0.9; margin-left: 8px; }"
    ".debug-box { background: #f5f5f5; border-left: 4px solid #ff9800; padding: 15px; margin-bottom: 20px; border-radius: 5px; font-family: monospace; font-size: 12px; }"
    ".debug-label { font-weight: bold; color: #333; }"
    ".debug-value { color: #666; }"
    ".info-box { background: #f5f5f5; border-left: 4px solid #667eea; padding: 15px; margin-bottom: 20px; border-radius: 5px; }"
    ".info-box-title { font-weight: bold; color: #333; margin-bottom: 5px; }"
    ".info-box-content { color: #666; font-size: 14px; line-height: 1.6; }"
    ".status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 8px; animation: pulse 2s infinite; }"
    ".status-good { background-color: #4caf50; }"
    ".status-moderate { background-color: #ff9800; }"
    ".status-poor { background-color: #f44336; }"
    "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }"
    ".update-btn { width: 100%; padding: 12px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; transition: transform 0.2s; }"
    ".update-btn:hover { transform: translateY(-2px); }"
    ".update-btn:active { transform: translateY(0); }"
    ".last-update { text-align: center; color: #999; font-size: 12px; margin-top: 15px; }"
    ".quality-bar { width: 100%; height: 8px; background: #ddd; border-radius: 10px; margin-top: 15px; overflow: hidden; }"
    ".quality-bar-fill { height: 100%; background: linear-gradient(90deg, #4caf50, #ff9800, #f44336); width: 0%; transition: width 0.5s ease; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1>Monitor de Polvo</h1>"
    "<p class=\"subtitle\">Sensor GP2Y1010 en ESP32</p>"
    "<div class=\"sensor-card\">"
    "<div class=\"metric\">"
    "<div class=\"metric-label\">Densidad de Polvo</div>"
    "<div class=\"metric-value\" id=\"dustValue\">--</div>"
    "<div class=\"metric-unit\">µg/m³</div>"
    "</div>"
    "<div class=\"metric\">"
    "<div class=\"metric-label\">Voltaje del Sensor</div>"
    "<div style=\"font-size: 28px; font-weight: bold;\">"
    "<span id=\"voltageValue\">--</span>"
    "<span class=\"metric-unit\" style=\"font-size: 14px;\">V</span>"
    "</div>"
    "</div>"
    "</div>"
    "<div class=\"debug-box\">"
    "<div class=\"debug-label\">DEBUG - Valor ADC crudo:</div>"
    "<div class=\"debug-value\" id=\"adcValue\">0</div>"
    "<div class=\"debug-label\" style=\"margin-top: 10px;\">Estado del LED:</div>"
    "<div class=\"debug-value\" id=\"ledStatus\">Verificando...</div>"
    "</div>"
    "<div id=\"qualityStatus\" class=\"info-box\">"
    "<div class=\"info-box-title\">"
    "<span class=\"status-indicator status-good\" id=\"statusIndicator\"></span>"
    "Calidad del Aire"
    "</div>"
    "<div class=\"info-box-content\" id=\"qualityText\">Cargando datos...</div>"
    "<div class=\"quality-bar\">"
    "<div class=\"quality-bar-fill\" id=\"qualityBar\"></div>"
    "</div>"
    "</div>"
    "<button class=\"update-btn\" onclick=\"updateData()\">Actualizar Ahora</button>"
    "<div class=\"last-update\" id=\"lastUpdate\">Última actualización: --</div>"
    "</div>"
    "<script>"
    "let updateInterval;"
    "function updateData() {"
    "fetch('/api/sensor')"
    ".then(response => response.json())"
    ".then(data => {"
    "document.getElementById('dustValue').textContent = data.dust.toFixed(2);"
    "document.getElementById('voltageValue').textContent = data.voltage.toFixed(2);"
    "document.getElementById('adcValue').textContent = data.rawADC;"
    "document.getElementById('ledStatus').textContent = data.ledStatus;"
    "const now = new Date();"
    "document.getElementById('lastUpdate').textContent = 'Última actualización: ' + now.toLocaleTimeString('es-ES');"
    "updateQualityStatus(data.dust);"
    "})"
    ".catch(error => console.error('Error:', error));"
    "}"
    "function updateQualityStatus(dustValue) {"
    "let status, text, percentage, indicatorClass;"
    "if (dustValue < 50) {"
    "status = 'Excelente';"
    "text = 'La calidad del aire es excelente';"
    "percentage = (dustValue / 50) * 25;"
    "indicatorClass = 'status-good';"
    "} else if (dustValue < 100) {"
    "status = 'Bueno';"
    "text = 'La calidad del aire es buena';"
    "percentage = 25 + ((dustValue - 50) / 50) * 25;"
    "indicatorClass = 'status-good';"
    "} else if (dustValue < 150) {"
    "status = 'Moderado';"
    "text = 'La calidad del aire es moderada. Se recomienda precaución.';"
    "percentage = 50 + ((dustValue - 100) / 50) * 25;"
    "indicatorClass = 'status-moderate';"
    "} else {"
    "status = 'Pobre';"
    "text = 'La calidad del aire es pobre. Se recomienda usar mascarilla.';"
    "percentage = 75 + ((dustValue - 150) / 150) * 25;"
    "indicatorClass = 'status-poor';"
    "}"
    "const qualityText = document.getElementById('qualityText');"
    "qualityText.innerHTML = '<strong>' + status + '</strong><br>' + text;"
    "const indicator = document.getElementById('statusIndicator');"
    "indicator.className = 'status-indicator ' + indicatorClass;"
    "const bar = document.getElementById('qualityBar');"
    "bar.style.width = Math.min(percentage, 100) + '%';"
    "}"
    "window.onload = function() {"
    "updateData();"
    "updateInterval = setInterval(updateData, 5000);"
    "};"
    "window.onbeforeunload = function() {"
    "clearInterval(updateInterval);"
    "};"
    "</script>"
    "</body>"
    "</html>";
  
  server.send(200, "text/html; charset=utf-8", html);
}

// ========== API JSON PARA DATOS DEL SENSOR ==========
void handleSensorAPI() {
  readDustSensor();
  
  String ledStatus = (digitalRead(LED_PIN) == HIGH) ? "ENCENDIDO" : "APAGADO";
  
  String json = "{";
  json += "\"dust\":" + String(dustDensity, 2) + ",";
  json += "\"voltage\":" + String(voltage, 2) + ",";
  json += "\"rawADC\":" + String(rawADC) + ",";
  json += "\"ledStatus\":\"" + ledStatus + "\",";
  json += "\"timestamp\":" + String(millis());
  json += "}";
  
  server.send(200, "application/json; charset=utf-8", json);
}

// ========== CONFIGURACIÓN WIFI ==========
void setupWiFi() {
  Serial.println("\n\n========== INICIALIZANDO ESP32 ==========");
  Serial.println("Conectando a WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conexion WiFi exitosa!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
  } else {
    Serial.println("Error: No se pudo conectar a WiFi");
  }
}

// ========== CONFIGURACIÓN DEL SERVIDOR WEB ==========
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/sensor", handleSensorAPI);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404 - Pagina no encontrada");
  });
  
  server.begin();
  Serial.println("Servidor web iniciado en el puerto 80");
}

// ========== SETUP ==========
void setup() {
  // Inicializar comunicación serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========== CONFIGURANDO PINES ==========");
  
  // Configurar pines
  pinMode(DUST_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.print("GPIO ");
  Serial.print(DUST_SENSOR_PIN);
  Serial.println(" configurado como INPUT");
  Serial.print("GPIO ");
  Serial.print(LED_PIN);
  Serial.println(" configurado como OUTPUT");
  
  // Configurar resolución del ADC
  analogReadResolution(12); // 12 bits = 0-4095
  Serial.println("ADC configurado a 12 bits (0-4095)");
  
  // Test inicial del LED
  Serial.println("\nTest del LED:");
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ENCENDIDO");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED APAGADO");
  
  // Test inicial de lectura ADC
  Serial.println("\nTest de lectura ADC:");
  for (int i = 0; i < 5; i++) {
    int valor = analogRead(DUST_SENSOR_PIN);
    Serial.print("Lectura ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(valor);
    delay(100);
  }
  
  // Conectar a WiFi
  setupWiFi();
  
  // Iniciar servidor web
  setupWebServer();
  
  Serial.println("========== INICIALIZACION COMPLETADA ==========\n");
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  // Manejar solicitudes del cliente
  server.handleClient();
  
  // Debug periódico
  if (millis() - lastDebugTime >= DEBUG_INTERVAL) {
    Serial.println("\n=== DEBUG PERIODICO ===");
    int testADC = analogRead(DUST_SENSOR_PIN);
    float testVoltage = (testADC / (float)ADC_MAX) * VOLTAGE_REF;
    Serial.print("Valor ADC crudo: ");
    Serial.println(testADC);
    Serial.print("Voltaje: ");
    Serial.print(testVoltage);
    Serial.println(" V");
    Serial.println("===================\n");
    lastDebugTime = millis();
  }
  
  // Mostrar IP periódicamente
  if (millis() - lastDisplayTime >= DISPLAY_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[");
      Serial.print(millis() / 1000);
      Serial.print("s] IP de la pagina web: http://");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[!] WiFi desconectado - Intentando reconectar...");
      WiFi.reconnect();
    }
    lastDisplayTime = millis();
  }
  
  delay(10);
}
