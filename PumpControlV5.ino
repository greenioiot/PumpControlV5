
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Wire.h>
#include <ShiftRegister74HC595.h>

#include <WiFiManager.h> 
#include <WiFiClient.h>

#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <WebServer.h>
//#include <AsyncTCP.h>
//#include <ESPAsyncWebServer.h>

//#include <SPIFFS.h>

#define MDNS_DEVICE_NAME "pump-esp32"
#define SERVICE_NAME "pump-service"
#define SERVICE_PROTOCOL "udp"
#define SERVICE_PORT 5600

// ตั้งค่า Port ของ Web Server เป็น Port 80
WebServer server(80);
//AsyncWebServer server(80);
  
// หน้า Login
const char* loginPage =  
"<div style='display:flex; justify-content:center; margin-top:100px;'>"
  "<div style='padding: 30px 40px; border-radius:16px; background: #eee;'>"
    "<div style='display:flex; flex-direction:column; gap:20px;'>"
      "<div style='display:flex; justify-content:center;'>"
        "<b>ESP32 Login</b>"
      "</div>"
      "<div style='display:flex;'>"
        "<label style='width:100px;'>Username:</label>"
          "<input style='height:18px;' type='text' id='username' >"
      "</div>"
      "<div style='display:flex;'>"
        "<label style='width:100px;'>Password:</label>"
          "<input style='height:18px;' type='Password' id='password'>"
      "</div>"
      "<div style='display:flex; justify-content:end;'>"
        "<button style='width:80px; height:28px;' onclick='login()'>Login</button>"
      "</div>"
    "</div>"
  "</div>"
"<div>"
"<script>"
    "function login(){"
      "fetch('login', {"
        "method: 'POST',"
        "headers: {"
          "'Content-Type': 'application/json',"
        "},"
        "body: JSON.stringify({"
          "username: '' + document.querySelector('#username').value,"
          "password: '' + document.querySelector('#password').value"
        "}),"
      "})"
      ".then(response => response.json())"
      ".then(data => {"
        "console.log('Success:', data);"
        "if(data){"
          "if(data['success'] != null){"
        "let success = data['success'];"
        "let message = data['message'];"
        "if(success) {"
           "alert('login successful.');"
           "window.location = 'uploadOTA';"
        "}"
        "else {"
           "alert('login failed, ', message);"  
        "}"
          "}"
        "}"
      "})"
      ".catch((error) => {"
        "alert('error: ', error);"
      "});"
    "}"
"</script>";

// หน้า Index Page
const char* uploadOTAPage = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";


#define dryIn1 35
#define dryIn2 34
#define dryIn3 39
#define dryIn4 36

// parameters: <number of shift registers> (data pin, clock pin, latch pin)
//ShiftRegister74HC595<1> sr(25, 13, 26);
ShiftRegister74HC595 sr(1, 25, 13, 26);

#define deviceMango01 "2a770b00-913d-11ec-8c3f-7fb91a1b0600"
#define deviceMango02 "5fa49720-91ab-11ec-8c3f-7fb91a1b0600"
#define deviceMango03 "76ff4a00-91ab-11ec-8c3f-7fb91a1b0600"
#define deviceMango04 "9ea40130-91b1-11ec-8c3f-7fb91a1b0600"
#define deviceMango05 "ae4996e0-91b1-11ec-8c3f-7fb91a1b0600"
#define deviceMango06 "abd3cc60-993a-11ec-8c3f-7fb91a1b0600"

#define tokenMango01 "mangosteen01"
#define tokenMango02 "mangosteen02"
#define tokenMango03 "mangosteen03"
#define tokenMango04 "mangosteen04"
#define tokenMango05 "mangosteen05"
#define tokenMango06 "mangosteen06"

