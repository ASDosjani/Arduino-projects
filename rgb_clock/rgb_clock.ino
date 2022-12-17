//HTML converter: https://tomeko.net/online_tools/cpp_text_escape.php?lang=en

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h> 
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino_JSON.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include "icons.h"

//#define DEBUG //uncomment this to turn on Serial communication

#define BUTTON_PIN 5
bool buttonstate=false;
#define ONE_WIRE_BUS 13
#define LED_PIN 4
#define LED_COUNT 8
#define BUZZER_PIN 16
int r=255,g=255,b=255;
#define NOTE_A4 49         // MIDI note value for middle A
#define FREQ_A4 440        // frequency for middle A

//https://openweathermap.org/weather-conditions
#define THUNDER 200+99
#define DRIZZLE 300+99
#define RAIN 500+99
#define SNOW 600+99
#define ATMOSPHERE 700+99
#define SUN  800
#define SUN_CLOUD  801
#define CLOUD 801+98

unsigned long lastCallTime = 0, timeout = 0;            // last time you called the updateWeather function, in milliseconds
const unsigned long postingInterval = 10L * 1000L;  // delay between updates, in milliseconds

u8g2_uint_t offset;     // current offset for the scrolling text
u8g2_uint_t width;      // pixel width of the scrolling text (must be lesser than 128 unless U8G2_16BIT is defined
float weatherTemperature = 0;
int weatherId = 0, weatherHumidity = 0;
String weatherDescription = "",city;
float tempC;
bool transition=false;
int page=0;

JSONVar json=JSON.parse("[]");//music
bool play=false;
int noteindex=0;
unsigned long notetime=0;

//Week Days
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//Month names
const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
tm timeinfo;
time_t now;
long unsigned lastNTPtime;
unsigned long lastEntryTime,clocktimeout=0,weathertimeout=0,screendimtime,ledtimeout=0,longpress=0,sleeptimer=300000;
int brightmode=0,ledspeed=60,ledmode=0,ledbrightness=128,ledb2,ledb5,lastmode=1;
long ledhue3=0,ledhue4 = 0,ledhue5=0;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ400);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 14, /* data=*/ 12);

ESP8266WebServer server(80);
ESP8266WiFiMulti wifiMulti;
//initiate the WifiClient
WiFiClient client;

DNSServer dnsServer;

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  pinMode(BUZZER_PIN,OUTPUT);
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  delay(10);
  strip.show();            // Turn OFF all pixels ASAP
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  if(!LittleFS.begin()){
#ifdef DEBUG
    Serial.println("An Error has occurred while mounting LittleFS");
#endif
    return;
  }
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setBitmapMode(1);
  u8g2.setFontMode(1);

  File file = LittleFS.open("/ap", "r");
  String ap = file.readString();
  file.close();
  JSONVar apjson=JSON.parse(ap);
  if(LittleFS.exists("/ap"))
  for(int i=0;i<apjson.length();i+=2)
  {
    if(String((const char*)apjson[i+1]).length()==1)
        wifiMulti.addAP(apjson[i]);
    else
        wifiMulti.addAP(apjson[i],apjson[i+1]);
  }
  
  width = u8g2.getDisplayWidth();
  u8g2.clearBuffer();
  drawConnect();
  u8g2.sendBuffer();
  connectWiFi();
  
  if (MDNS.begin("rgbclock")) {
#ifdef DEBUG
    Serial.println("MDNS responder started");
#endif
  }
  
  server.on("/", ledhtml);
  server.on("/music", musichtml);
  server.on("/ledupdate", ledupdate);
  server.on("/musicupdate", musicupdate);
  server.on("/musicfile", musicfile);
  server.on("/musicplay", musicplay);
  server.on("/musicstop", musicstop);
  server.on("/settings", settingshtml);
  server.on("/apupdate", apupdate);
  server.on("/cityupdate", cityupdate);
  server.on("/reboot", [](){ESP.restart();});
  server.onNotFound(settingshtml);
  server.begin();
  
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("rgbclock");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  sensors.begin();

  File c = LittleFS.open("/city", "r"); //read city
  city = c.readString();
  c.close();
  configTime(0, 0, NTP_SERVER);
  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  if (getNTPtime(10)) {  // wait up to 10sec to sync
#ifdef DEBUG
    Serial.println("Time not set");
#endif
  }
  lastNTPtime = time(&now);
  lastEntryTime = millis();

  tone(BUZZER_PIN, 523) ; //DO note 523 Hz
  delay (100); 
  tone(BUZZER_PIN, 587) ; //RE note ...
  delay (100); 
  tone(BUZZER_PIN, 659) ; //MI note ...
  delay (100); 
  tone(BUZZER_PIN, 783) ; //FA note ...
  delay (100); 
  tone(BUZZER_PIN, 880) ; //SOL note ...
  delay (100); 
  tone(BUZZER_PIN, 987) ; //LA note ...
  delay (100); 
  tone(BUZZER_PIN, 1046) ; // SI note ...
  delay (100); 
  noTone(BUZZER_PIN) ;

  ArduinoOTA.begin();
#ifdef DEBUG
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  screendimtime=millis()+20000;
}

