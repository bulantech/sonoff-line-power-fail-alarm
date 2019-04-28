#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <WiFiClientSecureAxTLS.h> // arduino core 2.5.0

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <FS.h> 
#include <ArduinoJson.h>  // V 5.13.4

#define SW_DET  12
#define SW_PIN  0
#define LED 13

int state = 0; 
int stateCount = 0;
int firstStart = 1;

int POWER_UP_SEND_COUNT = 3; //ต้องการให้แจ้งเตือนซ้ำ 3 ครั้ง
int POWER_DOWN_SEND_COUNT = 5; //ต้องการให้แจ้งเตือนซ้ำ 3 ครั้ง
int POWER_SEND_TIME =  60; //ห่างกัน 60 วินาที

int powerUpSendCount = POWER_UP_SEND_COUNT; 
int powerUpSendTime = POWER_SEND_TIME; 
int powerDownSendCount = POWER_DOWN_SEND_COUNT; 
int powerDownSendTime = POWER_SEND_TIME; 

// Line config
//#define LINE_TOKEN "ez4tu8UFv2Yw7ELVCeyrfSfFqOYTUlb6QqTfZJotMp3" //"LhVtD4p7uL879nYR9EoACUvZuC30gNonSQ1RaNC5jUP"
char LINE_TOKEN[45] = "ez4tu8UFv2Yw7ELVCeyrfSfFqOYTUlb6QqTfZJotMp3";
String line_token;
char power_down_count[4] = "6";
char power_up_count[4] = "6";
char cap_count[4] = "60";

bool shouldSaveConfig = false;

// https://meyerweb.com/eric/tools/dencoder/
String powerDown = "%E0%B8%95%E0%B8%AD%E0%B8%99%E0%B8%99%E0%B8%B5%E0%B9%89%E0%B9%82%E0%B8%A3%E0%B8%87%E0%B9%80%E0%B8%88%E0%B8%99%E0%B9%84%E0%B8%9F%E0%B8%9F%E0%B9%89%E0%B8%B2%E0%B9%82%E0%B8%A3%E0%B8%87%E0%B8%97%E0%B8%B5%E0%B9%884%20%E0%B9%84%E0%B8%9F%E0%B8%9F%E0%B9%89%E0%B8%B2%E0%B8%94%E0%B8%B1%E0%B8%9A%E0%B8%AD%E0%B8%A2%E0%B8%B9%E0%B9%88";
String powerUp = "%E0%B8%95%E0%B8%AD%E0%B8%99%E0%B8%99%E0%B8%B5%E0%B9%89%E0%B9%82%E0%B8%A3%E0%B8%87%E0%B9%80%E0%B8%88%E0%B8%99%E0%B9%84%E0%B8%9F%E0%B8%9F%E0%B9%89%E0%B8%B2%E0%B9%82%E0%B8%A3%E0%B8%87%E0%B8%97%E0%B8%B5%E0%B9%884%20%E0%B9%84%E0%B8%9F%E0%B8%9F%E0%B9%89%E0%B8%B2%E0%B8%A1%E0%B8%B2%E0%B8%9B%E0%B8%81%E0%B8%95%E0%B8%B4%E0%B9%81%E0%B8%A5%E0%B9%89%E0%B8%A7";

void Line_Notify_Send(String msg) {
  digitalWrite(LED, 0); //on
//  return; //test
//  WiFiClientSecure client; 
  axTLS::WiFiClientSecure client; // arduino core 2.5.0

  if (!client.connect("notify-api.line.me", 443)) {
    Serial.println("connection failed");
    return;   
  }

  String req = "";
  req += "POST /api/notify HTTP/1.1\r\n";
  req += "Host: notify-api.line.me\r\n";
  req += "Authorization: Bearer " + String(line_token) + "\r\n";
  req += "Cache-Control: no-cache\r\n";
  req += "User-Agent: ESP8266\r\n";
  req += "Content-Type: application/x-www-form-urlencoded\r\n";
  req += "Content-Length: " + String(String("message=" + msg).length()) + "\r\n";
  req += "\r\n";
  req += "message=" + msg;
  // Serial.println(req);
  client.print(req);
    
  delay(20);

  Serial.println("-------------");
  while(client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
    Serial.println(line);
  }
  Serial.println("-------------");
  digitalWrite(LED, 1); //off
}

