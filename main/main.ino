#include <WiFi.h>
#include <LiquidCrystal_I2C_Hangul.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <CString>
#include <iostream>
#include <WiFiClientSecure.h>
using namespace std;

#define USE_SERIAL Serial

const char ssid[] = "WTY_GWL";
const char password[] = "A940812A";
const char url[] = "https://data.epa.gov.tw/api/v2/uv_s_01?api_key=e8dd42e6-9b8b-43f8-991e-b3dee723a52d&limit=34&sort=publishtime%20desc&format=JSON";
const char *mqttServer = "192.168.0.105";
const int mqttPort = 1883;
const char *mqttUser = "";
const char *mqttPassword = "";

char host[] = "notify-api.line.me"; // LINE Notify API網址
String Linetoken = "rOHJ8EqnoiDD1fYXOdRDGyWvwGYbxmlwc8IvZigPLOk";

String Side, old_time;
int config = 1, error_url = 0;

String arr_county[34];
String arr_side[34];
String arr_uv[34];
String arr_update_time[34];

WiFiClient espClient;
PubSubClient client(espClient);
WiFiClientSecure line_client; // 網路連線物件

LiquidCrystal_I2C_Hangul lcd(0x27, 16, 2);

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);
  Serial.println("開始連線到SSID：");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("連線完成");

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected())
  {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP32Client", mqttUser, mqttPassword))
    {

      Serial.println("connected");
      client.subscribe("UV");
    }
    else
    {

      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  line_client.setInsecure(); // ESP32核心 1.0.6以上

  lcd.init();
  lcd.backlight();
}

void loop()
{
  // put your main code here, to run repeatedly:
  // Serial.println("啟動網頁連線");
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  // Serial.print("httpCode=");
  // Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    delay(2000);
    // Serial.print("payload=");
    // Serial.println(payload.length());
    // USE_SERIAL.println(payload);
    //  String input;

    DynamicJsonDocument doc(8192);

    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    for (JsonObject field : doc["fields"].as<JsonArray>())
    {

      const char *field_id = field["id"];     // "sitename", "uvi", "publishagency", "county", "wgs84lon", ...
      const char *field_type = field["type"]; // "text", "text", "text", "text", "text", "text", "text"

      const char *field_info_label = field["info"]["label"]; // "測站名稱", "紫外線指數", "發布單位", "縣市", "WGS84經度", ...
    }

    const char *resource_id = doc["resource_id"]; // "4a128442-6065-4787-a5ef-3dadf3c5f176"

    const char *extras_api_key = doc["__extras"]["api_key"]; // "e8dd42e6-9b8b-43f8-991e-b3dee723a52d"

    bool include_total = doc["include_total"];            // true
    const char *total = doc["total"];                     // "1869055"
    const char *resource_format = doc["resource_format"]; // "object"
    const char *limit = doc["limit"];                     // "34"
    const char *offset = doc["offset"];                   // "0"

    const char *links_start = doc["_links"]["start"];
    const char *links_next = doc["_links"]["next"];
    int i = 0;
    for (JsonObject record : doc["records"].as<JsonArray>())
    {

      const char *record_sitename = record["sitename"];           // "塔塔加", "阿里山", "屏東", "橋頭", "新營", "朴子", "斗六", "南投", ...
      const char *record_uvi = record["uvi"];                     // "1.5", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", ...
      const char *record_publishagency = record["publishagency"]; // "環境保護署", "環境保護署", "環境保護署", "環境保護署", ...
      const char *record_county = record["county"];               // "嘉義縣", "嘉義縣", "屏東縣", "高雄市", "臺南市", "嘉義縣", "雲林縣", "南投縣", ...
      const char *record_wgs84lon = record["wgs84lon"];           // "120,51,35", "120,48,8", "120,29,17", "120,18,20", ...
      const char *record_wgs84lat = record["wgs84lat"];           // "23,28,19", "23,30,37", "22,40,23", "22,45,27", ...
      const char *record_publishtime = record["publishtime"];     // "2022-12-22 09:00", "2022-12-22 09:00", ...

      arr_side[i] = record_sitename;
      arr_county[i] = record_county;
      arr_uv[i] = record_uvi;
      arr_update_time[i] = record_publishtime;

      i++;
    }
  }
  if (config == 1)
  {
    Serial.println("請先選擇地區！");
    Line("請先選擇地區！");
    config = 0;
  }
  if (old_time != arr_update_time[0] && error_url == 0)
  {
    display();
  }

  client.loop();
  http.end();
}