void loop() {
  if(ledmode==2&&ledtimeout<=millis()){ //LED Modes
    ledtimeout=millis()+ledspeed;
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i,r,g,b); //  Set pixel's color (in RAM)
    }
    if(ledb2<ledbrightness){
      ledb2++;
      if(ledb2<0)
      strip.setBrightness(-ledb2);
      else strip.setBrightness(ledb2);
    }
    else ledb2=-ledb2;
    strip.show();
  }
  else if(ledmode==3&&ledtimeout<=millis()){
    ledtimeout=millis()+ledspeed;
    if(ledhue3==65536)ledhue3=0;
    for(int i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(ledhue3)));
    }
    strip.show(); // Update strip with new contents
    ledhue3 += 256;
  }
  else if(ledmode==4&&ledtimeout<=millis()){
    ledtimeout=millis()+ledspeed;
    if(ledhue4==65536)ledhue4=0;
    for(int i=0; i<strip.numPixels(); i++) {
      int pixelHue = ledhue4 + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    ledhue4 += 256;
  }
  else if(ledmode==5&&ledtimeout<=millis()){
    ledtimeout=millis()+ledspeed;
    if(ledhue5 == 65536)ledhue5=0;
    for(int i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(ledhue5)));
    }
    if(ledb5<ledbrightness){
      ledb5++;
      if(ledb5==0)ledhue5+=4096;
      if(ledb5<0)
      strip.setBrightness(-ledb5);
      else strip.setBrightness(ledb5);
    }
    else ledb5=-ledb5;
    strip.show();
  }
  if(screendimtime<=millis()&&brightmode==0){
    brightmode=1;
    screendimtime=millis()+10000;
    u8g2.setContrast(0); //lower brightness
    u8g2.sendF("ca", 0x0db, 1 << 4);
    u8g2.sendF("ca", 0x0d9, (1 << 4) | 10 );
  }
  if(screendimtime<=millis()&&brightmode==1){
    brightmode=2;
    u8g2.setPowerSave(1);
  }
  if(!buttonstate&&!transition&&digitalRead(BUTTON_PIN)==LOW){
    buttonstate=true;
    longpress=millis()+1000;
  }
  if(!transition&&buttonstate&&digitalRead(BUTTON_PIN)==HIGH){//button release
    buttonstate=false;
    sleeptimer=millis()+300000;
    if(longpress<=millis()){//long button press
      if(ledmode>0){
        for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
            strip.setPixelColor(i,0,0,0); //  Set pixel's color (in RAM)
          }
      strip.show();
      lastmode=ledmode;
      ledmode=0;
      }
      else {
        ledmode=lastmode;
        if(ledmode==1){
          for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
            strip.setPixelColor(i,r,g,b); //  Set pixel's color (in RAM)
          }
          strip.show();//  Update strip
         }
      }
    }
    else{//short button press
    screendimtime=millis()+20000;
    if(brightmode==2)u8g2.setPowerSave(0);
    u8g2.setContrast(255); //max brightness
    u8g2.sendF("ca", 0x0db, 7 << 4);
    u8g2.sendF("ca", 0x0d9, (15 << 4) | 1 );
    if(brightmode==2){//update when wake up
      if(page==1){
      updateWeather();
      sensors.requestTemperatures();
      tempC = sensors.getTempCByIndex(0);
      u8g2.clearBuffer();
      drawWeather(0);
      u8g2.sendBuffer();
      }
      else if(page==0){
      getTimeReducedTraffic(30);
      u8g2.clearBuffer();
      drawClock(timeinfo,0);
      u8g2.sendBuffer();
    }
    }
    else if(brightmode==0){
    page++;
    transition=true;
    if(page==1){
      updateWeather();
      sensors.requestTemperatures();
      tempC = sensors.getTempCByIndex(0);
      }
    else if(page==3)page=0;
      }
    brightmode=0;
    }
 }
  if(clocktimeout<=millis()&&brightmode<2&&page==0&&!transition){
    clocktimeout=millis()+1000;
    getTimeReducedTraffic(30);
    u8g2.clearBuffer();
    drawClock(timeinfo,0);
    u8g2.sendBuffer();
  }
  else if(weathertimeout<=millis()&&brightmode<2&&page==1&&!transition){
    weathertimeout=millis()+20000;
    updateWeather();
    sensors.requestTemperatures();
    tempC = sensors.getTempCByIndex(0);
    u8g2.clearBuffer();
    drawWeather(0);
    u8g2.sendBuffer();
    }
  if (page==1&&transition) {
    u8g2_uint_t x;
    
    // draw the scrolling text at current offset
    x = offset;
    getTimeReducedTraffic(30);
    u8g2.clearBuffer();
    drawClock(timeinfo,x);
    drawWeather(x+128);
    u8g2.sendBuffer();
    
    offset -= 8;            // scroll by 8 pixel
    if ( (u8g2_uint_t)offset < (u8g2_uint_t) - width ) {
      offset = 0;
      transition=false;
    }
  }
  if (page==2&&transition) {
    u8g2_uint_t x;

    // draw the scrolling text at current offset
    x = offset;
    u8g2.clearBuffer();
    drawWeather(x);
    drawWiFi(x+128);
    u8g2.sendBuffer();
    
    offset -= 8;            // scroll by 8 pixel
    if ( (u8g2_uint_t)offset < (u8g2_uint_t) - width ) {
      offset = 0;
      transition=false;
    }
  }
  if (page==0&&transition) {
    u8g2_uint_t x;

    // draw the scrolling text at current offset
    x = offset;
    getTimeReducedTraffic(30);
    u8g2.clearBuffer();
    drawWiFi(x);
    drawClock(timeinfo,x+128);
    u8g2.sendBuffer();
    
    offset -= 8;            // scroll by 8 pixel
    if ( (u8g2_uint_t)offset < (u8g2_uint_t) - width ) {
      offset = 0;
      transition=false;
    }
  }
  dnsServer.processNextRequest();
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  if(sleeptimer<=millis()&&ledmode==0){//lightsleep
#ifdef DEBUG
     Serial.println("Entering light sleep");
#endif
     WiFi.mode(WIFI_OFF);
     wifi_set_opmode_current(NULL_MODE);
     wifi_fpm_set_sleep_type(LIGHT_SLEEP_T); // set sleep type, the above posters wifi_set_sleep_type() didnt seem to work for me although it did let me compile and upload with no errors 
     wifi_fpm_open(); // Enables force sleep
     gpio_pin_wakeup_enable(BUTTON_PIN, GPIO_PIN_INTR_LOLEVEL); //GPIO_PIN_INTR_LOLEVEL for a logic low, can also do other interrupts, see gpio.h above
     wifi_fpm_do_sleep(0xFFFFFFF); // Sleep for longest possible time
     delay(50);
#ifdef DEBUG
     Serial.println("Wake up");
#endif
      screendimtime=millis()+20000;
      sleeptimer=millis()+300000;
      u8g2.setPowerSave(0);
      u8g2.setContrast(255); //max brightness
      u8g2.sendF("ca", 0x0db, 7 << 4);
      u8g2.sendF("ca", 0x0d9, (15 << 4) | 1 );
      u8g2.clearBuffer();
      drawConnect();
      u8g2.sendBuffer();
      connectWiFi();
      if(page==1){
      updateWeather();
      sensors.requestTemperatures();
      tempC = sensors.getTempCByIndex(0);
      u8g2.clearBuffer();
      drawWeather(0);
      u8g2.sendBuffer();
      }
      else if(page==0){
      getTimeReducedTraffic(30);
      lastEntryTime=millis()-26000;
      u8g2.clearBuffer();
      drawClock(timeinfo,0);
      u8g2.sendBuffer();
      }
      brightmode=0;
  }
  if(play){
     if(notetime<millis()){
      if(noteindex==json.length()){
      play=false;
      noteindex=0;
     }
     if(play){
      if((int)json[noteindex]==13){  //rest
        noTone(BUZZER_PIN);
        }
      else{
       float frequency =  FREQ_A4 * pow(2, (((int)json[noteindex+1]*12+(int)json[noteindex]-9-NOTE_A4) / 12.0));
       tone(BUZZER_PIN,frequency);
      }
     notetime=millis()+1000/pow(2,(int)json[noteindex+2]);
     noteindex+=3;
#ifdef DEBUG
     Serial.println(json.length());
     Serial.println(noteindex);
#endif
     }
     }
  }
  else noTone(BUZZER_PIN);
}
void connectWiFi(){
  if (wifiMulti.run(10000) != WL_CONNECTED) {
#ifdef DEBUG
    Serial.println("Connection Failed!");
#endif
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("RGB Clock");
    dnsServer.start(53,"*",WiFi.softAPIP());
    }
}
void ledhtml(){
  String option="<option value='1'>Static Color</option>\n<option value='2'>Pulsating Static Color</option>\n<option value='3'>Rainbow</option>\n<option value='4'>Circular Rainbow</option>\n<option value='5'>Pulsating Rainbow</option>";
  option.replace(String(ledmode)+"'",ledmode+"' selected");
  String led_html = "<!DOCTYPE html>\n<html>\n<head>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<style>\nbody {\n  font-family: \"Lato\", sans-serif;\n  transition: background-color .5s;\n}\n.sidenav {\n  height: 100%;\n  width: 0;\n  position: fixed;\n  z-index: 1;\n  top: 0;\n  left: 0;\n  background-color: #111;\n  overflow-x: hidden;\n  transition: 0.5s;\n  padding-top: 60px;\n}\n\n.sidenav a {\n  padding: 8px 8px 8px 32px;\n  text-decoration: none;\n  font-size: 25px;\n  color: #818181;\n  display: block;\n  transition: 0.3s;\n}\n\n.sidenav a:hover {\n  color: #f1f1f1;\n}\n\n.sidenav .closebtn {\n  position: absolute;\n  top: 0;\n  right: 25px;\n  font-size: 36px;\n  margin-left: 50px;\n}\n\n#main {\n  transition: margin-left .5s;\n  padding: 16px;\n}\n\n@media screen and (max-height: 450px) {\n  .sidenav {padding-top: 15px;}\n  .sidenav a {font-size: 18px;}\n}\n.style1 {\n\ttext-align: center;\n}\n.color-pick {\n    width:200px;\n    height:200px;\n    overflow: hidden;\n    border-radius:100%;\n   margin: auto;\n}\ninput[type='color'] {\n    width:100%;\n    height:100%;\n    background: #FFFFFF 0% 0% no-repeat padding-box;\n    box-shadow: 0px 3px 6px #0000001A;\n    transform: scale(1.5);\n}\n.style2 {\n\ttext-align: left;\n}\n</style>\n</head>\n<body>\n\n<div id=\"mySidenav\" class=\"sidenav\">\n  <a href=\"javascript:void(0)\" class=\"closebtn\" onclick=\"closeNav()\">&times;</a>\n  <a href=\"/\"><strong>LED</strong></a>\n  <a href=\"music\">Music</a>\n  <a href=\"settings\">Settings</a>\n</div>\n\n<div id=\"main\" class=\"style1\">\n  <div class=\"style2\"><span style=\"font-size:30px;cursor:pointer\" onclick=\"openNav()\" class=\"style2\">&#9776;</span>\n  </div>\n  <div class=\"color-pick\">\n  <input type=\"color\" id=\"cp\" onchange=\"update()\" value=\""
   +(String(r, HEX).length()==1?"#0"+String(r, HEX):"#"+String(r, HEX))+(String(g, HEX).length()==1?"0"+String(g, HEX):String(g, HEX))+(String(b, HEX).length()==1?"0"+String(b, HEX):String(b, HEX))
   +"\"></div>\n<div>\n<br>\n<select id=\"mode\" onchange=\"update()\" \"style=\"min-width:150px;\">"
   +option
   +"</select>\n<br><br>\n<label>Brightness:</label>\n<br>\n<input type=\"range\" id=\"b\" style=\"min-width:350px;\" min=\"0\" max=\"255\" onchange=\"update()\" value=\""
   +ledbrightness
   +"\">\n<br><br>\n<label id=\"l\">Speed:</label>\n<br>\n<input type=\"range\" id=\"speed\" style=\"min-width:350px;\" min=\"20\" max=\"100\" onchange=\"update()\" value=\""
   +map(ledspeed,100,20,20,100)
   +"\">\n</div>\n</div>\n<script>\nfunction openNav() {\n  document.getElementById(\"mySidenav\").style.width = \"250px\";\n  document.getElementById(\"main\").style.marginLeft = \"250px\";}\n\nfunction closeNav() {\n  document.getElementById(\"mySidenav\").style.width = \"0\";\n  document.getElementById(\"main\").style.marginLeft= \"0\";\n}\nfunction update() {\n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/ledupdate?value=\"+document.getElementById(\"cp\").value.substr(1)+\"&mode=\"+document.getElementById(\"mode\").value+\"&b=\"+document.getElementById(\"b\").value+\"&speed=\"+document.getElementById(\"speed\").value, true);\n  xhr.send();\n}\n</script>\n</body>\n</html>"
   ;
   server.send(200, "text/html", led_html);
   sleeptimer=millis()+300000;
}
void ledupdate(){
#ifdef DEBUG
  Serial.println(server.arg("value"));
#endif
  r=StrToHex(server.arg("value").substring(0,2));
  g=StrToHex(server.arg("value").substring(2,4));
  b=StrToHex(server.arg("value").substring(4));
  ledbrightness=server.arg("b").toInt();
  ledspeed=map(server.arg("speed").toInt(),20,100,100,20);
  if(ledmode!=2)ledb2=0;
  if(ledmode!=3)ledhue3=0;
  if(ledmode!=4)ledhue4=0;
  if(ledmode!=5){ledhue5=0; ledb5=0;}
  ledmode=server.arg("mode").toInt();
  ledtimeout=0;
  if(ledmode==1){
  strip.setBrightness(ledbrightness);
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i,r,g,b); //  Set pixel's color (in RAM)
  }
  strip.show();//  Update strip
  if(r==0&&g==0&&b==0)ledmode=0;
  }
  else if(ledmode==3||ledmode==4)strip.setBrightness(ledbrightness);
  sleeptimer=millis()+300000;
}
int StrToHex(String str)
{
  char s[str.length()+1];
  str.toCharArray(s,str.length()+1);
  return (int)strtoul(s, 0, 16);
}
void musichtml(){
    String options="";
    Dir dir = LittleFS.openDir("/m");
    while(dir.next()){
      options+="<option>"+dir.fileName()+"</option>";
    }
    
  String music_html ="<!DOCTYPE html>\n<html>\n<head>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<style>\nbody {\n  font-family: \"Lato\", sans-serif;\n  transition: background-color .5s;\n}\n.sidenav {\n  height: 100%;\n  width: 0;\n  position: fixed;\n  z-index: 1;\n  top: 0;\n  left: 0;\n  background-color: #111;\n  overflow-x: hidden;\n  transition: 0.5s;\n  padding-top: 60px;\n}\n\n.sidenav a {\n  padding: 8px 8px 8px 32px;\n  text-decoration: none;\n  font-size: 25px;\n  color: #818181;\n  display: block;\n  transition: 0.3s;\n}\n\n.sidenav a:hover {\n  color: #f1f1f1;\n}\n\n.sidenav .closebtn {\n  position: absolute;\n  top: 0;\n  right: 25px;\n  font-size: 36px;\n  margin-left: 50px;\n}\n\n#main {\n  transition: margin-left .5s;\n  padding: 16px;\n}\n\n@media screen and (max-height: 450px) {\n  .sidenav {padding-top: 15px;}\n  .sidenav a {font-size: 18px;}\n}\n.style1 {\n\ttext-align: center;\n}\n.style2 {\n\ttext-align: left;\n}\n</style>\n</head>\n<body onload=\"musicfile(document.getElementById('file').children[0])\">\n\n<div id=\"mySidenav\" class=\"sidenav\">\n  <a href=\"javascript:void(0)\" class=\"closebtn\" onclick=\"closeNav()\">&times;</a>\n  <a href=\"/\">LED</a>\n  <a href=\"music\"><strong>Music</strong></a>\n  <a href=\"settings\">Settings</a>\n</div>\n\n<div id=\"main\" class=\"style1\">\n<div class=\"style2\"><span style=\"font-size:30px;cursor:pointer\" onclick=\"openNav()\">&#9776;</span>\n</div>\n<button onclick=\"remove()\">Remove selected file</button>\n<select id=\"file\" style=\"min-width: 100px\" onchange=\"musicfile(this)\">"
  +options
  +"</select><label> File name:\n<input type=\"text\" id=\"fn\" maxlength=\"28\"> </label>\n<input type=\"file\" accept=\"text/,.txt\" id=\"import\" onchange=\"importfile()\" style=\"display: none;\">\n<button onclick=\"document.getElementById('import').click();\">Import file</button>\n<button onclick=\"savefile()\">Save as file</button>\n<br>\n<br>\n<button onclick=\"ins(0,0,0)\">Add note</button>\n<button onclick=\"document.getElementById('d').innerHTML='';\">Clear</button>\n<button onclick=\"save()\">Save to device</button>\n<button onclick=\"play()\">Play</button>\n<button onclick=\"stop()\">Stop</button>\n<div id=\"d\"></div>\n</div>\n<script>\nfunction openNav() {\n  document.getElementById(\"mySidenav\").style.width = \"250px\";\n  document.getElementById(\"main\").style.marginLeft = \"250px\";\n}\n\nfunction closeNav() {\n  document.getElementById(\"mySidenav\").style.width = \"0\";\n  document.getElementById(\"main\").style.marginLeft= \"0\";\n}\nfunction play() {\n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/musicplay\", true);\n  xhr.send();\n}\nfunction stop() {\n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/musicstop\", true);\n  xhr.send();\n}\nfunction remove() {\n  if (confirm(\"Are you sure want to remove?\") == true) {\n  var x = document.getElementById('file');\n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/musicfile?fn=\"+x.value+\"&del\", true);\n  xhr.send();\n  x.remove(x.selectedIndex);\n   if(document.getElementById(\"file\").hasChildNodes()){\n    musicfile(document.getElementById(\"file\"));\n   }\n }\n}\nfunction importfile(){\nvar fr=new FileReader();\nfr.onload=function(){\ndocument.getElementById('d').innerHTML='';\nvar r = JSON.parse(fr.result);\nfor(let i =0;i<r.length;i+=3){\nins(r[i],r[i+1],r[i+2]);\n}\n}\nfr.readAsText(document.getElementById(\"import\").files[0]);\ndocument.getElementById(\"fn\").value=document.getElementById(\"import\").files[0].name.slice(0,-4);\n}\nfunction savefile(){\nif(document.getElementById(\"fn\").value!=\"\")\n  {\n  if(document.getElementById('d').hasChildNodes()){\nvar array = document.getElementById('d').getElementsByTagName(\"select\");\nvar s=[];\n    for(var i = 0;i<array.length;i++)\n    {\n    s.push(new Number(array[i].value));\n    }\nvar a = document.createElement(\"a\");\na.href = window.URL.createObjectURL(new Blob([JSON.stringify(s)], {type: \"text/plain\"}));\na.download = document.getElementById(\"fn\").value+\".txt\";\na.click();\n}\nelse alert(\"There is nothing to save!\");\n}\nelse alert(\"File name required!\");\n}\nfunction musicfile(element){\n  var xhr = new XMLHttpRequest();\n  xhr.onreadystatechange = function() {\n    if (this.readyState == 4 && this.status == 200) {\n      document.getElementById('d').innerHTML='';\n      var r = JSON.parse(this.responseText);\n      for(let i =0;i<r.length;i+=3){\n      ins(r[i],r[i+1],r[i+2]);\n      }\n    }\n  };\n  xhr.open(\"GET\", \"/musicfile?fn=\"+element.value, true);\n  xhr.send();\n  document.getElementById(\"fn\").value=element.value;\n}\nfunction save() {\n  if(document.getElementById(\"fn\").value!=\"\")\n  {\n    var array = document.getElementById('d').getElementsByTagName(\"select\");\n    var s=[];\n    for(var i = 0;i<array.length;i++)\n    {\n    s.push(new Number(array[i].value));\n    }\n    var xhr = new XMLHttpRequest();\n    xhr.open(\"POST\", \"/musicupdate?fn=\"+document.getElementById(\"fn\").value+\"&json=\"+JSON.stringify(s), true);\n    xhr.send();\n    \n    var x = document.getElementById(\"file\");\n    var c=false;\n    if(document.getElementById(\"file\").hasChildNodes()){\n    for(i in x.children){\n    if(x[i].value==document.getElementById(\"fn\").value)c=true;\n    }\n    }\n    if(!c)x.appendChild(new Option(document.getElementById(\"fn\").value));\n  }\n    else alert(\"File name required!\");\n}\n\nfunction createnote(n, o, t){\n    var div = document.createElement(\"div\");\n\tvar table = document.createElement(\"table\");\n\tvar tr1 = document.createElement(\"tr\");\n\tvar tr2 = document.createElement(\"tr\");\n\tvar tr3 = document.createElement(\"tr\");\n\tvar td1 = document.createElement(\"td\");\n\tvar td2 = document.createElement(\"td\");\n\tvar td3 = document.createElement(\"td\");\n\tvar td4 = document.createElement(\"td\");\n\tvar close = document.createElement(\"b\");\n\tvar noteselect = document.createElement(\"select\");\n\tvar octaveselect = document.createElement(\"select\");\n\tvar timeselect = document.createElement(\"select\");\n\tvar insert = document.createElement(\"button\");\n\tvar notes= [\"C\",\"C#\",\"D\",\"D#\",\"E\",\"F\",\"F#\",\"G\",\"G#\",\"A\",\"A#\",\"B\",\"Rest\"];\n\tvar time= [\"1\",\"1/2\",\"1/4\",\"1/8\",\"1/16\"];\n    \n\tfor(let i=0;i<notes.length;i++){\n\tnoteselect.appendChild(new Option(notes[i],i+1));\n\t}\n\tfor(let i = 1;i<=8;i++){\n\toctaveselect.appendChild(new Option(i));\n\t}\n\tfor(let i=0;i<time.length;i++)\n\t{\n\ttimeselect.appendChild(new Option(time[i],i));\n\t}\n    \n    if(n>0) noteselect.children[n-1].setAttribute(\"selected\",\"true\");\n    if(o>0) octaveselect.children[o-1].setAttribute(\"selected\",\"true\");\n    else\n\toctaveselect.children[3].setAttribute(\"selected\",\"true\");\n    if(t>0) timeselect.children[t].setAttribute(\"selected\",\"true\");\n    else timeselect.children[1].setAttribute(\"selected\",\"true\");\n    \n    div.style.width=\"140px\";\n    div.style.display=\"inline-block\";\n    div.style.marginTop=\"10px\";\n    table.style.backgroundColor=\"#a3a3a3\";\n    table.style.display=\"inline-block\";\n    noteselect.style.userSelect=\"none\";\n\toctaveselect.style.userSelect=\"none\";\n\ttimeselect.style.userSelect=\"none\";\n\tclose.innerHTML=\"&times;\";\n\tclose.style.fontSize=\"25px\";\n\tclose.style.cursor=\"default\";\n\tclose.style.userSelect=\"none\";\n\tclose.onclick=function(){rem(this);};\n\tinsert.innerHTML=\"+\";\n    insert.style.float=\"right\";\n    insert.style.marginTop=\"33px\";\n\tinsert.onclick=function(){this.parentElement.insertAdjacentElement(\"afterend\",createnote(0,0,0))};\n    \n    td1.appendChild(noteselect);\n    td1.innerHTML+=\" Note\";\n    td2.appendChild(close);\n    tr1.appendChild(td1);\n    tr1.appendChild(td2);\n    \n    td3.appendChild(octaveselect);\n    td3.innerHTML+=\" Octave\";\n    tr2.appendChild(td3);\n    \n    td4.appendChild(timeselect);\n    td4.innerHTML+=\" Time\";\n    tr3.appendChild(td4);\n    \n    table.appendChild(tr1);\n    table.appendChild(tr2);\n    table.appendChild(tr3);\n    div.appendChild(table);\n    div.appendChild(insert);\n    return div;\n}\nfunction ins(n, o, t){\n\tdocument.getElementById(\"d\").appendChild(createnote(n, o, t));\n}\nfunction rem(element){\nelement.parentElement.parentElement.parentElement.parentElement.remove();\n}\n</script>\n</body>\n</html>"
  ;
  server.send(200, "text/html", music_html);
  sleeptimer=millis()+300000;
  }
