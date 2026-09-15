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
unsigned long lastDisplayTime = 0;
const unsigned long DISPLAY_INTERVAL = 10000; // Mostrar IP cada 10 segundos

// ========== FUNCIÓN PARA LEER EL SENSOR ==========
void readDustSensor() {
  // Encender LED
  digitalWrite(LED_PIN, HIGH);
  
  // Esperar a que se estabilice
  delayMicroseconds(WAIT_TIME * 1000);
  
  // Leer múltiples muestras y promediar
  long sumValue = 0;
  int numSamples = 100;
  for (int i = 0; i < numSamples; i++) {
    sumValue += analogRead(DUST_SENSOR_PIN);
    delayMicroseconds(10);
  }
  
  int avgRaw = sumValue / numSamples;
  
  // Apagar LED
  digitalWrite(LED_PIN, LOW);
  
  // Convertir lectura ADC a voltaje
  voltage = (avgRaw / (float)ADC_MAX) * VOLTAGE_REF;
  
  // Convertir voltaje a densidad de polvo (µg/m³)
  // Fórmula del fabricante: densidad = (voltage - Vref) / 0.005
  // Vref típicamente es 0.8V
  float Vref = 0.8;
  dustDensity = (voltage - Vref) / 0.005;
  
  // Asegurar que no sea negativo
  if (dustDensity < 0) {
    dustDensity = 0;
  }
}

// ========== PÁGINA WEB PRINCIPAL ==========
void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html lang="es">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Monitor de Polvo - GP2Y1010</title>
      <style>
        * {
          margin: 0;
          padding: 0;
          box-sizing: border-box;
        }
        
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          min-height: 100vh;
          display: flex;
          justify-content: center;
          align-items: center;
          padding: 20px;
        }
        
        .container {
          background: white;
          border-radius: 15px;
          box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
          padding: 40px;
          max-width: 500px;
          width: 100%;
        }
        
        h1 {
          text-align: center;
          color: #333;
          margin-bottom: 10px;
          font-size: 28px;
        }
        
        .subtitle {
          text-align: center;
          color: #666;
          margin-bottom: 30px;
          font-size: 14px;
        }
        
        .sensor-card {
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          border-radius: 10px;
          padding: 30px;
          color: white;
          margin-bottom: 20px;
          text-align: center;
        }
        
        .metric {
          margin-bottom: 20px;
        }
        
        .metric-label {
          font-size: 14px;
          opacity: 0.9;
          margin-bottom: 8px;
          text-transform: uppercase;
          letter-spacing: 1px;
        }
        
        .metric-value {
          font-size: 48px;
          font-weight: bold;
          font-family: 'Courier New', monospace;
        }
        
        .metric-unit {
          font-size: 18px;
          opacity: 0.9;
          margin-left: 8px;
        }
        
        .info-box {
          background: #f5f5f5;
          border-left: 4px solid #667eea;
          padding: 15px;
          margin-bottom: 20px;
          border-radius: 5px;
        }
        
        .info-box-title {
          font-weight: bold;
          color: #333;
          margin-bottom: 5px;
        }
        
        .info-box-content {
          color: #666;
          font-size: 14px;
          line-height: 1.6;
        }
        
        .status-indicator {
          display: inline-block;
          width: 12px;
          height: 12px;
          border-radius: 50%;
          margin-right: 8px;
          animation: pulse 2s infinite;
        }
        
        .status-good {
          background-color: #4caf50;
        }
        
        .status-moderate {
          background-color: #ff9800;
        }
        
        .status-poor {
          background-color: #f44336;
        }
        
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
        
        .update-btn {
          width: 100%;
          padding: 12px;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          color: white;
          border: none;
          border-radius: 5px;
          font-size: 16px;
          cursor: pointer;
          transition: transform 0.2s;
        }
        
        .update-btn:hover {
          transform: translateY(-2px);
        }
        
        .update-btn:active {
          transform: translateY(0);
        }
        
        .last-update {
          text-align: center;
          color: #999;
          font-size: 12px;
          margin-top: 15px;
        }
        
        .quality-bar {
          width: 100%;
          height: 8px;
          background: #ddd;
          border-radius: 10px;
          margin-top: 15px;
          overflow: hidden;
        }
        
        .quality-bar-fill {
          height: 100%;
          background: linear-gradient(90deg, #4caf50, #ff9800, #f44336);
          width: 0%;
          transition: width 0.5s ease;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>🌍 Monitor de Polvo</h1>
        <p class="subtitle">Sensor GP2Y1010 en ESP32</p>
        
        <div class="sensor-card">
          <div class="metric">
            <div class="metric-label">Densidad de Polvo</div>
            <div class="metric-value" id="dustValue">--</div>
            <div class="metric-unit">µg/m³</div>
          </div>
          
          <div class="metric">
            <div class="metric-label">Voltaje del Sensor</div>
            <div style="font-size: 28px; font-weight: bold;">
              <span id="voltageValue">--</span>
              <span class="metric-unit" style="font-size: 14px;">V</span>
            </div>
          </div>
        </div>
        
        <div id="qualityStatus" class="info-box">
          <div class="info-box-title">
            <span class="status-indicator status-good" id="statusIndicator"></span>
            Calidad del Aire
          </div>
          <div class="info-box-content" id="qualityText">
            Cargando datos...
          </div>
          <div class="quality-bar">
            <div class="quality-bar-fill" id="qualityBar"></div>
          </div>
        </div>
        
        <button class="update-btn" onclick="updateData()">🔄 Actualizar Ahora</button>
        <div class="last-update" id="lastUpdate">Última actualización: --</div>
      </div>
      
      <script>
        let updateInterval;
        
        function updateData() {
          fetch('/api/sensor')
            .then(response => response.json())
            .then(data => {
              // Actualizar valores
              document.getElementById('dustValue').textContent = data.dust.toFixed(2);
              document.getElementById('voltageValue').textContent = data.voltage.toFixed(2);
              
              // Actualizar fecha
              const now = new Date();
              document.getElementById('lastUpdate').textContent = 
                'Última actualización: ' + now.toLocaleTimeString('es-ES');
              
              // Actualizar calidad del aire
              updateQualityStatus(data.dust);
            })
            .catch(error => console.error('Error:', error));
        }
        
        function updateQualityStatus(dustValue) {
          let status, text, percentage, indicatorClass;
          
          if (dustValue < 50) {
            status = 'Excelente';
            text = '✓ La calidad del aire es excelente';
            percentage = (dustValue / 50) * 25;
            indicatorClass = 'status-good';
          } else if (dustValue < 100) {
            status = 'Bueno';
            text = '✓ La calidad del aire es buena';
            percentage = 25 + ((dustValue - 50) / 50) * 25;
            indicatorClass = 'status-good';
          } else if (dustValue < 150) {
            status = 'Moderado';
            text = '⚠ La calidad del aire es moderada. Se recomienda precaución.';
            percentage = 50 + ((dustValue - 100) / 50) * 25;
            indicatorClass = 'status-moderate';
          } else {
            status = 'Pobre';
            text = '✗ La calidad del aire es pobre. Se recomienda usar mascarilla.';
            percentage = 75 + ((dustValue - 150) / 150) * 25;
            indicatorClass = 'status-poor';
          }
          
          const qualityText = document.getElementById('qualityText');
          qualityText.innerHTML = `<strong>${status}</strong><br>${text}`;
          
          const indicator = document.getElementById('statusIndicator');
          indicator.className = 'status-indicator ' + indicatorClass;
          
          const bar = document.getElementById('qualityBar');
          bar.style.width = Math.min(percentage, 100) + '%';
        }
        
        // Actualizar cada 5 segundos
        window.onload = function() {
          updateData();
          updateInterval = setInterval(updateData, 5000);
        };
        
        // Limpiar intervalo al salir
        window.onbeforeunload = function() {
          clearInterval(updateInterval);
        };
      </script>
    </body>
    </html>
  )";
  
  server.send(200, "text/html; charset=utf-8", html);
}

