/*waitTime
 * 
 * JuanFi v2.1
 * 
 * PisoWifi coinslot system with integration to Mikrotik Hotspot, 
 * Using
 * 
 * Features
 * 
 * Coinslot System
 *    -Mikrotik integration
 *    -Pause expiration
 *    -Codeless generation
 * 
 * Admin System
 *   - Initial setup of the system
 *   - Mikrotik connection setup, SSID setup, coinslot settings
 *   - Promo Rates configuration ( Rates, expiration)
 *   - Dashboard, Sales report
 * 
 * Supported ESP32 Lanbase and ESP8266 
 * 
 * Created by Ivan Julius Alayan
 * 
*/

#ifdef ESP32
  #include <TelnetClient.h>
  #include "lan_definition.h"
  #include <SPIFFS.h>
#else
  #include <ESP8266TelnetClient.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266WiFiMulti.h>
  #include <ESP8266HTTPClient.h>
  #include <ESP8266mDNS.h>
  #include <DNSServer.h>
#endif


#include <EEPROM.h>
#include "FS.h"
#include <base64.h>
#include <LiquidCrystal_I2C.h>

int TURN_OFF = 0;
int TURN_ON = 1;


LiquidCrystal_I2C lcd(0x27, 16, 2);
LiquidCrystal_I2C lcd20x4(0x27, 20, 4);

volatile int coin = 0;
volatile int totalCoin = 0;
boolean isNewVoucher = false;
int coinsChange = 0;
String currentActiveVoucher = "";
String currentMacAttempt = "";
int timeToAdd = 0;
bool coinSlotActive = false;
bool acceptCoin = false;
unsigned long targetMilis = 0;
bool coinExpired = false;
bool mikrotekConnectionSuccess = false;
String currentMacAddress = "";
String currentIpAddress = "";
#ifdef ESP32
  String HARDWARE_TYPE = "ESP32";
#else
  String HARDWARE_TYPE = "ESP8266";
#endif

typedef struct {
  String rateName;
  int price;
  int minutes;
  int validity;
} PromoRates;

typedef struct {
  String mac;
  long unlockTime;
  int attemptCount;
} AttemptMacAddress;

int attemptedMaxCount = 20;
AttemptMacAddress attempted[20];
PromoRates rates[100];
int ratesCount = 0;
int currentValidity = 0;

const int LIFETIME_COIN_COUNT_ADDRESS = 0;
const int COIN_COUNT_ADDRESS = 5;
const int CUSTOMER_COUNT_ADDRESS = 10;
const int RANDOM_MAC_ADDRESS = 15;

void ICACHE_RAM_ATTR coinInserted()    
{
  if(coinSlotActive){
    coin = coin + 1;  
    coinsChange = 1;
  }
}


int COIN_SELECTOR_PIN = 0;
int COIN_SET_PIN = 0;
int INSERT_COIN_LED = 0;
int SYSTEM_READY_LED = 0;
int INSERT_COIN_BTN_PIN = 0;
int CHECK_INTERNET_CONNECTION = 0;
int LED_TRIGGER_TYPE = 1;
int VOUCHER_LOGIN_OPTION = 0;
int VOUCHER_VALIDITY_OPTION = 0;
String VOUCHER_PROFILE = "default";
String VOUCHER_PREFIX = "P";

int MAX_WAIT_COIN_SEC = 30000;
int COINSLOT_BAN_COUNT = 0;
int COINSLOT_BAN_MINUTES = 0;
int LCD_TYPE = 0;
int macRandomIndex = 0;


//put here your raspi ip address, and login details
IPAddress mikrotikRouterIp (10, 0, 0, 1);
String user = "pisonet";
String pwd = "abc123";
String ssid     = "MikrofffffTik-36DA2B";
String password = "";
String adminAuth = "";
String vendorName = "";


IPAddress apIP(172, 217, 28, 1);

#ifdef ESP32
  EthernetWebServer server(80);
  EthernetClient client;
  EthernetClient client2;
  telnetClient tc(client);
#else
  ESP8266WiFiMulti WiFiMulti;
  WiFiClient client2;
  WiFiClient client;
  ESP8266telnetClient tc(client);
  ESP8266WebServer server(80);
  const byte DNS_PORT = 53;
  DNSServer dnsServer;
#endif



const int WIFI_CONNECT_TIMEOUT = 45000;
const int WIFI_CONNECT_DELAY = 500;

bool networkConnected = false;
bool cableNotConnected = false;
bool welcomePrinted = false;
bool manualVoucher = false;

int lastSaleTime = 0;
int thankyou_cooldown = 5000;
long lastPrinted = 0;

String MARQUEE_MESSAGE = "This is marquee";