void musicupdate(){
  json = JSON.parse(server.arg("json"));
#ifdef DEBUG
  Serial.println(json);
#endif
  if(LittleFS.exists("/m/"+server.arg("fn")))LittleFS.remove("/m/"+server.arg("fn"));
  File file = LittleFS.open("/m/"+server.arg("fn"), "w");
  file.print(json);
  file.close();
  sleeptimer=millis()+300000;
}
void musicfile(){
  musicstop();
  if(server.hasArg("del"))LittleFS.remove("/m/"+server.arg("fn"));
  else {
  File file = LittleFS.open("/m/"+server.arg("fn"), "r");
  String temp = file.readString();
  json=JSON.parse(temp);
  file.close();
  server.send(200, "text/plain", temp);
#ifdef DEBUG
  Serial.println(json);
#endif
  }
  sleeptimer=millis()+300000;
  }
void musicplay(){
  if(json.length()>2){
  noteindex=0;
  play=true;
  }
  sleeptimer=millis()+300000;
  }
void musicstop(){
  play=false;
  delay(100);
  sleeptimer=millis()+300000;
  }
void settingshtml(){
  File file = LittleFS.open("/ap", "r");
  String ap = file.readString();
  JSONVar apjson=JSON.parse(ap);
  ap="";
  if(LittleFS.exists("/ap"))
  for(int i=0;i<apjson.length();i+=2)
  {
   ap+="<tr><td class='ap' contenteditable>";
   ap+=(const char*)apjson[i];
   ap+="</td><td class='ap' contenteditable>";
   ap+=(const char*)apjson[i+1];
   ap+="</td><td><a style='cursor: default; user-select: none;' onclick='this.parentElement.parentElement.remove()'><strong>&times;</strong></a></td></tr>";
  }
  String settings_html ="<!DOCTYPE html>\n<html>\n<head>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<style>\nbody {\n  font-family: \"Lato\", sans-serif;\n  transition: background-color .5s;\n}\n.sidenav {\n  height: 100%;\n  width: 0;\n  position: fixed;\n  z-index: 1;\n  top: 0;\n  left: 0;\n  background-color: #111;\n  overflow-x: hidden;\n  transition: 0.5s;\n  padding-top: 60px;\n}\n\n.sidenav a {\n  padding: 8px 8px 8px 32px;\n  text-decoration: none;\n  font-size: 25px;\n  color: #818181;\n  display: block;\n  transition: 0.3s;\n}\n\n.sidenav a:hover {\n  color: #f1f1f1;\n}\n\n.sidenav .closebtn {\n  position: absolute;\n  top: 0;\n  right: 25px;\n  font-size: 36px;\n  margin-left: 50px;\n}\n\n#main {\n  transition: margin-left .5s;\n  padding: 16px;\n}\n\n@media screen and (max-height: 450px) {\n  .sidenav {padding-top: 15px;}\n  .sidenav a {font-size: 18px;}\n}\ninput[type='color'] {\nwidth:100%;\nheight:100%;\nbackground: #FFFFFF 0% 0% no-repeat padding-box;\nbox-shadow: 0px 3px 6px #0000001A;\ntransform: scale(1.5);\n}\n.ap {\nborder-style: solid;\nborder-width: 1px;\nwidth: 48%;\n}\n.style12 {\nwidth: 48%;\ntext-align: center;\nborder-left-style: none;\nborder-left-width: 2px;\nborder-right-style: none;\nborder-right-width: 2px;\nborder-top-style: none;\nborder-top-width: 2px;\nborder-bottom-style: solid;\nborder-bottom-width: 2px;\n}\n</style>\n</head>\n<body>\n\n<div id=\"mySidenav\" class=\"sidenav\">\n  <a href=\"javascript:void(0)\" class=\"closebtn\" onclick=\"closeNav()\">&times;</a>\n  <a href=\"/\">LED</a>\n  <a href=\"music\">Music</a>\n  <a href=\"settings\"><strong>Settings</strong></a>\n</div>\n\n<div id=\"main\" class=\"style1\">\n  <div style=\"text-align: left;\"><span style=\"font-size:30px;cursor:pointer\" onclick=\"openNav()\" class=\"style2\">&#9776;</span>\n  </div>\n  <div style=\"text-align: center;\">\n  <table style=\"border-collapse: collapse; width: 70%\" id=\"t\" align=\"center\" cellspacing=\"0\">\n<tr>\n<td class=\"style12\" ><strong>SSID</strong></td>\n<td class=\"style12\" ><strong>Password</strong></td>\n<td ></td>\n</tr>"
  +ap
  +"</table>\n<input name=\"Button1\" type=\"button\" style=\"margin:10px\" onclick=\"add()\" value=\"Add access point\">\n<input name=\"Button1\" type=\"button\" style=\"margin:10px\" onclick=\"save()\" value=\"Save and Restart\">\n<br>\n<label>\nCity: \n<input type=\"text\" id=\"c\" value=\""
  +city
  +"\">\n</label>\n<button onclick=\"cityupdate()\">Save</button>\n<br>\n<br>\n<button onclick=\"reboot()\">Reboot</button>\n</div>\n</div>\n<script>\n\t\t\t\nfunction openNav() {\n  document.getElementById(\"mySidenav\").style.width = \"250px\";\n  document.getElementById(\"main\").style.marginLeft = \"250px\";}\n\nfunction closeNav() {\n  document.getElementById(\"mySidenav\").style.width = \"0\";\n  document.getElementById(\"main\").style.marginLeft= \"0\";\n}\nfunction save(){\n  var ap=[];\n  var table =document.getElementById('t');\n  for (var r = 1; r < table.rows.length; r++) {\n      if(table.rows[r].cells[0].innerHTML!=\"\"){\n     ap.push(table.rows[r].cells[0].innerHTML);\n     ap.push(table.rows[r].cells[1].innerHTML.length>0?table.rows[r].cells[1].innerHTML:0);\n      }\n  } \n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/apupdate?json=\"+JSON.stringify(ap), true);\n  xhr.send();\n}\nfunction reboot(){\n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/reboot\", true);\n  xhr.send();\n}\nfunction add(){\nvar table =document.getElementById('t');\nvar row = table.insertRow();\nvar cell1 = row.insertCell();\nvar cell2 = row.insertCell();\nvar cell3 = row.insertCell();\ncell1.className=\"ap\";\ncell1.setAttribute(\"contenteditable\",\"true\");\ncell2.className=\"ap\";\ncell2.setAttribute(\"contenteditable\",\"true\");\ncell3.innerHTML=\"<a style='cursor: default; user-select: none;' onclick='this.parentElement.parentElement.remove()'><strong>&times;</strong></a>\";\n}\nfunction cityupdate(){\n  var xhr = new XMLHttpRequest();\n  xhr.open(\"POST\", \"/cityupdate?city=\"+document.getElementById('c').value, true);\n  xhr.send();\n}\n</script>\n</body>\n</html>"
  ;
  server.send(200, "text/html", settings_html);
  sleeptimer=millis()+300000;
  }