#define deviceControlMango "4073b320-a486-11ec-8c3f-7fb91a1b0600"
#define tokenControlMango "ControlMangosteen04"

 /*
#define MAX_ACCESS_TOKEN 1024 
#define userLoginToken "greenio.asia@gmail.com"
#define passLoginToken "green7650"

bool IsAccessTokenOK = false;
char AccessToken[MAX_ACCESS_TOKEN];
int IndexAccessToken = 0;
*/
String ACCESS_TOKEN = "eyJhbGciOiJIUzUxMiJ9.eyJzdWIiOiJncmVlbmlvLmFzaWFAZ21haWwuY29tIiwic2NvcGVzIjpbIlRFTkFOVF9BRE1JTiJdLCJ1c2VySWQiOiJiODcyN2E5MC03NTZmLTExZTgtOTRmYS0xZjBhM2UyODRmNTEiLCJmaXJzdE5hbWUiOiJJdHRpY2hhaSIsImVuYWJsZWQiOnRydWUsImlzUHVibGljIjpmYWxzZSwidGVuYW50SWQiOiJhY2IwMTI4MC03NTZmLTExZTgtOTRmYS0xZjBhM2UyODRmNTEiLCJjdXN0b21lcklkIjoiMTM4MTQwMDAtMWRkMi0xMWIyLTgwODAtODA4MDgwODA4MDgwIiwiaXNzIjoiaHR0cDovL2Fpcy50Yi5uaTExLmNvbSIsImlhdCI6MTY0OTU4MTg2MiwiZXhwIjoxNjgxMTE3ODYyfQ._2UjMIpQn1pay53b1ZJJf2sFIiwMLZzo5P9hseufP9alDkq4tB7X6RGwcskfZ6KCSh-Vryz803ZzupJbOKTRMQ";

uint8_t getRelayAllState = 0;
uint8_t readRelayAllState = 0;
uint8_t prevRelayAllState = 0;

typedef struct {
  int temp;
  int humid;
  int setValue;
  int timeout;
  int countdown;
} pumpConfig_t;

pumpConfig_t pumpV1;
pumpConfig_t pumpV2;
pumpConfig_t pumpV3;
pumpConfig_t pumpV4;

unsigned long currentMillis = 0;
unsigned long startMillis = 0;
unsigned long controlMillis = 0;

const unsigned long periodCheckStatus = 1000;
const unsigned long periodControlRelay = 10000;
 
uint8_t countESPReset = 0;
const unsigned char timeESPReset = 60;

bool IsSetDNS = false;
bool IsLoginUploadOTA = false;
String userOTA = "admin";
String passOTA = "p@ssw0rd123";
 