void heartBeatPrint()
{
  static int num = 1;

  Serial.print(F("H"));        // H means alive

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong current_millis;

#define HEARTBEAT_INTERVAL    10000L

  current_millis = millis();
  
  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((current_millis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = current_millis + HEARTBEAT_INTERVAL;
  }
}

void setup () { 
                                
  Serial.begin (115200);
  EEPROM.begin(32);
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }  
  populateSystemConfiguration(); 
  
  pinMode(COIN_SELECTOR_PIN, INPUT_PULLUP);
  pinMode(INSERT_COIN_LED, OUTPUT);
  pinMode(SYSTEM_READY_LED, OUTPUT);
  pinMode(COIN_SET_PIN, OUTPUT);
  pinMode(INSERT_COIN_BTN_PIN, INPUT_PULLUP);

  #ifdef ESP32
    initializeLANSetup();
    initializeLCD();
  #else
    initializeLCD();
    // We start by connecting to a WiFi network
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(ssid.c_str(), password.c_str());
    Serial.println();
    Serial.println();
    Serial.print("Wait for WiFi, connecting to ");
    Serial.print(ssid);
  
    int second = 0;
    while (second <= WIFI_CONNECT_TIMEOUT) {
      networkConnected = (WiFiMulti.run() == WL_CONNECTED);
      Serial.print(".");
      if(networkConnected){
        break;
      }
      digitalWrite(SYSTEM_READY_LED, evaluateTriggerOutput(TURN_OFF));
      delay(WIFI_CONNECT_DELAY);
      digitalWrite(SYSTEM_READY_LED, evaluateTriggerOutput(TURN_ON));
      second += WIFI_CONNECT_DELAY;
    }
    currentIpAddress = WiFi.localIP().toString().c_str();
    currentMacAddress = WiFi.macAddress();
    digitalWrite(SYSTEM_READY_LED, evaluateTriggerOutput(TURN_OFF));
  #endif
  
  
  if(networkConnected){
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(currentIpAddress);
    Serial.print("Mac address: ");
    Serial.println(currentMacAddress);
    Serial.println("Connecting.... ");
    digitalWrite(INSERT_COIN_LED, evaluateTriggerOutput(TURN_OFF));
    digitalWrite(SYSTEM_READY_LED, evaluateTriggerOutput(TURN_OFF));
    digitalWrite(COIN_SET_PIN, LOW);
    Serial.print("Attaching interrupt ");
    attachInterrupt(COIN_SELECTOR_PIN, coinInserted, RISING);
    loginMirotik();
   
    #ifdef ESP32
      //nothing
    #else
      if (MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
      }
    #endif

    server.on("/topUp", topUp);
    server.on("/checkCoin", checkCoin);
    server.on("/useVoucher", useVoucher);
    server.on("/health", handleHealth);
    server.on("/getRates", handleUserGetRates);
    server.on("/testInsertCoin", testInsertCoin);
    server.onNotFound(handleNotFound);
    printWelcome();
    welcomePrinted = true;
    
  }else{
    #ifdef ESP32
      //nothing
    #else
      //Soft AP setup
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP("JuanFi Setup");
      //if DNSServer is started with "*" for domain name, it will reply with
      //provided IP to all DNS request
      dnsServer.start(DNS_PORT, "*", apIP);
    #endif

    server.onNotFound([]() {
      server.sendHeader("Location", String("/admin"), true);
      server.send ( 302, "text/plain", "");
    });
    if(LCD_TYPE > 0){
      if(LCD_TYPE == 1){
        lcd.clear();
        if(cableNotConnected){
          lcd.setCursor(0, 0);
          lcd.print("Cable");
          lcd.setCursor(0, 1);
          lcd.print("Not connected");
        }else{
          lcd.setCursor(0, 0);
          lcd.print("Initial Setup");
          lcd.setCursor(0, 1);
          lcd.print("IP: 172.217.28.1");
        }
      }else if(LCD_TYPE == 2){
        lcd20x4.clear();
        lcd20x4.setCursor(0, 0);
        lcd20x4.print("JuanFi");
        if(cableNotConnected){
          lcd20x4.setCursor(0, 1);
          lcd20x4.print("Cable not connected");
          lcd20x4.setCursor(0, 2);
          lcd20x4.print("Pls check");
          lcd20x4.setCursor(0, 3);
          lcd20x4.print("connections");
        }else{
          lcd20x4.setCursor(0, 1);
          lcd20x4.print("Initial Setup");
          lcd20x4.setCursor(0, 2);
          lcd20x4.print("IP: 172.217.28.1");
        }
      }
    }
  }
  
  server.on("/admin/api/dashboard", handleAdminDashboard);
  server.on("/admin/js/jquery.min.js", handleJquerySript);
  server.on("/admin/api/resetStatistic", handleAdminResetStats);
  server.on("/admin/api/saveSystemConfig", handleAdminSaveSystemConfig);
  server.on("/admin/api/getSystemConfig", handleAdminGetSystemConfig);
  server.on("/admin/api/getRates", handleAdminGetRates);
  server.on("/admin/api/saveRates", handleAdminSaveRates);
  server.on("/admin/api/logout", handleLogout);
  server.on("/admin", handleAdminPage);

  populateRates();
  
  server.begin();
  
  if(mikrotekConnectionSuccess){
    digitalWrite(SYSTEM_READY_LED, evaluateTriggerOutput(TURN_ON));
  }

}

#ifdef ESP32
void initializeLANSetup(){
  delay(3000);
  Serial.print("\nStarting ESP32_FS_EthernetWebServer on " + String(BOARD_TYPE));
  Serial.println(" with " + String(SHIELD_TYPE));
  Serial.println(ETHERNET_WEBSERVER_VERSION);

  #if USE_ETHERNET_WRAPPER
  
    EthernetInit();
  
  #else

  #if USE_NATIVE_ETHERNET
    ET_LOGWARN(F("======== USE_NATIVE_ETHERNET ========"));
  #elif USE_ETHERNET
    ET_LOGWARN(F("=========== USE_ETHERNET ==========="));
  #elif USE_ETHERNET2
    ET_LOGWARN(F("=========== USE_ETHERNET2 ==========="));
  #elif USE_ETHERNET3
    ET_LOGWARN(F("=========== USE_ETHERNET3 ==========="));
  #elif USE_ETHERNET_LARGE
    ET_LOGWARN(F("=========== USE_ETHERNET_LARGE ==========="));
  #elif USE_ETHERNET_ESP8266
    ET_LOGWARN(F("=========== USE_ETHERNET_ESP8266 ==========="));
  #elif USE_ETHERNET_ENC
    ET_LOGWARN(F("=========== USE_ETHERNET_ENC ==========="));
  #else
    ET_LOGWARN(F("========================="));
  #endif

  ET_LOGWARN(F("Default SPI pinout:"));
  ET_LOGWARN1(F("MOSI:"), MOSI);
  ET_LOGWARN1(F("MISO:"), MISO);
  ET_LOGWARN1(F("SCK:"),  SCK);
  ET_LOGWARN1(F("SS:"),   SS);
  ET_LOGWARN(F("========================="));

  // You can use Ethernet.init(pin) to configure the CS pin
  //Ethernet.init(10);  // Most Arduino shields
  //Ethernet.init(5);   // MKR ETH shield
  //Ethernet.init(0);   // Teensy 2.0
  //Ethernet.init(20);  // Teensy++ 2.0
  //Ethernet.init(15);  // ESP8266 with Adafruit Featherwing Ethernet
  //Ethernet.init(33);  // ESP32 with Adafruit Featherwing Ethernet

  #ifndef USE_THIS_SS_PIN
    #define USE_THIS_SS_PIN   5   //22    // For ESP32
  #endif

  ET_LOGWARN1(F("ESP32 setCsPin:"), USE_THIS_SS_PIN);

  // For other boards, to change if necessary
  #if ( USE_ETHERNET || USE_ETHERNET_LARGE || USE_ETHERNET2 || USE_ETHERNET_ENC )
    // Must use library patch for Ethernet, EthernetLarge libraries
    // ESP32 => GPIO2,4,5,13,15,21,22 OK with Ethernet, Ethernet2, EthernetLarge
    // ESP32 => GPIO2,4,5,15,21,22 OK with Ethernet3
  
    //Ethernet.setCsPin (USE_THIS_SS_PIN);
    Ethernet.init (USE_THIS_SS_PIN);
  
  #elif USE_ETHERNET3
    // Use  MAX_SOCK_NUM = 4 for 4K, 2 for 8K, 1 for 16K RX/TX buffer
  #ifndef ETHERNET3_MAX_SOCK_NUM
  #define ETHERNET3_MAX_SOCK_NUM      4
  #endif
  
    Ethernet.setCsPin (USE_THIS_SS_PIN);
    Ethernet.init (ETHERNET3_MAX_SOCK_NUM);
  
  #elif USE_CUSTOM_ETHERNET
  
    // You have to add initialization for your Custom Ethernet here
    // This is just an example to setCSPin to USE_THIS_SS_PIN, and can be not correct and enough
    Ethernet.init(USE_THIS_SS_PIN);
  
  #endif  //( USE_ETHERNET || USE_ETHERNET2 || USE_ETHERNET3 || USE_ETHERNET_LARGE )

  #endif  //USE_ETHERNET_WRAPPER
     // start the ethernet connection and the server:
    Serial.println("Ethernet initialized...");

    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Cable not detected!!!");
      networkConnected = false;
      cableNotConnected = true;
    }else if(Ethernet.begin(mac[macRandomIndex]) != 0){
      networkConnected = true;
    }else{
      networkConnected = false;
      Serial.println("Cannot connect to dhcp server");
      Ethernet.begin(mac[macRandomIndex], apIP, apIP, apIP, IPAddress(255, 255, 255, 0));
    }
    // Just info to know how to connect correctly
    Serial.println(F("========================="));
    Serial.println(F("Currently Used SPI pinout:"));
    Serial.print(F("MOSI:"));
    Serial.println(MOSI);
    Serial.print(F("MISO:"));
    Serial.println(MISO);
    Serial.print(F("SCK:"));
    Serial.println(SCK);
    Serial.print(F("SS:"));
    Serial.println(SS);
  #if USE_ETHERNET3
    Serial.print(F("SPI_CS:"));
    Serial.println(SPI_CS);
  #endif
    Serial.println("=========================");
  
    Serial.print(F("Connected! IP address: "));
    Serial.println(Ethernet.localIP());

    currentIpAddress = Ethernet.localIP().toString().c_str();
    char str[12] = "";
    array_to_string(mac[macRandomIndex], 6, str);
    for(int i=0;i<12;i++){
      if(i > 0 && i%2 == 0){
        currentMacAddress += ":";
      }
      currentMacAddress += str[i];
    }
}
#endif

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