void apupdate(){
#ifdef DEBUG
  Serial.println(server.arg("json"));
#endif
  if(LittleFS.exists("/ap"))LittleFS.remove("/ap");
  File file = LittleFS.open("/ap", "w");
  file.print(server.arg("json"));
  file.close();
  ESP.restart();
  }
void cityupdate(){
  city=server.arg("city");
#ifdef DEBUG
  Serial.println(city);
#endif
  if(LittleFS.exists("/city"))LittleFS.remove("/city");
  File file = LittleFS.open("/city", "w");
  file.print(city);
  file.close();
  weathertimeout=0;//update display
  sleeptimer=millis()+300000;
}

bool getNTPtime(int sec) {
  {
    uint32_t start = millis();
    do {
      time(&now);
      localtime_r(&now, &timeinfo);
#ifdef DEBUG
      Serial.print(".");
#endif
      delay(10);
    } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
    if (timeinfo.tm_year <= (2016 - 1900)) return true;  // the NTP call was not successful
  }
  return false;
}
void getTimeReducedTraffic(int sec) {
  tm *ptm;
  if ((millis() - lastEntryTime) < (1000 * sec)) {
    now = lastNTPtime + (int)(millis() - lastEntryTime) / 1000;
  } else {
    lastEntryTime = millis();
    lastNTPtime = time(&now);
    now = lastNTPtime;
#ifdef DEBUG
    Serial.println("Get NTP time");
#endif
  }
  ptm = localtime(&now);
  timeinfo = *ptm;
}
void drawConnect(){
  u8g2.setFont(u8g2_font_helvB10_te);
  u8g2.setCursor(0,25);
  u8g2.print(F("Connecting to"));
  u8g2.setCursor(0,42);
  u8g2.print(F("Wifi..."));
  }
