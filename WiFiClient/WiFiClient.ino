#include <WiFi.h>
#include <esp_wifi.h>
#include <PubSubClient.h>

// --- הגדרות רשת ו-MQTT ---
const char* WIFI_SSID     = "cellcom-va2.4";     // שם רשת ה-Wi-Fi
const char* WIFI_PASSWORD = "av046080021"; // סיסמת ה-Wi-Fi
const char* ssid = "cellcom-va2.4";
const char* password = "av046080021"; // הסיסמה לרשת ה-Wi-Fi
const char* mqtt_server = "10.100.102.27";    // ה-IP של השרת שבו רץ ה-Docker

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// פינים להדגמה (למשל ממסר למשאבה/חימום)
const int RELAY_PUMP_PIN = 26;

// טיימר לשליחת מדדים (כמו טמפרטורה/מצב)
unsigned long lastMsgTime = 0;

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);

  // תמיכה בערוצים 1-13 (ישראל/אירופה)
  wifi_country_t country = { .cc = "IL", .schan = 1, .nchan = 13, .max_tx_power = 20, .policy = WIFI_COUNTRY_POLICY_AUTO };
  esp_wifi_set_country(&country);

  wifi_config_t sta_config;
  memset(&sta_config, 0, sizeof(sta_config));
  strcpy((char*)sta_config.sta.ssid, ssid);
  strcpy((char*)sta_config.sta.password, password);
  sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  WiFi.begin();

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
}

// פונקציית טיפול בפקודות נכנסות מ-Home Assistant
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  // טיפול בפקודת משאבה
  if (String(topic) == "pool/pump/set_state") {
    if (message == "ON") {
      digitalWrite(RELAY_PUMP_PIN, HIGH);
      mqttClient.publish("pool/pump/state", "ON"); // דיווח על שינוי המצב בחזרה ל-HA
    } else if (message == "OFF") {
      digitalWrite(RELAY_PUMP_PIN, LOW);
      mqttClient.publish("pool/pump/state", "OFF");
    }
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // חיבור לשרת ה-MQTT עם הודעת סטטוס אונליין/אופליין (LWT)
    if (mqttClient.connect("Pool_ESP32_Master", "pool/status", 1, true, "offline")) {
      Serial.println("connected!");
      
      // עדכון הסטטוס ל-online
      mqttClient.publish("pool/status", "online", true);

      // הרשמה לערוץ קבלת הפקודות מ-HA
      mqttClient.subscribe("pool/pump/set_state");
    } else {
      Serial.printf("failed, rc=%d. Retrying in 5 seconds...\n", mqttClient.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW);

  setup_wifi();

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // דוגמה לשליחת נתונים מחזוריים (למשל קריאת חיישן) כל 10 שניות
  unsigned long now = millis();
  if (now - lastMsgTime > 10000) {
    lastMsgTime = now;
    
    // כאן תוכל להכניס קריאה אמיתית מחיישנים
    float temp = 26.5; 
    mqttClient.publish("pool/temperature/water", String(temp, 1).c_str());
  }
}