void initializeLCD(){
  if(LCD_TYPE > 0){
     if(LCD_TYPE == 1){
       lcd.init();   // initializing the LCD
       lcd.backlight(); // Enable or Turn On the backlight 
       lcd.print("Initializing.."); 
     }else if(LCD_TYPE == 2){
       lcd20x4.init();   // initializing the LCD
       lcd20x4.backlight(); // Enable or Turn On the backlight 
       lcd20x4.print("Initializing.."); 
     }
  }
}

void handleNotFound()
{
    Serial.println("preflight....");
    if (server.method() == HTTP_OPTIONS)
    {
        Serial.println("Preflight request....");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Max-Age", "10000");
        server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "*");
        server.sendHeader("Access-Control-Allow-Credentials", "false");
        server.send(204);
    }
    else
    {
        server.send(404, "text/plain", "");
    }
}

void handleHealth(){
  setupCORSPolicy();
  server.send ( 200, "text/plain", "ok");
}

void handleLogout(){
  server.sendHeader("WWW-Authenticate", "Basic realm=\"Secure\"");
  server.send(401, "text/html", "<html>Authentication failed</html>");
}

void loginMirotik(){

   //WHICH CHARACTER SHOULD BE INTERPRETED AS "PROMPT"?
    tc.setPromptChar('>');

    //this is to trigger manually the login 
    //since it could be a problem to attach the serial monitor while negotiating with the server (it cause the board reset)
    //remove it or replace it with a delay/wait of a digital input in case you're not using the serial monitors
    Serial.print("Logging in to mikrotik ");
    Serial.print(mikrotikRouterIp);
    Serial.print(" using ");
    Serial.print(user);
    Serial.print(" / ");
    Serial.println(pwd);
    delay(3000);
  
    //PUT HERE YOUR USERNAME/PASSWORD
    mikrotekConnectionSuccess = tc.login(mikrotikRouterIp, user.c_str(), pwd.c_str());
    if(mikrotekConnectionSuccess){
      Serial.println("Login to mikrotek router success");
    }else{
      //Temporary fix for those cannot connect to mikrotik
      mikrotekConnectionSuccess = true;
      Serial.println("Warning, Failed to login in mikrotek router, please check mikrotik log");
    }
}

void testInsertCoin(){
  if(!isAuthorized()){
     handleNotAuthorize();
     return;
  }  
  String data = server.arg("coin");
  if(coinSlotActive){
    coin = data.toInt();  
    coinsChange = 1;
  }
  server.send(200, "text/plain", "ok");
}

void eeWriteInt(int pos, int val) {
    byte* p = (byte*) &val;
    EEPROM.write(pos, *p);
    EEPROM.write(pos + 1, *(p + 1));
    EEPROM.write(pos + 2, *(p + 2));
    EEPROM.write(pos + 3, *(p + 3));
    EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  if( val < 0){
    return 0;
  }else{
    return val;
  }
}

void handleJquerySript(){
  handleFileRead("/admin/js/jquery.min.js");
}

void handleUserGetRates(){
  setupCORSPolicy();
  handleFileRead("/admin/config/rates.data");
}

void handleAdminGetRates(){
  if(!isAuthorized()){
     handleNotAuthorize();
     return;
  }  
  handleFileRead("/admin/config/rates.data");
}

void handleAdminSaveRates(){
  if(!isAuthorized()){
     handleNotAuthorize();
     return;
  }
  
  String data = server.arg("data");
  handleFileWrite("/admin/config/rates.data", data);
  populateRates();
  server.send(200, "text/plain", "ok");
}

void handleAdminSaveSystemConfig(){
  if(!isAuthorized()){
     handleNotAuthorize();
     return;
  }
  
  String data = server.arg("data");
  handleFileWrite("/admin/config/system.data", data);
  server.send(200, "text/plain", "ok");
  delay(2000);
  ESP.restart();
}

void handleAdminGetSystemConfig(){
  if(!isAuthorized()){
     handleNotAuthorize();
     return;
  }
  
  handleFileRead("/admin/config/system.data");
}

