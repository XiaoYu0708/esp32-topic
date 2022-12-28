#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cstring>
#include <iostream>

using namespace std;

#define USE_SERIAL Serial

const char ssid[] = "WTY";
const char password[] = "A940812A";
const char url[] = "https://data.epa.gov.tw/api/v2/uv_s_01?api_key=e8dd42e6-9b8b-43f8-991e-b3dee723a52d&limit=34&sort=publishtime%20desc&format=JSON";
const char* mqttServer = "192.168.26.35";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

String Side, old_time;
int config = 1;

String arr_county[34];
String arr_side[34];
String arr_uv[34];
String arr_update_time[34];

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);
  Serial.println("開始連線到SSID：");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("連線完成");

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {

      Serial.println("connected");
      client.subscribe("UV");
    } else {

      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  //Serial.println("啟動網頁連線");
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  //Serial.print("httpCode=");
  //Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    delay(2000);
    //Serial.print("payload=");
    //Serial.println(payload.length());
    //USE_SERIAL.println(payload);
    // String input;

    DynamicJsonDocument doc(8192);

    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    for (JsonObject field : doc["fields"].as<JsonArray>()) {

      const char* field_id = field["id"];      // "sitename", "uvi", "publishagency", "county", "wgs84lon", ...
      const char* field_type = field["type"];  // "text", "text", "text", "text", "text", "text", "text"

      const char* field_info_label = field["info"]["label"];  // "測站名稱", "紫外線指數", "發布單位", "縣市", "WGS84經度", ...
    }

    const char* resource_id = doc["resource_id"];  // "4a128442-6065-4787-a5ef-3dadf3c5f176"

    const char* extras_api_key = doc["__extras"]["api_key"];  // "e8dd42e6-9b8b-43f8-991e-b3dee723a52d"

    bool include_total = doc["include_total"];             // true
    const char* total = doc["total"];                      // "1869055"
    const char* resource_format = doc["resource_format"];  // "object"
    const char* limit = doc["limit"];                      // "34"
    const char* offset = doc["offset"];                    // "0"

    const char* links_start = doc["_links"]["start"];
    const char* links_next = doc["_links"]["next"];
    int i = 0;
    for (JsonObject record : doc["records"].as<JsonArray>()) {

      const char* record_sitename = record["sitename"];            // "塔塔加", "阿里山", "屏東", "橋頭", "新營", "朴子", "斗六", "南投", ...
      const char* record_uvi = record["uvi"];                      // "1.5", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", ...
      const char* record_publishagency = record["publishagency"];  // "環境保護署", "環境保護署", "環境保護署", "環境保護署", ...
      const char* record_county = record["county"];                // "嘉義縣", "嘉義縣", "屏東縣", "高雄市", "臺南市", "嘉義縣", "雲林縣", "南投縣", ...
      const char* record_wgs84lon = record["wgs84lon"];            // "120,51,35", "120,48,8", "120,29,17", "120,18,20", ...
      const char* record_wgs84lat = record["wgs84lat"];            // "23,28,19", "23,30,37", "22,40,23", "22,45,27", ...
      const char* record_publishtime = record["publishtime"];      // "2022-12-22 09:00", "2022-12-22 09:00", ...

      arr_side[i] = record_sitename;
      arr_county[i] = record_county;
      arr_uv[i] = record_uvi;
      arr_update_time[i] = record_publishtime;

      i++;
    }
  }
  if (config == 1) {
    Serial.println("請先選擇地區！");
  }
  if (old_time != arr_update_time[0]) {
    display();
  }
  client.loop();
  http.end();
}


void callback(char* topic, byte* message, unsigned int length) {

  if (String(topic) == "UV") {
    Side = "";
    for (int i = 0; i < length; i++) {
      Side += (char)message[i];
    }
    config = 0;
    display();
  }
}

void display() {
  for (int n = 0; n <= 34; n++) {
    if (arr_side[n] == Side) {
      Serial.print("地區：");
      Serial.print(arr_county[n]);
      Serial.print("，");
      Serial.print(arr_side[n]);
      Serial.print("，UV:");
      Serial.print(arr_uv[n]);
      Serial.print("，最後更新時間：");
      Serial.println(arr_update_time[n]);
      old_time = arr_update_time[0];
    }
  }
}
