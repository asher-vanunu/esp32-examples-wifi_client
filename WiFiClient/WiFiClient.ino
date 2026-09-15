#include <WiFi.h>
#include <esp_wifi.h> // נדרש להגדרת Country Code
#include <PubSubClient.h>

// ===================================================
// 1. פרטי ה-Wi-Fi ושרת ה-MQTT שלך
// ===================================================
const char* WIFI_SSID     = "cellcom-va2.4";     // שם רשת ה-Wi-Fi
const char* WIFI_PASSWORD = "av046080021"; // סיסמת ה-Wi-Fi
//const char* WIFI_SSID     = "Alango_WiFi_Second";     // שם רשת ה-Wi-Fi
//const char* WIFI_PASSWORD = "alangorulezzz"; // סיסמת ה-Wi-Fi

const char* MQTT_BROKER   = "10.100.102.27";      // כתובת ה-IP של ה-Home Assistant / MQTT Broker
const uint16_t MQTT_PORT  = 1883;                 // פורט ברירת המחדל
const char* MQTT_USER     = "mqtt_user";          // שם משתמש MQTT (אם מוגדר ב-HA)
const char* MQTT_PASS     = "mqtt_password";      // סיסמת MQTT (אם מוגדרת ב-HA)

// ===================================================
// 2. אובייקטים לתקשורת
// ===================================================
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
int testCounter = 0;

// ===================================================
// 3. חיבור ל-Wi-Fi ול-MQTT
// ===================================================
void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);

  // 1. אתחול מנגנון ה-Wi-Fi
  WiFi.mode(WIFI_STA);

  // 2. הגדרת אבטחה בסיסית
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);

  // 3. הגדרת אזור (Israel/Europe) התומך בערוצים 1 עד 13
  wifi_country_t country = {
    .cc = "IL",
    .schan = 1,
    .nchan = 13,
    .max_tx_power = 20,
    .policy = WIFI_COUNTRY_POLICY_AUTO
  };
  esp_wifi_set_country(&country);

  // 4. שינוי פרמטרי הסריקה כך שיכללו סריקה של ערוצים 12-13 (All Channels)
  wifi_scan_threshold_t scan_thresh = {
    .rssi = -127,
    .authmode = WIFI_AUTH_WPA2_PSK
  };
  
  wifi_config_t sta_config;
  memset(&sta_config, 0, sizeof(sta_config));
  strcpy((char*)sta_config.sta.ssid, WIFI_SSID);
  strcpy((char*)sta_config.sta.password, WIFI_PASSWORD);
  sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN; // סריקה של כל הערוצים (1-13) ולא רק 1-11
  sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL; // התחברות לאות החזק ביותר

  // 5. החלת ההגדרות והתחלת החיבור
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void reconnect() {
  // בלולאה עד להתחברות מחדש ל-MQTT
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // ניסיון חיבור (מזהה ייחודי + LWT)
    if (client.connect("ESP32_Pool_Test", MQTT_USER, MQTT_PASS, "pool/status", 1, true, "offline")) {
      Serial.println("connected!");
      
      // ברגע שהתחברנו, שולחים הודעת שהבקר מחובר (Online)
      client.publish("pool/status", "online", true);
      
      // הרשמה ל-Topic לפקודות נכנסות (למשל הדלקת לד/ממסר לבדיקה)
      client.subscribe("pool/test_switch/set");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// ===================================================
// 4. קבלת הודעות נכנסות מ-Home Assistant
// ===================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // דוגמה: אם קיבלנו פקודה ב-Topic הבדיקה
  if (String(topic) == "pool/test_switch/set") {
    if (message == "ON") {
      Serial.println("Switch is ON!");
      client.publish("pool/test_switch/state", "ON", true);
    } else if (message == "OFF") {
      Serial.println("Switch is OFF!");
      client.publish("pool/test_switch/state", "OFF", true);
    }
  }
}

// ===================================================
// 5. Setup & Loop
// ===================================================
void setup() {
  Serial.begin(115200);
  setup_wifi();
  
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // שליחת הודעת בדיקה (מונה עולה) כל 10 שניות
  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    testCounter++;
    
    String payload = String(testCounter);
    client.publish("pool/test_counter", payload.c_str());
    Serial.print("Published counter: ");
    Serial.println(payload);
  }
}


#if 0

/*
    Go to thingspeak.com and create an account if you don't have one already.
    After logging in, click on the "New Channel" button to create a new channel for your data. This is where your data will be stored and displayed.
    Fill in the Name, Description, and other fields for your channel as desired, then click the "Save Channel" button.
    Take note of the "Write API Key" located in the "API keys" tab, this is the key you will use to send data to your channel.
    Replace the channelID from tab "Channel Settings" and privateKey with "Read API Keys" from "API Keys" tab.
    Replace the host variable with the thingspeak server hostname "api.thingspeak.com"
    Upload the sketch to your ESP32 board and make sure that the board is connected to the internet. The ESP32 should now send data to your Thingspeak channel at the intervals specified by the loop function.
    Go to the channel view page on thingspeak and check the "Field1" for the new incoming data.
    You can use the data visualization and analysis tools provided by Thingspeak to display and process your data in various ways.
    Please note, that Thingspeak accepts only integer values.

    You can later check the values at https://thingspeak.com/channels/2005329
    Please note that this public channel can be accessed by anyone and it is possible that more people will write their values.
 */

#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "cellcom-va5";          // Change this to your WiFi SSID
const char *password = "av046080021";  // Change this to your WiFi password

const char *host = "api.thingspeak.com";        // This should not be changed
const int httpPort = 80;                        // This should not be changed
const String channelID = "2005329";             // Change this to your channel ID
const String writeApiKey = "V6YOTILH9I7D51F9";  // Change this to your Write API key
const String readApiKey = "34W6LGLIFXD56MPM";   // Change this to your Read API key

// The default example accepts one data filed named "field1"
// For your own server you can ofcourse create more of them.
int field1 = 0;

int numberOfResults = 3;  // Number of results to be read
int fieldNumber = 1;      // Field number which will be read out

void setup() {
  Serial.begin(115200);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println("******************************************************");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void readResponse(NetworkClient *client) {
  unsigned long timeout = millis();
  while (client->available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client->stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client->available()) {
    String line = client->readStringUntil('\r');
    Serial.print(line);
  }

  Serial.printf("\nClosing connection\n\n");
}

void loop() {
  NetworkClient client;
  String footer = String(" HTTP/1.1\r\n") + "Host: " + String(host) + "\r\n" + "Connection: close\r\n\r\n";

  // WRITE --------------------------------------------------------------------------------------------
  if (!client.connect(host, httpPort)) {
    return;
  }

  client.print("GET /update?api_key=" + writeApiKey + "&field1=" + field1 + footer);
  readResponse(&client);

  // READ --------------------------------------------------------------------------------------------

  String readRequest = "GET /channels/" + channelID + "/fields/" + fieldNumber + ".json?results=" + numberOfResults + " HTTP/1.1\r\n" + "Host: " + host + "\r\n"
                       + "Connection: close\r\n\r\n";

  if (!client.connect(host, httpPort)) {
    return;
  }

  client.print(readRequest);
  readResponse(&client);

  // -------------------------------------------------------------------------------------------------

  ++field1;
  delay(10000);
}


#endif