void handleAdminResetStats(){
   if(!isAuthorized()){
     handleNotAuthorize();
     return;
   }
  
   String type = server.arg("type");
   if(type == "lifeTimeCount"){
    eeWriteInt(LIFETIME_COIN_COUNT_ADDRESS, 0);
   }else if(type == "coinCount"){
    eeWriteInt(COIN_COUNT_ADDRESS, 0);
   }else if(type == "customerCount"){
    eeWriteInt(CUSTOMER_COUNT_ADDRESS, 0);
   }
  server.send(200, "text/plain", "ok");
}

void handleAdminDashboard(){
  if(!isAuthorized()){
    handleNotAuthorize();
    return;
  }
  
  long upTime = millis();
  int lifeTimeCoinCount = eeGetInt(LIFETIME_COIN_COUNT_ADDRESS);
  int coinCount = eeGetInt(COIN_COUNT_ADDRESS);
  int customerCount = eeGetInt(CUSTOMER_COUNT_ADDRESS);
  bool hasInternetConnection = hasInternetConnect();
  String data = "";
         data += String(upTime);
         data += String("|");
         data += String(lifeTimeCoinCount);
         data += String("|");
         data += String(coinCount);
         data += String("|");
         data += String(customerCount);
         data += String("|");
         if(hasInternetConnection){
          data += String("1");
         }else{
          data += String("0");
         }
         data += String("|");
         if(mikrotekConnectionSuccess){
          data += String("1");
         }else{
          data += String("0");
         }
         data += String("|");
         data += currentMacAddress;
         data += String("|");
         data += currentIpAddress;
         data += String("|");
         data += HARDWARE_TYPE;
         
  server.send(200, "text/plain", data);
}

void handleAdminPage(){
  if(!isAuthorized()){
    handleNotAuthorize();
    return;
  }
  
  handleFileRead("/admin/system-config.html");
}

bool isAuthorized(){
  String auth = server.header("Authorization");
  String expectedAuth = "Basic "+adminAuth;
  if(auth != expectedAuth){
    Serial.print("Admin incorrect: ");
    Serial.print(auth);
    Serial.print(" vs ");
    Serial.println(expectedAuth);
  }
  return auth == expectedAuth;
}

void handleNotAuthorize(){
  server.sendHeader("WWW-Authenticate", "Basic realm=\"Secure\"");
  server.send(401, "text/html", "<html>Authentication failed</html>");
}