void postLoginOTA()
{
  String postBody = server.arg("plain");
  Serial.println(postBody);
  StaticJsonDocument<400> doc;
  DeserializationError error = deserializeJson(doc, postBody); 
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(200, "application/json", " {\"success\":false,\"message\":\"invalid data request\"}");
    return;
  }
  if(!doc["username"].isNull() && !doc["password"].isNull()){
    String username = doc["username"];
    String password = doc["password"]; 
    Serial.println("username: " + username);
    Serial.println("password: " + password);
    if( username == userOTA && password == passOTA){
      IsLoginUploadOTA = true;
      server.send(200, "application/json", " {\"success\":true,\"message\":\"\"}"); 
      return;
    } 
  }  
  IsLoginUploadOTA = false;
  server.send(200, "application/json", " {\"success\":false,\"message\":\"invalid username or password\"}");  
}
/*
void GetAccessToken(String user, String pass) 
{ 
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient _client;
    HTTPClient _http;
    String serverPath = "https://thingcontrol.io/api/auth/login"; 
    _http.begin(_client, serverPath.c_str());
    _http.addHeader("Content-Type", "application/json");
    //_http.addHeader("X-Authorization", "Bearer " + ACCESS_TOKEN);
    
    String _data = "{ \"username\":\"" + user + "\",\"password\":\"" + pass + "\" }";
    Serial.print("Get Access Token: ");
    Serial.println(_data);
    
    int httpResponseCode = _http.POST(_data);
    if (httpResponseCode > 0 ) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = _http.getString();
      Serial.println(payload); 
      
      if(httpResponseCode == 200){
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload); 
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        else {
          if(!doc["token"].isNull()){
            String valStr = doc["token"];
            int len = valStr.length();
            for(int i = 0; i < len; i++){
              if(i < MAX_ACCESS_TOKEN){
                AccessToken[IndexAccessToken] = valStr[i];
                IndexAccessToken++;
              }
            }
            AccessToken[IndexAccessToken] = '\0';
            if(IndexAccessToken == len){
              Serial.println("IsAcessToken: OK");
              IsAccessTokenOK = true;
            }
            else {
              Serial.println("IsAcessToken: Failed");
            }
            Serial.println("read AcessToken: " + valStr);
          }
        }
        
      }
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    _http.end();
  } 
}
*/
void GetAttributes(String deviceToken, pumpConfig_t* pump)
{
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient _client;
    HTTPClient _http;
    String serverPath = "http://thingcontrol.io:8080/api/v1/" + deviceToken + "/attributes?";
   
    _http.begin(_client, serverPath.c_str());
    _http.addHeader("Content-Type", "application/json"); 
    
    int httpResponseCode = _http.GET();
    if (httpResponseCode > 0 ) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = _http.getString();
      //Serial.println(payload);

      if(httpResponseCode == 200)
      {
        StaticJsonDocument<400> doc;
        DeserializationError error = deserializeJson(doc, payload); 
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        else {
          if(!doc["set"].isNull()){
            String valStr = doc["set"];
            pump->setValue = valStr.toInt();
            Serial.println("read setValue: " + valStr);
          }
          if(!doc["timeout"].isNull()){
            String valStr = doc["timeout"];
            pump->timeout = valStr.toInt(); 
            Serial.println("read timeout: " + valStr);
          } 
        }
      }
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    _http.end();
  }
}
void GetTelemetry(String deviceId, pumpConfig_t* pump)
{
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient _client;
    HTTPClient _http;
    String serverPath = "http://thingcontrol.io:8080/api/plugins/telemetry/DEVICE/" + deviceId + "/values/timeseries?";
 
    _http.begin(_client, serverPath.c_str());
    _http.addHeader("Content-Type", "application/json");
    _http.addHeader("X-Authorization", "Bearer " + ACCESS_TOKEN);
    
    int httpResponseCode = _http.GET();
    if (httpResponseCode > 0 ) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = _http.getString();
      //Serial.println(payload);
      if(httpResponseCode == 200){
        StaticJsonDocument<400> doc;
        DeserializationError error = deserializeJson(doc, payload); 
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        else { 
          if(!doc["hum"][0]["value"].isNull()){
            String valStr = doc["hum"][0]["value"];
            pump->humid = valStr.toInt();
            Serial.println("read humid: " + valStr);
          }
          if(!doc["temp"][0]["value"].isNull()){
            String valStr = doc["temp"][0]["value"];
            pump->temp = valStr.toInt();
            Serial.println("read temp: " + valStr);
          }  
        }
      }
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    _http.end();
  }  
}
void PostTelemetry(String deviceToken, String keys, String values)
{
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient _client;
    HTTPClient _http;
    String serverPath = "http://thingcontrol.io:8080/api/v1/" + deviceToken + "/telemetry"; 
    _http.begin(_client, serverPath.c_str());
    _http.addHeader("Content-Type", "application/json");
    _http.addHeader("X-Authorization", "Bearer " + ACCESS_TOKEN);
    
    String _data = "{\"" + keys + "\":\"" + values+  "\"}";
    Serial.println("Post to " + deviceToken + ": " + _data);
    
    int httpResponseCode = _http.POST(_data);
    if (httpResponseCode > 0 ) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = _http.getString();
      Serial.println(payload); 
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    _http.end();
  } 
}
void GetRelayState(String deviceId)
{
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient _client;
    HTTPClient _http;
    String serverPath = "http://thingcontrol.io:8080/api/plugins/telemetry/DEVICE/" + deviceId + "/values/timeseries?";
 
    _http.begin(_client, serverPath.c_str());
    _http.addHeader("Content-Type", "application/json");
    _http.addHeader("X-Authorization", "Bearer " + ACCESS_TOKEN);
    
    int httpResponseCode = _http.GET();
    if (httpResponseCode > 0 ) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = _http.getString();
      //Serial.println(payload);
      
      if(httpResponseCode == 200){
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload); 
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        else { 
           getRelayAllState = 0;
           
           if(!doc["value0"][0]["value"].isNull()){
              String _valueStr = doc["value0"][0]["value"];
              int _value = _valueStr.toInt();
              Serial.print("read value0: ");
              Serial.println(_value);
              if(_value != 0){
                getRelayAllState |= 0x01;
              }
              else {
                getRelayAllState &= ~0x01;
              }
           }
           if(!doc["value1"][0]["value"].isNull()){
              String _valueStr = doc["value1"][0]["value"];
              int _value = _valueStr.toInt();
              Serial.print("read value1: ");
              Serial.println(_value);
              if(_value != 0){
                getRelayAllState |= 0x02;
              }
              else {
                getRelayAllState &= ~0x02;
              }
           }
           if(!doc["value2"][0]["value"].isNull()){
              String _valueStr = doc["value2"][0]["value"];
              int _value = _valueStr.toInt();
              Serial.print("read value2: ");
              Serial.println(_value);
              if(_value != 0){
                getRelayAllState |= 0x04;
              }
              else {
                getRelayAllState &= ~0x04;
              }
           }
           if(!doc["value3"][0]["value"].isNull()){
              String _valueStr = doc["value3"][0]["value"];
              int _value = _valueStr.toInt();
              Serial.print("read value3: ");
              Serial.println(_value);
              if(_value != 0){
                getRelayAllState |= 0x08;
              }
              else {
                getRelayAllState &= ~0x08;
              }
           }
        }
      }
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    _http.end();
  } 
}



 
int wifiStatus = WL_IDLE_STATUS;
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}
 
