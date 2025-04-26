#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h"

// WiFi credentials
const char* ssid = "SIJA-PNP";
const char* password = "12345678";

// MQTT Server config
#define MQTT_SERVER     "broker.emqx.io"
#define MQTT_PORT       1883

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_PORT);

// MQTT Topics
Adafruit_MQTT_Subscribe admin_sub = Adafruit_MQTT_Subscribe(&mqtt, "topic/admin");

// Publish topic (dinamis)
String publish_topic = "";
Adafruit_MQTT_Publish* data_pub = nullptr;

// Pin setup
#define DHTPIN 4
#define DHTTYPE DHT22
#define SOILPIN 35 // soil analog input pin

DHT dht(DHTPIN, DHTTYPE);

bool start_publish = false;
unsigned long lastPublish = 0;

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Init MQTT
  mqtt.subscribe(&admin_sub);
  dht.begin();

  pinMode(SOILPIN, INPUT); // optional but good practice
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.processPackets(10);
  mqtt.ping();

  // Cek pesan dari admin
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &admin_sub) {
      String msg = (char *)admin_sub.lastread;
      Serial.print("Pesan dari admin: ");
      Serial.println(msg);

      if (msg.startsWith("set topic ")) {
        publish_topic = msg.substring(10); // ambil setelah "set topic "
        if (data_pub) delete data_pub;
        data_pub = new Adafruit_MQTT_Publish(&mqtt, publish_topic.c_str());
        start_publish = true;
        Serial.print("Mulai publish ke topic: ");
        Serial.println(publish_topic);
      }
    }
  }

  // Publish suhu, kelembaban, dan soil sebagai JSON
  if (start_publish && (millis() - lastPublish > 5000)) {
    lastPublish = millis();
    float suhu = dht.readTemperature();
    float humi = dht.readHumidity();
    int soil_raw = analogRead(SOILPIN);
    float soilPercent = map(soil_raw, 1300, 3300, 100, 0); // sesuaikan kalibrasi

    if (!isnan(suhu) && !isnan(humi) && data_pub) {
      String jsonPayload = "{";
      jsonPayload += "\"temperature\":";
      jsonPayload += String(suhu, 2);
      jsonPayload += ",\"humidity\":";
      jsonPayload += String(humi, 2);
      jsonPayload += ",\"soil\":";
      jsonPayload += String(soilPercent, 2);
      jsonPayload += "}";

      Serial.print("Kirim: ");
      Serial.println(jsonPayload);
      data_pub->publish(jsonPayload.c_str());
    } else {
      Serial.println("Gagal baca suhu/kelembaban");
    }
  }
}

void reconnect() {
  int8_t ret;
  while ((ret = mqtt.connect()) != 0) {
    Serial.print("Gagal konek MQTT, retry 5 detik. Error: ");
    Serial.println(mqtt.connectErrorString(ret));
    delay(5000);
  }
  Serial.println("MQTT connected!");
}