// ========== API JSON PARA DATOS DEL SENSOR ==========
void handleSensorAPI() {
  readDustSensor();
  
  String json = "{";
  json += "\"dust\":" + String(dustDensity, 2) + ",";
  json += "\"voltage\":" + String(voltage, 2) + ",";
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
    Serial.println("✓ Conexión WiFi exitosa!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
  } else {
    Serial.println("✗ Error: No se pudo conectar a WiFi");
  }
}

// ========== CONFIGURACIÓN DEL SERVIDOR WEB ==========
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/sensor", handleSensorAPI);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404 - Página no encontrada");
  });
  
  server.begin();
  Serial.println("✓ Servidor web iniciado en el puerto 80");
}

// ========== SETUP ==========
void setup() {
  // Inicializar comunicación serial
  Serial.begin(115200);
  delay(1000);
  
  // Configurar pines
  pinMode(DUST_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Configurar resolución del ADC
  analogReadResolution(12); // 12 bits = 0-4095
  
  Serial.println("✓ Pines configurados correctamente");
  Serial.print("  - Sensor de polvo: GPIO ");
  Serial.println(DUST_SENSOR_PIN);
  Serial.print("  - LED: GPIO ");
  Serial.println(LED_PIN);
  
  // Conectar a WiFi
  setupWiFi();
  
  // Iniciar servidor web
  setupWebServer();
  
  Serial.println("========== INICIALIZACIÓN COMPLETADA ==========\n");
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  // Manejar solicitudes del cliente
  server.handleClient();
  
  // Mostrar IP periódicamente
  if (millis() - lastDisplayTime >= DISPLAY_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[");
      Serial.print(millis() / 1000);
      Serial.print("s] IP de la página web: http://");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[!] WiFi desconectado - Intentando reconectar...");
      WiFi.reconnect();
    }
    lastDisplayTime = millis();
  }
  
  delay(10);
}