void setup() 
{
  Serial.begin(115200);
  Serial.println(F("Open SerialPort Buadrate: 115200"));
  Serial.println(F("***********************************"));
  
  /*
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }*/
  
  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);
 
  if (!wifiManager.autoConnect("@Thingcontrol_AP")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset(); 
    Serial.println("failed to connect reset in " + String(countESPReset) + "/" + String(timeESPReset));
    if(countESPReset >= timeESPReset){
      countESPReset = 0;
      ESP.restart();
    }
    countESPReset++;
    delay(1000);
  } 
  currentMillis = millis();  //initial start time

   /*
    if (!MDNS.begin("pump-esp32")) { //http://pump-esp32.local
      Serial.println(F("Error setting up MDNS responder!")); 
      ESP.restart(); 
    }
    Serial.println("mDNS responder started");
  */
  if(!MDNS.begin(MDNS_DEVICE_NAME)) {
     Serial.println(F("Error encountered while starting mDNS"));
  } 
  //MDNS.addService(SERVICE_NAME, SERVICE_PROTOCOL, SERVICE_PORT); 
  //Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, []() {
    IsLoginUploadOTA = false;
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginPage);
  });
  
  server.on("/login", HTTP_POST, postLoginOTA);
  
  server.on("/uploadOTA", HTTP_GET, []() {
    if(IsLoginUploadOTA) {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", uploadOTAPage);
    }
    else {
      server.sendHeader("Connection", "close");
      server.send(401, "text/html", "Unauthorized");
    }
  });
  server.on("/update", HTTP_POST, []() {
    if(IsLoginUploadOTA == true){
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }
    else {
      server.sendHeader("Connection", "close");
      server.send(401, "text/html", "Unauthorized");
    }
  }, []() {
    if(IsLoginUploadOTA == true){
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    }
  });
  server.begin();
  Serial.println("HTTP server started");
  
  pinMode(dryIn1, INPUT);
  pinMode(dryIn2, INPUT);
  pinMode(dryIn3, INPUT);
  pinMode(dryIn4, INPUT);

  for (int i = 0; i < 4; i++)
  {
    sr.set(i, HIGH); // set single pin HIGH
    delay(250);
  }
  for (int i = 0; i < 4; i++)
  {
    sr.set(i, LOW); // set single pin HIGH
    delay(250);
  }
  sr.setAllHigh(); // set all pins HIGH
  delay(500);

  sr.setAllLow(); // set all pins LOW
  delay(500); 

  //init relay state
  readRelayAllState = 0;

  pumpV1.temp = 8888;
  pumpV2.temp = 8888;
  pumpV3.temp = 8888;
  pumpV4.temp = 8888;

  pumpV1.humid = 9999;
  pumpV2.humid = 9999;
  pumpV3.humid = 9999;
  pumpV4.humid = 9999;
  
  pumpV1.setValue = 65;
  pumpV2.setValue = 65;
  pumpV3.setValue = 65;
  pumpV4.setValue = 65;

  pumpV1.countdown = 0;
  pumpV2.countdown = 0;
  pumpV3.countdown = 0;
  pumpV4.countdown = 0; 
}