void ledBlink() {
  digitalWrite(LED, 0); //on
  delay(10);
  digitalWrite(LED, 1); //off
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(SW_DET, INPUT_PULLUP);
  pinMode(LED, OUTPUT);  
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  Serial.println("Reset wifi config?:");
  for(int i=5; i>0; i--){
    Serial.print(String(i)+" "); 
    ledBlink();
    delay(1000);
  }
  
  //reset saved settings
  if(digitalRead(SW_PIN) == LOW) // Press button
  {
    Serial.println();
    Serial.println("Reset wifi config");
    digitalWrite(LED, 0); //on
    wifiManager.resetSettings(); 
    SPIFFS.format();
  }    

//  wifiManager.autoConnect("AutoConnectAP");
//  Serial.println("connected...yeey :)");

  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
        
          strcpy(LINE_TOKEN, json["LINE_TOKEN"]);
          strcpy(power_down_count, json["power_down_count"]);
          strcpy(power_up_count, json["power_up_count"]);
          strcpy(cap_count, json["cap_count"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManagerParameter custom_LINE_TOKEN("LINE_TOKEN", "LINE_TOKEN", LINE_TOKEN, 43);
  WiFiManagerParameter custom_power_down_count("power_down_count", "power_down_count", power_down_count, 2);
  WiFiManagerParameter custom_power_up_count("power_up_count", "power_up_count", power_up_count, 2);
  WiFiManagerParameter custom_cap_count("cap_count", "cap_count", cap_count, 2);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
//  WiFiManagerParameter blynk_text("<br><span>BLYNK</span>");
//  wifiManager.addParameter(&blynk_text);
  wifiManager.addParameter(&custom_LINE_TOKEN);
  wifiManager.addParameter(&custom_power_down_count);
  wifiManager.addParameter(&custom_power_up_count);
  wifiManager.addParameter(&custom_cap_count);

  if (!wifiManager.autoConnect("PowerFailAlarm")) {
    Serial.println("failed to connect and hit timeout");
//    delay(3000);
    for(int i=0; i<10; i++) {
      digitalWrite(LED_BUILTIN, 0); //on
      delay(50);
      digitalWrite(LED_BUILTIN, 1); //off
      delay(500); 
    }
    
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(LINE_TOKEN, custom_LINE_TOKEN.getValue());
  strcpy(power_down_count, custom_power_down_count.getValue());
  strcpy(power_up_count, custom_power_up_count.getValue());
  strcpy(cap_count, custom_cap_count.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    Serial.println("LINE_TOKEN -> "+String(LINE_TOKEN));
    json["LINE_TOKEN"] = LINE_TOKEN;
    json["power_down_count"] = power_down_count;
    json["power_up_count"] = power_up_count;
    json["cap_count"] = cap_count;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  line_token = String(LINE_TOKEN);
  POWER_DOWN_SEND_COUNT = String(power_down_count).toInt();
  POWER_UP_SEND_COUNT = String(power_up_count).toInt();
  POWER_SEND_TIME = String(cap_count).toInt();

  Serial.println();
  Serial.print("local ip: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ledBlink();

  if(!digitalRead(SW_DET)) { //power down
    if(!state) { //แจ้งเตือนไฟดับครั้งแรก
      state = 1;
      powerDownSendCount = POWER_DOWN_SEND_COUNT; //กำหนดค่านับจำนวนกี่ครั้ง
      powerDownSendTime = 1000*POWER_SEND_TIME; //กำหนดค่าเวลาในการส่ง
      Serial.println("POWER DOWN");    
      Line_Notify_Send(powerDown);      
    } else {
      powerDownSendTime -= 5000;
      if(powerDownSendTime <= 0) {
        powerDownSendTime = 1000*POWER_SEND_TIME;
        if(powerDownSendCount) {
          --powerDownSendCount;
          Serial.print(String(powerDownSendCount));
          Serial.println(" POWER DOWN");    
          Line_Notify_Send(powerDown);    
        }
      }
    }
    delay(5000);
  } else { //power up
    if(firstStart) {
      firstStart = 0;      
      Serial.println("Smart Detector Start");
      Line_Notify_Send("Smart Detector Start");
    } else {
      if(state) { //แจ้งเตือนไฟติดครั้งแรก
        state = 0;
        powerUpSendCount = POWER_UP_SEND_COUNT; //กำหนดค่านับจำนวนกี่ครั้ง
        powerUpSendTime = 1000*POWER_SEND_TIME; //กำหนดค่าเวลาในการส่ง
        Serial.println("POWER UP");
        Line_Notify_Send(powerUp);
      } else {
        powerUpSendTime -= 2000;
        if(powerUpSendTime <= 0) {
          powerUpSendTime = 1000*POWER_SEND_TIME;
          if(powerUpSendCount) {
            --powerUpSendCount;
            Serial.print(String(powerUpSendCount));
            Serial.println(" POWER UP");
            Line_Notify_Send(powerUp);    
          }
        }
      }     
    } 
    delay(2000);   
  }   
  
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