void callback(char *topic, byte *message, unsigned int length)
{

  if (String(topic) == "UV")
  {
    Side = "";
    for (int i = 0; i < length; i++)
    {
      Side += (char)message[i];
    }
    config = 0;
    display();
  }
}

void display()
{
  for (int n = 0; n <= 34; n++)
  {
    if (arr_side[n] == Side)
    {
      String level = "";
      if (arr_uv[n].toInt() >= 11)
      {
        level = "紫色";
      }
      else if (arr_uv[n].toInt() >= 8)
      {
        level = "紅色";
      }
      else if (arr_uv[n].toInt() >= 6)
      {
        level = "橙色";
      }
      else if (arr_uv[n].toInt() >= 3)
      {
        level = "黃色";
      }
      else
      {
        level = "綠色";
      }

      if (arr_uv[n].toInt() >= 0)
      {
        String message = "\n地區：" + arr_county[n] + "，" + arr_side[n] + "\n紫外線指數，UV：" + arr_uv[n] + "\n紫外線等級：" + level + "\n最後更新時間：" + arr_update_time[n];
        Serial.println(message);
        Line(message);

        char msg[64] = {0};

        strcat(msg, String(arr_county[n]).c_str());
        strcat(msg, ",");
        strcat(msg, String(arr_side[n]).c_str());
        strcat(msg, ",");
        strcat(msg, String(arr_uv[n]).c_str());
        strcat(msg, ",");
        strcat(msg, level.c_str());
        strcat(msg, ",");
        strcat(msg, String(arr_update_time[n]).c_str());

        client.publish("UV_Return", msg);
        client.publish("UV_Return_error", "");

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("UV:" + arr_uv[n]);
        lcd.setCursor(0, 1);
        lcd.print(arr_update_time[n]);

        error_url = 0;
      }
      else
      {
        String message = arr_county[n] + arr_side[n] + "\n網站錯誤：\nhttps://data.epa.gov.tw/api/v2/uv_s_01?api_key=e8dd42e6-9b8b-43f8-991e-b3dee723a52d&limit=34&sort=publishtime%20desc&format=JSON";

        Line(message);

        char msg[64] = {0};

        strcat(msg, String(arr_county[n]).c_str());
        strcat(msg, ",");
        strcat(msg, String(arr_side[n]).c_str());
        strcat(msg, ",");
        strcat(msg, 0);
        strcat(msg, ",");
        strcat(msg, "error");
        strcat(msg, ",");
        strcat(msg, "error");

        client.publish("UV_Return", msg);
        client.publish("UV_Return_error", "網站錯誤");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("error");

        error_url = 1;
      }
      old_time = arr_update_time[0];
    }
  }
}

void Line(String message)
{
  if (line_client.connect(host, 443))
  {
    int LEN = message.length();
    // 傳遞POST表頭
    String url = "/api/notify";
    line_client.println("POST " + url + " HTTP/1.1");
    line_client.print("Host: ");
    line_client.println(host);
    // 權杖
    line_client.print("Authorization: Bearer ");
    line_client.println(Linetoken);
    line_client.println("Content-Type: application/x-www-form-urlencoded");
    line_client.print("Content-Length: ");
    line_client.println(String((LEN + 8)));
    line_client.println();
    line_client.print("message=");
    line_client.println(message);
    line_client.println();
    // 等候回應
    delay(2000);
    String response = line_client.readString();
    // 顯示傳遞結果
    Serial.println(response);
    line_client.stop(); // 斷線，否則只能傳5次
  }
  else
  {
    // 傳送失敗
    Serial.println("connected fail");
  }
}