void loop() 
{ 
  
  currentMillis = millis();   
  if( abs(currentMillis - startMillis) >= periodCheckStatus){
    wifiStatus = WiFi.status();
    if(wifiStatus == WL_CONNECTED){
      countESPReset = 0;
      if(IsSetDNS == false){
        if(!MDNS.begin(MDNS_DEVICE_NAME)) {
           Serial.println("Error encountered while starting mDNS");
           return;
        } 
        //MDNS.addService(SERVICE_NAME, SERVICE_PROTOCOL, SERVICE_PORT); 
        //Serial.println(WiFi.localIP());
        IsSetDNS = true;
      } 
    }
    else {
      Serial.println("WiFi status failed!");
      Serial.println("ESP reset in " + String(countESPReset) + "/" + String(timeESPReset));
      if(countESPReset >= timeESPReset){
        countESPReset = 0;
        ESP.restart();
      }
      countESPReset++;
    } 

    if(pumpV1.countdown > 0){
      pumpV1.countdown--;
      Serial.println("pumpV1.countdown: " + String(pumpV1.countdown));
      if(pumpV1.countdown <= 0){ 
        Serial.println(F("Turn Off Relay 1 and Post Telemetry"));
        sr.set(0, LOW);  
        PostTelemetry(tokenControlMango, "value0", "0");
        delay(100);
      } 
    }
    if(pumpV2.countdown > 0){
      pumpV2.countdown--;
      Serial.println("pumpV2.countdown: " + String(pumpV2.countdown));
      if(pumpV2.countdown <= 0){
        Serial.println(F("Turn Off Relay 2 and Post Telemetry"));
        sr.set(1, LOW);  
        PostTelemetry(tokenControlMango, "value1", "0");
        delay(100);
      } 
    }
    if(pumpV3.countdown > 0){
      pumpV3.countdown--;
      Serial.println("pumpV3.countdown: " + String(pumpV3.countdown));
      if(pumpV3.countdown <= 0){
        Serial.println(F("Turn Off Relay 3 and Post Telemetry"));
        sr.set(2, LOW);  
        PostTelemetry(tokenControlMango, "value2", "0");
        delay(100);
      } 
    }
    if(pumpV4.countdown > 0){
      pumpV4.countdown--;
      Serial.println("pumpV4.countdown: " + String(pumpV4.countdown));
      if(pumpV4.countdown <= 0){
        Serial.println(F("Turn Off Relay 4 and Post Telemetry"));
        sr.set(3, LOW);  
        PostTelemetry(tokenControlMango, "value3", "0");
        delay(100);
      } 
    }
    
    startMillis = currentMillis;
  }

  if( abs(currentMillis - controlMillis) >= periodControlRelay){
    if(wifiStatus == WL_CONNECTED){
      
      GetAttributes(tokenMango01, &pumpV1);
      delay(100);
      GetAttributes(tokenMango02, &pumpV2);
      delay(100);
      GetAttributes(tokenMango03, &pumpV3);
      delay(100);
      GetAttributes(tokenMango04, &pumpV4);
      delay(100);

      GetTelemetry(deviceMango01, &pumpV1);
      delay(100);
      GetTelemetry(deviceMango02, &pumpV2);
      delay(100);
      GetTelemetry(deviceMango03, &pumpV3);
      delay(100);
      GetTelemetry(deviceMango04, &pumpV4);
      delay(100);
 
      if( pumpV1.countdown <= 0 ){
        if( (pumpV1.humid / 10.0) <= pumpV1.setValue ){
          pumpV1.countdown = pumpV1.timeout * 60;

          Serial.println(F("Turn On Relay 1 and Post Telemetry"));
          sr.set(0, HIGH);  
          PostTelemetry(tokenControlMango, "value0", "1");
          delay(100);
        }
      }
      if( pumpV2.countdown <= 0 ){
        if( (pumpV2.humid / 10.0) <= pumpV2.setValue ){
          pumpV2.countdown = pumpV2.timeout * 60;

          Serial.println(F("Turn On Relay 2 and Post Telemetry"));
          sr.set(1, HIGH);  
          PostTelemetry(tokenControlMango, "value1", "1");
          delay(100);
        }
      }
      if( pumpV3.countdown <= 0 ){
        if( (pumpV3.humid / 10.0) <= pumpV3.setValue ){
          pumpV3.countdown = pumpV3.timeout * 60;

          Serial.println(F("Turn On Relay 3 and Post Telemetry"));
          sr.set(2, HIGH);  
          PostTelemetry(tokenControlMango, "value2", "1");
          delay(100);
        }
      }
      if( pumpV4.countdown <= 0 ){
        if( (pumpV4.humid / 10.0) <= pumpV4.setValue ){
          pumpV4.countdown = pumpV4.timeout * 60;

          Serial.println(F("Turn On Relay 4 and Post Telemetry"));
          sr.set(3, HIGH);  
          PostTelemetry(tokenControlMango, "value3", "1");
          delay(100);
        }
      }

      GetRelayState(deviceControlMango);
      Serial.print("Get Relay State: ");
      Serial.print((getRelayAllState & 0x01) >> 0);
      Serial.print(" ");
      Serial.print((getRelayAllState & 0x02) >> 1);
      Serial.print(" ");
      Serial.print((getRelayAllState & 0x04) >> 2);
      Serial.print(" ");
      Serial.print((getRelayAllState & 0x08) >> 3);
      Serial.println();

      uint8_t* readAllState = sr.getAll();
      readRelayAllState = (*readAllState);
      Serial.print("Read Relay State: ");
      Serial.print((readRelayAllState & 0x01) >> 0);
      Serial.print(" ");
      Serial.print((readRelayAllState & 0x02) >> 1);
      Serial.print(" ");
      Serial.print((readRelayAllState & 0x04) >> 2);
      Serial.print(" ");
      Serial.print((readRelayAllState & 0x08) >> 3);
      Serial.println();

      if(readRelayAllState != getRelayAllState){
        readRelayAllState = getRelayAllState;
        //--------------- Set Relay 1 ---------------
        if( (readRelayAllState & 0x01) != 0 ){  
          Serial.println(F("Turn On Relay 1"));
          sr.set(0, HIGH); 
          delay(100);
        }
        else {  
          Serial.println(F("Turn Off Relay 1"));
          sr.set(0, LOW); 
          delay(100);
        }
        //--------------- Set Relay 2 ---------------
        if( (readRelayAllState & 0x02) != 0 ){  
          Serial.println(F("Turn On Relay 2"));
          sr.set(1, HIGH); 
          delay(100);
        }
        else {  
          Serial.println(F("Turn Off Relay 2"));
          sr.set(1, LOW); 
          delay(100);
        }
        //--------------- Set Relay 3 ---------------
        if( (readRelayAllState & 0x04) != 0 ){  
          Serial.println(F("Turn On Relay 3"));
          sr.set(2, HIGH); 
          delay(100);
        }
        else {  
          Serial.println(F("Turn Off Relay 3"));
          sr.set(2, LOW); 
          delay(100);
        }
        //--------------- Set Relay 4 ---------------
        if( (readRelayAllState & 0x08) != 0 ){ 
          Serial.println(F("Turn On Relay 4")); 
          sr.set(3, HIGH); 
          delay(100);
        }
        else {  
          Serial.println(F("Turn Off Relay 4"));
          sr.set(3, LOW); 
          delay(100);
        }
      }
      
    }
    controlMillis = currentMillis;
  }
  server.handleClient(); 
}