void drawClock(tm localTime,int x) {
#ifdef DEBUG
  Serial.print(localTime.tm_mday);
  Serial.print('/');
  Serial.print(localTime.tm_mon + 1);
  Serial.print('/');
  Serial.print(localTime.tm_year - 100+2000);
  Serial.print('-');
  Serial.print(localTime.tm_hour);
  Serial.print(':');
  Serial.print(localTime.tm_min);
  Serial.print(':');
  Serial.print(localTime.tm_sec);
  Serial.print(' ');
  Serial.println(weekDays[localTime.tm_wday]);
#endif
  
  u8g2.setFont(u8g2_font_logisoso20_tf);
  String hourstring=String(localTime.tm_hour);
  char hour[hourstring.length()+1];
  hourstring.toCharArray(hour,hourstring.length()+1);
  u8g2.setCursor(x+(localTime.tm_hour<10?47:32), 30);
  u8g2.print(hour);
  String minstring=localTime.tm_min<10?"0"+String(localTime.tm_min):String(localTime.tm_min);
  char minute[minstring.length()+1];
  minstring.toCharArray(minute,minstring.length()+1);
  u8g2.setCursor(x+68, 30);
  u8g2.print(minute);
  if(localTime.tm_sec%2==0){
    u8g2.drawDisc(x+64,15,1);
    u8g2.drawDisc(x+64,25,1);
  }
  u8g2.setFont(u8g2_font_helvB10_te);
  String daystring=weekDays[localTime.tm_wday];
  char day[daystring.length()+1];
  daystring.toCharArray(day,daystring.length()+1);
  u8g2.setCursor(x + (width - u8g2.getUTF8Width(day)) / 2, 50); //center
  u8g2.print(day);
  u8g2.setFont(u8g2_font_6x12_te);
  String datestring=String(localTime.tm_year - 100+2000)+"."+String(localTime.tm_mon + 1)+"."+String(localTime.tm_mday);
  char date[datestring.length()+1];
  datestring.toCharArray(date,datestring.length()+1);
  u8g2.setCursor(x, 64); //center
  u8g2.print(date);
  u8g2.setFont(u8g2_font_battery19_tn);
  u8g2.drawGlyph(x+118,64,map(constrain(analogRead(A0),750,1024),750,1024,48,53));
}