bool handleFileRead(String path){  // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(SPIFFS.exists(pathWithGz))                          // If there's a compressed version available
      path += ".gz";                                         // Use the compressed version
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

bool handleFileWrite(String path, String content){  // send the right file to the client (if it exists)
  Serial.println("handleFileWrite: " + path);
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "w");
    int bytesWritten = file.print(content);
    if(bytesWritten <= 0){
      return false;
    }
    file.close();
    Serial.println(String("\Write file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

String readFile(String path){ 
  String result;
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "r");
    String content = file.readStringUntil('\n');
    file.close();
    return content;
  }
  return result;
}

String getContentType(String filename){
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool checkIfSystemIsAvailable(){
  if(!mikrotekConnectionSuccess){
    char * keys[] = {"status", "errorCode"};
    char * values[] = {"false", "coin.slot.notavailable"};
    setupCORSPolicy();
    server.send(200, "application/json", toJson(keys, values, 2));
    return false;
  }else{
    return true;
  }
}



#ifdef ESP32
  char internetServerAddress[] = "ifconfig.me";  // server address
  int internetCheckPort = 80;
  EthernetHttpClient  httpClient(client2, internetServerAddress, internetCheckPort);
#else
  String INTERNET_CHECK_URL = "http://ifconfig.me";
#endif;

bool hasInternetConnect(){

    #ifdef ESP32
      httpClient.get("/");
      int statusCode = httpClient.responseStatusCode();
      String response = httpClient.responseBody();
      Serial.print("Status code: ");
      Serial.println(statusCode);
      Serial.print("Response: ");
      Serial.println(response);
      return true;
    #else
      HTTPClient http;  
  
      http.begin(client2, INTERNET_CHECK_URL); //HTTP
      http.addHeader("User-Agent", "curl/7.55.1");
      int httpCode = http.GET();
      if (httpCode > 0) {
        const String& payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
        Serial.println("Internet connection detected!");
        http.end();
        return true;
      }else{
        Serial.println("Internet connection not detected!");
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
      }
    #endif;
}

void addAttemptToCoinslot(){
  if(COINSLOT_BAN_COUNT > 0 && (!manualVoucher)){
    int currentMacIndex = -1;
    int availableIndex = -1;
    for(int i=0;i<attemptedMaxCount;i++){
      if (attempted[i].mac == currentMacAttempt){
          currentMacIndex = i;
          break;
      }else if(attempted[i].mac == ""){
        availableIndex = i;
      }
    }
     Serial.println(currentMacAttempt);
     Serial.println(currentMacIndex);
     Serial.println(availableIndex);
    if(currentMacIndex > -1){
      attempted[currentMacIndex].attemptCount++;
     
      if(attempted[currentMacIndex].attemptCount >= COINSLOT_BAN_COUNT){
          long curMil = millis();
          attempted[currentMacIndex].unlockTime = curMil + (COINSLOT_BAN_MINUTES * 60000);
          Serial.print("Unlock time: ");
          Serial.println(attempted[currentMacIndex].unlockTime);
      }
    }else{
      if(availableIndex > -1){
        attempted[availableIndex].mac = currentMacAttempt;
        attempted[availableIndex].attemptCount++ ;
      }
    }
  }
}

void clearAttemptToCoinSlot(){
  if(COINSLOT_BAN_COUNT > 0){
    for(int i=0;i<attemptedMaxCount;i++){
        if (attempted[i].mac == currentMacAttempt){
            attempted[i].mac = "";
            attempted[i].unlockTime = 0;
            attempted[i].attemptCount = 0;
            break;
        }
    }
  }
}

void checkCoin(){

  if(!checkIfSystemIsAvailable()){
      return;
  }
  
  String voucher = server.arg("voucher");
  if(!validateVoucher(voucher)){
      return;
  }

  if(coinExpired){
    char * keys[] = {"status", "errorCode"};
    char * values[] = {"false", "coins.wait.expired"};
    setupCORSPolicy();
    server.send(200, "application/json", toJson(keys, values, 2));
    return;
  }

  if(!acceptCoin){
    totalCoin += coin;
    timeToAdd = calculateAddTime();
    char * keys[] = {"status", "newCoin", "timeAdded", "totalCoin", "validity"};
    char coinStr[16];
    itoa(coin, coinStr, 10);
    char timeToAddStr[16];
    itoa(timeToAdd, timeToAddStr, 10);
    char totalCoinStr[16];
    itoa(totalCoin, totalCoinStr, 10);
    char validityStr[16];
    itoa(currentValidity, validityStr, 10);
    char * values[] = {"true", coinStr, timeToAddStr, totalCoinStr, validityStr};
    activateCoinSlot();
    setupCORSPolicy();
    server.send(200, "application/json", toJson(keys, values, 5));
  }else{
    char * keys[] = {"status", "errorCode", "remainTime", "timeAdded", "totalCoin", "waitTime", "validity"};
    char remainTimeStr[20];
    long remain = targetMilis - millis();
    itoa(remain, remainTimeStr, 10);
    char timeToAddStr[16];
    itoa(timeToAdd, timeToAddStr, 10);
    char totalCoinStr[16];
    itoa(totalCoin, totalCoinStr, 10);
    char waitTimeStr[16];
    itoa(MAX_WAIT_COIN_SEC, waitTimeStr, 10);
    char validityStr[16];
    itoa(currentValidity, validityStr, 10);
    char * values[] = {"false", "coin.not.inserted", remainTimeStr, timeToAddStr, totalCoinStr, waitTimeStr, validityStr};
    setupCORSPolicy();
    server.send(200, "application/json", toJson(keys, values, 7));
  }
}

void useVoucher(){

  if(!checkIfSystemIsAvailable()){
      return;
  }

  String voucher = server.arg("voucher");
  if(!validateVoucher(voucher)){
      return;
  }
  disableCoinSlot();
  if(timeToAdd > 0 ){
    clearAttemptToCoinSlot();
    //if(isNewVoucher){
      registerNewVoucher(voucher);
    //}
    updateStatistic();
    addTimeToVoucher(voucher, timeToAdd);
  }else{
    addAttemptToCoinslot();
  }
  char * keys[] = {"status", "totalCoin", "timeAdded", "validity"};
  char totalCoinStr[16];
  itoa(totalCoin, totalCoinStr, 10);
  char timeToAddStr[16];
  itoa(timeToAdd, timeToAddStr, 10);
  char validityStr[16];
  itoa(currentValidity, validityStr, 10);
  char * values[] = {"true", totalCoinStr, timeToAddStr, validityStr};
  printThankYou();
  resetGlobalVariables();
  setupCORSPolicy();
  acceptCoin = false;
  server.send(200, "application/json", toJson(keys, values, 4));
}

void updateStatistic(){
    int lifeTimeCoinCount = eeGetInt(LIFETIME_COIN_COUNT_ADDRESS);
    lifeTimeCoinCount += totalCoin;
    eeWriteInt(LIFETIME_COIN_COUNT_ADDRESS, lifeTimeCoinCount);
    int coinCount = eeGetInt(COIN_COUNT_ADDRESS);
    coinCount += totalCoin;
    eeWriteInt(COIN_COUNT_ADDRESS, coinCount);
    int customerCount = eeGetInt(CUSTOMER_COUNT_ADDRESS);
    customerCount++;
    eeWriteInt(CUSTOMER_COUNT_ADDRESS, customerCount); 
 }

bool validateVoucher(String voucher){
  if(voucher != currentActiveVoucher){
      char * keys[] = {"status", "errorCode"};
      char * values[] = {"false", "coinslot.busy"};
      setupCORSPolicy();
      server.send(200, "application/json", toJson(keys, values, 2));
      return false;
  }else{
      return true;
  }
}

void topUp() {
  manualVoucher = false;
  thankyou_cooldown = 5000;
  bool hasInternetConnection = true;
  if(CHECK_INTERNET_CONNECTION == 1){
        hasInternetConnection = hasInternetConnect();
  }
  if(!hasInternetConnection){
    char * keys[] = {"status", "errorCode"};
    char * values[] = {"false", "no.internet.detected"};
    setupCORSPolicy();
    server.send(200, "application/json", toJson(keys, values, 2));
    return;
  }

  if(!checkIfSystemIsAvailable()){
      return;
  }

  String macAdd = server.arg("mac");
  if(!checkMacAddress(macAdd)){
    char * keys[] = {"status", "errorCode"};
    char * values[] = {"false", "coin.slot.banned"};
    setupCORSPolicy();
    server.send(200, "application/json", toJson(keys, values, 2));
    return;
  }
  currentMacAttempt = macAdd;
  String voucher = server.arg("voucher");
   if(currentActiveVoucher != "" && !validateVoucher(voucher)){
      return;
  }
  
  currentValidity = 0; 
  if(voucher == ""){
    voucher = generateVoucher();
    isNewVoucher = true;
  }else{
    if(isNewVoucher && voucher == currentActiveVoucher){
      isNewVoucher = true;
    }else{
      isNewVoucher = false;
    }
  }
  char * keys[] = {"status", "voucher"};
  int voucherLength = voucher.length() + 1;
  char voucherChar [voucherLength];
  voucher.toCharArray(voucherChar, voucherLength);
  char * values[] = {"true", voucherChar};
  if(voucher != currentActiveVoucher){
    resetGlobalVariables();
    activateCoinSlot();
    currentActiveVoucher = voucher;
  }
  setupCORSPolicy();
  server.send(200, "application/json", toJson(keys, values, 2));
}

boolean checkMacAddress(String mac){
  bool isValid = true;
  if(COINSLOT_BAN_COUNT > 0){
    Serial.print("Checking mac if valid ");
    Serial.println(mac);
    for(int i=0;i<attemptedMaxCount;i++){
      if (attempted[i].mac != ""){
          long curMil = millis();
          if( attempted[i].unlockTime > 0 && attempted[i].unlockTime <= curMil){
            Serial.print(attempted[i].mac);
            Serial.println(" unlocking mac address...");
            attempted[i].mac = "";
            attempted[i].attemptCount = 0;
            attempted[i].unlockTime = 0;
          }else if(attempted[i].mac == mac){
             Serial.print("Mac address has previous attempt");
             Serial.println(attempted[i].attemptCount);
             Serial.println(COINSLOT_BAN_COUNT);
             if( attempted[i].attemptCount >= COINSLOT_BAN_COUNT){
                isValid = false;
                Serial.print(mac);
                Serial.println(" mac address currenly banned");
             }
          }
      }
    }
  }
  return isValid;
}

void setupCORSPolicy(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Allow-Credentials", "false");
}

void activateCoinSlot(){
  digitalWrite(COIN_SET_PIN, HIGH);
  delay(200);
  coin = 0;
  acceptCoin = true;
  coinSlotActive = true;
  targetMilis = millis() + MAX_WAIT_COIN_SEC;
  digitalWrite(INSERT_COIN_LED, evaluateTriggerOutput(TURN_ON));
}

String toJson(char * keys[],char * values[],int nField){
  String json = "{";

  for (int i = 0; i < nField; i++) {
   if(i > 0){
    json += ",";
   }
   json += " \"";
   json += String(keys[i]);  
   json += "\": \"";
   json += String(values[i]);
   json += "\" ";
  
  }
  json += "}";
  return json;
}

String generateVoucher(){
  int randomNumber = random(1000, 9999);
  String voucher = VOUCHER_PREFIX+String(randomNumber);
  return voucher;
}

void registerNewVoucher(String voucher){
  String addCoinScript = "/ip hotspot user add name=";
  addCoinScript += voucher;
  addCoinScript += " limit-uptime=0 comment=0";
  if(VOUCHER_LOGIN_OPTION == 1){
    addCoinScript += " password=";
    addCoinScript += voucher;
  }
  if(VOUCHER_PROFILE != "" && VOUCHER_PROFILE != "default"){
    addCoinScript += " profile=";
    addCoinScript += VOUCHER_PROFILE;   
  }
  sendCommand(addCoinScript);
}

void addTimeToVoucher(String voucher, int secondsToAdd){

    String script = ":global lpt ; :global lpts; :global nlu; :set lpt [/ip hotspot user get ";
    script += voucher;
    script += " limit-uptime]; ";
    sendCommand(script);
    script = ":global hours [:pick $lpt 0 2]; :global minutes [:pick $lpt 3 5]; :global seconds [:pick $lpt 6 8];";
    sendCommand(script);
    script = " :set lpts [($hours*3600+$minutes*60+$seconds)]; :set nlu [($lpts+";
    script += secondsToAdd;
    script += ")]; ";
    //script += ")]; :global validity [/ip hotspot user get [find name=";
    //script += voucher;
    script += "/ip hotspot user set limit-uptime=$nlu comment=";
    script += currentValidity;
    script += "m " ;
    script += voucher;
    sendCommand(script);
}

void sendCommand(String script){
   Serial.println(script);
   int scriptLength = script.length() + 1;
   char command [scriptLength];
   script.toCharArray(command, scriptLength);
   tc.sendCommand(command);
}

void resetGlobalVariables(){
  currentActiveVoucher = "";
  timeToAdd = 0;
  totalCoin = 0;
}

void disableCoinSlot(){
  coinSlotActive = false;
  digitalWrite(COIN_SET_PIN, LOW);
  digitalWrite(INSERT_COIN_LED, evaluateTriggerOutput(TURN_OFF));
}

int calculateAddTime(){
  int totalTime = 0;
  currentValidity = 0;
  int remainingCoin = totalCoin;
  while(remainingCoin > 0){
    int candidatePrice = 0;
    int candidateIndex = -1;
    for(int i=0;i<ratesCount;i++){
      if(rates[i].price <= remainingCoin){
        if(candidatePrice < rates[i].price){
          candidatePrice = rates[i].price;
          candidateIndex = i;
        }
      }
    }
    if( candidateIndex != -1 ){
      //when extend time and voucher validity option is  First Validity + extend time, add the extend time instead of validity
      if((!isNewVoucher) && VOUCHER_VALIDITY_OPTION == 1){
        currentValidity += rates[candidateIndex].minutes;
      }else{
        currentValidity += rates[candidateIndex].validity;
      }
      
      totalTime += rates[candidateIndex].minutes;
      remainingCoin -= rates[candidateIndex].price;
    }else{
      break;
    }
  }
  return totalTime * 60;
}

const char * COLUMN_DELIMETER = "#";
const char * ROW_DELIMETER = "|";

void populateSystemConfiguration(){

  Serial.println("Loading system configuration");
  String data = readFile("/admin/config/system.data");
  Serial.print("Data: ");
  Serial.println(data);
  int rowSize = 25;
  String rows[rowSize];
  split(rows, data, '|');
  String ip[4];
  split(ip, rows[3], '.');
 
  mikrotikRouterIp[0] = ip[0].toInt();
  mikrotikRouterIp[1] = ip[1].toInt();
  mikrotikRouterIp[2] = ip[2].toInt();
  mikrotikRouterIp[3] = ip[3].toInt();
  vendorName = rows[0];
  ssid = rows[1];
  password = rows[2];
  user = rows[4];
  pwd = rows[5];
  MAX_WAIT_COIN_SEC = rows[6].toInt() * 1000;
  adminAuth = base64::encode(rows[7]+":"+rows[8]);
  COINSLOT_BAN_COUNT = rows[9].toInt();
  COINSLOT_BAN_MINUTES = rows[10].toInt();
  COIN_SELECTOR_PIN = rows[11].toInt();
  COIN_SET_PIN = rows[12].toInt();
  SYSTEM_READY_LED = rows[13].toInt();
  INSERT_COIN_LED = rows[14].toInt();
  LCD_TYPE = rows[15].toInt();
  INSERT_COIN_BTN_PIN = rows[16].toInt();
  CHECK_INTERNET_CONNECTION = rows[17].toInt();
  VOUCHER_PREFIX = rows[18];
  MARQUEE_MESSAGE = rows[19];
  macRandomIndex = rows[20].toInt();
  VOUCHER_LOGIN_OPTION = rows[21].toInt();
  VOUCHER_PROFILE = rows[22];
  VOUCHER_VALIDITY_OPTION = rows[23].toInt();
  LED_TRIGGER_TYPE = rows[24].toInt();

  #ifdef ESP32
  //this is for LAN base for generating MAC address, if already have existing in the setting we will reuse that mac address
  if(macRandomIndex < 0 || macRandomIndex > NUMBER_OF_MAC){
    macRandomIndex = random(0, NUMBER_OF_MAC-1);
    Serial.println("No mac address found yet on EEPROM, assigning new address");
    Serial.print(macRandomIndex);
    rows[20] = macRandomIndex;
    //save the random mac address
    String newData = "";
    for(int i=0;i<rowSize;i++){
      if(i>0){
        newData += ROW_DELIMETER;
      }
      newData += rows[i];
    }
    handleFileWrite("/admin/config/system.data", newData);
  }
  #endif
}


void split(String rows[], String data, char delimeter){
  int count = 0;
  String elementData = "";
  for(int i=0;i<data.length();i++){
      if(data.charAt(i) != delimeter){
        elementData.concat(data.charAt(i));
      }else{
        rows[count] = elementData;  
        elementData = "";
        count++;
      }
  }
  if(elementData != ""){
     rows[count] = elementData;
  }
}


void populateRates(){

  Serial.println("Loading promo rates");
  String data = readFile("/admin/config/rates.data");
  Serial.print("Data: ");
  Serial.println(data);
  int dataLength = data.length() + 1;
  char dataChar [dataLength];
  data.toCharArray(dataChar, dataLength);
  char * rowsPtr = strtok(dataChar, ROW_DELIMETER);
  ratesCount = 0;
  char *rows[100];
  while(rowsPtr != NULL){
    rows[ratesCount] = rowsPtr;
    ratesCount++; 
    rowsPtr = strtok(NULL, ROW_DELIMETER);
  }

  for(int i=0;i<ratesCount;i++){
    Serial.print("Data: ");
    Serial.println(rows[i]);
    char *column[4];
    char *colPtr = strtok(rows[i], COLUMN_DELIMETER);
    int j=0;
    while(colPtr != NULL){
      column[j] = colPtr;
      j++; 
      colPtr = strtok(NULL, COLUMN_DELIMETER);
    }
    rates[i].rateName = column[0];
    rates[i].price = atoi(column[1]);
    rates[i].minutes = atoi(column[2]);
    rates[i].validity = atoi(column[3]);
  }
  
}

int coinWaiting = 0;

void loop () {
   if(networkConnected){
    unsigned long currentMilis = millis();

   //handling for disconnection of AP
   if (!client.connected()) {
      handleSystemAbnormal();
      server.handleClient();
      return;
   }

    //insert button led will work only when have lcd
    if( LCD_TYPE > 0 ){
      int insertCoinButton = digitalRead(INSERT_COIN_BTN_PIN);
      if(insertCoinButton == LOW){
          printPleaseWait();
          if(!manualVoucher){
            if(welcomePrinted){
              bool result = activateManualVoucherPurchase();
              if(!result){
                //when no internet available, return back to normal to try later
                lastSaleTime = millis();
                thankyou_cooldown = 5000;
                welcomePrinted = false;
                return;  
              }
            }else{
              //clear thank you message after button press
              thankyou_cooldown = 0;
              targetMilis = currentMilis;
              delay(1000);
            }
          }else{
            //make coinslot expired when button is pressed
            targetMilis = currentMilis;
          }
      }
    }
       
    //insert coin logic
    if(acceptCoin){
      if((targetMilis > currentMilis)){
          coinExpired = false;
          //wait for the coin to insert
          if(coinsChange > 0){
            //delay(1500); change delay to coin waiting logic to prevent hanging of LCD 
            if(coinWaiting == 0){
              coinWaiting = currentMilis + 1500;
            }

            if(coinWaiting > currentMilis){
              goto printing;
            }

            coinWaiting = 0;
            if(coin == 6){
               coin = 5;
            }
            if(coin == 11){
               coin = 10;
            }
            Serial.print("Coin inserted: ");
            Serial.println(coin);
            coinsChange = 0;
            acceptCoin = false;

            //if manual voucher mode
            if(manualVoucher){
              totalCoin += coin;
              timeToAdd = calculateAddTime();
              activateCoinSlot();
            }
          }
          printing:
          if(timeToAdd > 0){
            printTransactionDetail();
          }else{
            printInsertCoinNow();
          }
      }else{
        disableCoinSlot();
        acceptCoin = false;
        coinExpired = true;
        manualVoucher = false;
        timeToAdd = calculateAddTime();
        //Auto add time no need to use voucher
        if(timeToAdd > 0 ) {
          clearAttemptToCoinSlot();
          Serial.print("Coin insert waiting expired, Auto using the voucher ");
          Serial.print(currentActiveVoucher);
          if(isNewVoucher){
            registerNewVoucher(currentActiveVoucher);
          }
          updateStatistic();
          addTimeToVoucher(currentActiveVoucher, timeToAdd);
          printThankYou();
        }else{
          addAttemptToCoinslot();
        }
        resetGlobalVariables();
      }
    }else{
      //if coinslot is disable
      //print welcome again after x seconds after thank you message
      if(targetMilis < currentMilis && currentMilis > (lastSaleTime + thankyou_cooldown)){
        welcomePrinted = true;
        printWelcome();
      }
    }
  }else{
    #ifdef ESP32
      //nothing
    #else
     dnsServer.processNextRequest();
    #endif
  }
  
  server.handleClient();
  #ifdef ESP32
    //nothing
  #else
    MDNS.update();
  #endif
}

void handleSystemAbnormal(){
    Serial.println("AP disconnected!!!!!!!!!!!!!!!");
    mikrotekConnectionSuccess = false;
    printSystemNotAvailable();
    digitalWrite(INSERT_COIN_LED, evaluateTriggerOutput(TURN_OFF));
    digitalWrite(SYSTEM_READY_LED, evaluateTriggerOutput(TURN_OFF));
    //Reconnect after 30 seconds
    delay(30000);
    ESP.restart();
}

void printInsertCoinNow(){
  if(LCD_TYPE > 0 ){
    long currentMilis = millis();
    //print only after 1 second to avoid performance issue
    if(currentMilis > (lastPrinted + 1000)){
      long remain = targetMilis - currentMilis;
      welcomePrinted = false;
      
     
      if(LCD_TYPE == 1){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Pls insert");
        lcd.setCursor(14, 0);
        lcd.print(String(remain/1000));
        lcd.setCursor(0, 1);
        lcd.print("coin now, 1/5/10");
      }else if(LCD_TYPE == 2){
        lcd20x4.clear();
        lcd20x4.setCursor(startCenterIndex(vendorName), 0);
        lcd20x4.print(vendorName);
        lcd20x4.setCursor(0, 1);
        lcd20x4.print("Pls insert");
        lcd20x4.setCursor(18, 1);
        lcd20x4.print(String(remain/1000));
        lcd20x4.setCursor(0, 2);
        lcd20x4.print("coin now, 1/5/10");
      }
      lastPrinted = currentMilis;
    }
  }
}

void printTransactionDetail(){
  if(LCD_TYPE > 0 ){
    long currentMilis = millis();
    //print only after 1 second to avoid performance issue
    if(currentMilis > (lastPrinted + 1000)){
      long remain = targetMilis - currentMilis;
      int days = timeToAdd / (3600*24);
      int hr =  timeToAdd % (3600*24) / 3600;
      int min =  timeToAdd % 3600 / 60;
  
     
     
      if(LCD_TYPE == 1){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PHP: "+String(totalCoin) + " ");
        lcd.setCursor(14, 0);
        lcd.print(String(remain/1000));
        lcd.setCursor(0, 1);
        String t = "T: ";
        t += String(days);
        t += "d ";
        t += String(hr);
        t += "h ";
        t += String(min);
        t += "m ";
        lcd.print(t);
      }else if(LCD_TYPE == 2){
        lcd20x4.clear();
        lcd20x4.setCursor(startCenterIndex(vendorName), 0);
        lcd20x4.print(vendorName);
        lcd20x4.setCursor(0, 1);
        lcd20x4.print("PHP: "+String(totalCoin) + " ");
        lcd20x4.setCursor(18, 1);
        lcd20x4.print(String(remain/1000));
        lcd20x4.setCursor(0, 2);
        String t = "T: ";
        t += String(days);
        t += "day ";
        t += String(hr);
        t += "hr ";
        t += String(min);
        t += "min ";
        lcd20x4.print(t);
      }
      lastPrinted = currentMilis;
    }
  }
}

void printThankYou(){
  if(LCD_TYPE > 0 ){
    welcomePrinted = false;
    if(LCD_TYPE == 1){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Code: "+currentActiveVoucher);
      lcd.setCursor(0, 1);
      lcd.print("Thank you!"); 
    }else if(LCD_TYPE == 2){
      lcd20x4.clear();
      lcd20x4.setCursor(startCenterIndex(vendorName), 0);
      lcd20x4.print(vendorName);
      lcd20x4.setCursor(0, 1);
      lcd20x4.print("Code: "+currentActiveVoucher);
      String thankYouText = "Thank you!";
      lcd20x4.setCursor(startCenterIndex(vendorName), 2);
      lcd20x4.print(thankYouText); 
    }
   
    lastSaleTime = millis();
  }
}

long lastWelcome = 0;
int welcomeBlinkState = 0;
int currentIndex=0;
void printWelcome(){
  long currentMilis = millis();
  if(LCD_TYPE > 0 && currentMilis > (lastWelcome + 500)){
     
      if(LCD_TYPE == 1){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Welcome to");
        lcd.setCursor(0, 1);
        if(welcomeBlinkState == 0){
          lcd.print(vendorName);
          welcomeBlinkState = 1;
        }else if(welcomeBlinkState == 1){
          lcd.print("");
          welcomeBlinkState = 0;
        }
      }else if(LCD_TYPE ==2){
        String text = "Welcome to";
        lcd20x4.clear();
        lcd20x4.setCursor(startCenterIndex(text), 0);
        lcd20x4.print(text);
        text = vendorName;
        lcd20x4.setCursor(startCenterIndex(text), 1);
        if(welcomeBlinkState == 0){
          lcd20x4.print(text);
          welcomeBlinkState = 1;
        }else if(welcomeBlinkState == 1){
          lcd20x4.print("");
          welcomeBlinkState = 0;
        }
        lcd20x4.setCursor(0, 3);
        String message = "                    ";
        int l = 0;
        for(int i=currentIndex;i<20;i++){
          if(l < MARQUEE_MESSAGE.length()){
            message[i] = MARQUEE_MESSAGE[l];
            l++;
          }
        }
        int a = l;
        for(int i=0;i<(MARQUEE_MESSAGE.length()-l);i++){
          message[i] = MARQUEE_MESSAGE[a];
          a++;
        }
        lcd20x4.print(message);
        currentIndex++;
        if(currentIndex >= 20){
          currentIndex = 0;
        }
      }
      lastWelcome = currentMilis;
  }  
}

bool activateManualVoucherPurchase(){
  bool hasInternetConnection = true;
  if(CHECK_INTERNET_CONNECTION == 1){
        hasInternetConnection = hasInternetConnect();
  }
  if(!hasInternetConnection){
    printInternetNotAvailable();
    return false;
  }

  if(!checkIfSystemIsAvailable()){
      printSystemNotAvailable();
      return false;
  }

  currentMacAttempt = currentMacAddress;
  currentValidity = 0;
  isNewVoucher = true;
  resetGlobalVariables();
  activateCoinSlot();
  currentActiveVoucher = generateVoucher();
  manualVoucher = true;
  //show 30 sec the voucher code
  thankyou_cooldown = 30000;
  return true;
}

void printSystemNotAvailable(){
  
  if(LCD_TYPE > 0){
    
      if(LCD_TYPE == 1){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("System not");
        lcd.setCursor(0, 1);
        lcd.print("Available");
      }else if(LCD_TYPE == 2){
        String text = "System not";
        lcd20x4.clear();
        lcd20x4.setCursor(startCenterIndex(text), 1);
        lcd20x4.print(text);
        text = "Available";
        lcd20x4.setCursor(startCenterIndex(text), 2);
      }
  }  
}

void printInternetNotAvailable(){
  
  if(LCD_TYPE > 0){
      if(LCD_TYPE == 1){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Internet not");
        lcd.setCursor(0, 1);
        lcd.print("Available");
      }else if(LCD_TYPE == 2){
        String text = "Internet not";
        lcd20x4.clear();
        lcd20x4.setCursor(startCenterIndex(text), 1);
        lcd20x4.print(text);
        text = "Available";
        lcd20x4.setCursor(startCenterIndex(text), 2);
      }
  }
}

void printPleaseWait(){
  
  if(LCD_TYPE > 0){
      if(LCD_TYPE == 1){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Please wait...");
        lcd.setCursor(0, 1);
        lcd.print("");
      }else if(LCD_TYPE == 2){
        lcd20x4.clear();
        lcd20x4.setCursor(startCenterIndex(vendorName), 0);
        lcd20x4.print(vendorName);
        String text = "Please wait...";
        lcd20x4.setCursor(startCenterIndex(text), 1);
        lcd20x4.print(text);
      }
  }
}

int startCenterIndex(String text){
    int totalSize = 20;
    int textLength = text.length();
    int blankCharCount = totalSize - textLength;
    int startCenterIndex = (blankCharCount / 2);
    if(blankCharCount%2 != 0){
        startCenterIndex--;
    }
    return startCenterIndex;
}


int evaluateTriggerOutput(int state){
  if(LED_TRIGGER_TYPE == 1){
    if(state == TURN_ON){
        return HIGH;
    }else{
        return LOW;
    }
  }else{
    if(state == TURN_ON){
        return LOW;
    }else{
        return HIGH;
    }
  }
}