void drawWeather(int x) {
  if (weatherId <= THUNDER) {
    u8g2.drawBitmap(x + 10, 1, 4, 32, thunder);
  }
  else if (weatherId <= DRIZZLE) { //
    u8g2.drawBitmap(x + 10, 1, 4, 32, drizzle);
  }
  else if (weatherId <= RAIN) {
    u8g2.drawBitmap(x + 10, 1, 4, 32, rain);
  }
  else if (weatherId <= SNOW) { //
    u8g2.drawBitmap(x + 10, 1, 4, 32, snow);
  }
  else if (weatherId <= ATMOSPHERE) { //
    u8g2.drawBitmap(x + 10, 1, 4, 32, atmosphere);
  }
  else if (weatherId == SUN) {
    u8g2.drawBitmap(x + 10, 1, 4, 32, sun);
  }
  else if (weatherId == SUN_CLOUD) {
    u8g2.drawBitmap(x + 10, 1, 4, 32, sun_cloud);
  }
  else if (weatherId <= CLOUD) {
    u8g2.drawBitmap(x + 10, 1, 4, 32, cloud);
  }
  u8g2.setFont(u8g2_font_logisoso16_tf);
  u8g2.setCursor(x + 40, 25);
  u8g2.print(weatherTemperature);
  u8g2.print("°C");   // requires enableUTF8Print()

  u8g2.setFont(u8g2_font_helvB08_te);
  char description[weatherDescription.length() + 1];
  weatherDescription.toCharArray(description, weatherDescription.length() + 1);
  u8g2.setCursor(x + (width - u8g2.getUTF8Width(description)) / 2, 45); //center
  u8g2.print(weatherDescription);
  
  u8g2.setFont(u8g2_font_helvB08_te);
  u8g2.setCursor(x , 64); //center
  String temp="Indoor temp: "+String(tempC)+"°C";
  u8g2.print(temp);
}

void drawWiFi(int x){
  u8g2.setFont(u8g2_font_helvB10_te);
  u8g2.setCursor(x + (width - u8g2.getUTF8Width("WiFi")) / 2,15);
  u8g2.print("WiFi");
  char ssid[WiFi.SSID().length()+1];
  WiFi.SSID().toCharArray(ssid,WiFi.SSID().length()+1);
  u8g2.setCursor(x + (width - u8g2.getUTF8Width(ssid)) / 2,30);
  u8g2.print(ssid);
  u8g2.setCursor(x + (width - u8g2.getUTF8Width("IP")) / 2,48);
  u8g2.print("IP");
  String i=(wifi_get_opmode()==WIFI_STA?WiFi.localIP():WiFi.softAPIP()).toString();
  char ip[i.length()+1];
  i.toCharArray(ip,i.length()+1);
  u8g2.setCursor(x + (width - u8g2.getUTF8Width(ip)) / 2,64);
  u8g2.print(ip);
  for(int i=x;i<x+128;i++)u8g2.drawPixel(i,32);
}
void updateWeather()
{
  // if there's a successful connection:
  if (client.connect("api.openweathermap.org", 80))
  { 
#ifdef DEBUG
    Serial.println("Connecting to OpenWeatherMap server...");
#endif
    // send the HTTP PUT request:
    client.println("GET /data/2.5/weather?q="+city+"&units=metric&APPID=e73e41373db6ddd280f289a6d8f6d5e6 HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();

    // Check HTTP status
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0)
    {
#ifdef DEBUG
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
#endif
      return;
    }

    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders))
    {
#ifdef DEBUG
      Serial.println(F("Invalid response"));
#endif
      return;
    }

    // Allocate the JSON document
    // Use arduinojson.org/v6/assistant to compute the capacity.
    DynamicJsonDocument doc(1024);

    // Parse JSON object
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
#ifdef DEBUG
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
#endif
      return;
    }

    weatherId = doc["weather"][0]["id"].as<int>();
    weatherTemperature = doc["main"]["temp"].as<float>();
    weatherHumidity = doc["main"]["humidity"].as<int>();
    weatherDescription = doc["weather"][0]["description"].as<String>();
    //Disconnect
    client.stop();

#ifdef DEBUG
    Serial.println(F("Response:"));
    Serial.print("Weather: ");
    Serial.println(weatherId);
    Serial.print("Temperature: ");
    Serial.println(weatherTemperature);
    Serial.print("Humidity: ");
    Serial.println(weatherHumidity);
    Serial.println();
#endif
    char scrollText[15];
    sprintf(scrollText, "Humidity:%3d%%", weatherHumidity);

    // note the time that this function was called
    lastCallTime = millis();
  }
}
