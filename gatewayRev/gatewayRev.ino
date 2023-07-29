//LoRa
#include <SPI.h>
#include <LoRa.h>
#define SS 15
#define RST 0
#define DI0 27
#define BAND 915E6

//AES
#include "AESLib.h"
AESLib aesLib;
char ciphertext[512];
// AES Encryption Key
byte aes_key[] = { 0x15, 0x2B, 0x7E, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xA5, 0xF7, 0x15, 0x08, 0x09, 0xCF, 0x4F, 0x3D };
// General initialization vector (you must use your own IV's in production for full security!!!)
byte aes_iv[N_BLOCK] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
// Generate IV (once)

//waktu
#include <time.h>
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7*3600;
const int   daylightOffset_sec = 0;

//WIFI
#include <WiFi.h>
#define WIFI_SSID "Masjid Nurul Huda"
#define WIFI_PASSWORD "gampangkok"
bool flagKonekWifi = false;

//Firebase
#include <FirebaseESP32.h>
#define FIREBASE_HOST "https://bank-sampah-mentari-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "ZhMArBiUbJYziTqivFOFcEJI4fjopChe1ONf9VkP"
FirebaseData firebaseData;

//Local client HTTP
#include <HTTPClient.h>
HTTPClient http;

//Variabel Global
String input;
String output;
String node;
String lattitude;
String longitude;
String ultrasonicData;
String batteryLevel;
String rssi;

void setup() {
  Serial.begin(115200);
  SPI.begin(14, 12, 13, 15);
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(BAND)) {
      Serial.println("Starting LoRa failed!");
      while (1);
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setSpreadingFactor(7);  
  LoRa.receive();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  setupwifi();
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  aes_init();

    // Initialize NTP client
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

}

void loop() {

  if(flagKonekWifi == false){
    setupwifi();
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
      Serial.print("Data diterima: ");
      //menangkap data yang diterima
      while (LoRa.available()) {
        input = LoRa.readString();
      }
      Serial.print(input);
      Serial.print("' with RSSI ");
      Serial.println(LoRa.packetRssi());
      rssi = LoRa.packetRssi();
      //cek node
      char cekNode = input[0];
      if(cekNode != '?'){
        input.toCharArray(ciphertext, input.length()+1);
        byte dec_iv[N_BLOCK] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // iv_block gets written to, provide own fresh copy...
        output = decrypt(ciphertext, dec_iv);    
        Serial.println("Decrypted: " + output);        
      }
      else if(cekNode == '?') output = input;
      ekstrakData(output);
  }
}

void aes_init(){
  aesLib.gen_iv(aes_iv);
}

//fungsi decrypt
String decrypt(char * msg, byte iv[]) {
  unsigned long ms = micros();
  int msgLen = strlen(msg);
  char decrypted[msgLen]; // half may be enough
  aesLib.decrypt64(msg, msgLen, (byte*)decrypted, aes_key,sizeof(aes_key) ,iv);  
  return String(decrypted);
}

void ekstrakData(String output){
  int firstCommaIndex = output.indexOf(',');
  int secondCommaIndex = output.indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = output.indexOf(',', secondCommaIndex + 1);
  int lastIndex = output.indexOf('!', thirdCommaIndex + 1);
  node = String(output[0]);
  batteryLevel = output.substring(1, firstCommaIndex);
  ultrasonicData = output.substring(firstCommaIndex + 1, secondCommaIndex);
  lattitude = output.substring(secondCommaIndex + 1, thirdCommaIndex);
  longitude = output.substring(thirdCommaIndex + 1, lastIndex);
  Serial.print("Battery Level: ");
  Serial.println(batteryLevel);
  Serial.print("Ultrasonic Data: ");
  Serial.println(ultrasonicData);
  Serial.print("Latitude: ");
  Serial.println(lattitude);
  Serial.print("Longitude: ");
  Serial.println(longitude);
  
  uploadData(node, ultrasonicData, batteryLevel, lattitude, longitude);
}

void setupwifi(){
  // Serial.print("Menghubungkan Wi-Fi");
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   Serial.print(".");
  //   delay(300);
  // }
  // Serial.println();
  if (WiFi.status() == WL_CONNECTED){
    Serial.print("terhubung dengan IP: ");
    Serial.println(WiFi.localIP());
    flagKonekWifi = true;
  }
}

void uploadData(String node, String ultrasonicData, String batteryLevel, String lattitude, String longitude)
{
  String nodePath;
  //seleksi node
  if (node == "@") {
  nodePath = "Node1";
  } else if (node == "#") {
  nodePath = "Node2";
  } else if (node == "?") {
  nodePath = "Node3";
  } else {
  nodePath = "unknown";
  }
  // Serial.println(nodePath);

  if (node != "" && nodePath != "unknown")
  {
    if(flagKonekWifi != false){
      String timestamp = getTime();
      //KIRIM DATA SENSOR
      Firebase.setString(firebaseData, "Node/" + nodePath + "/jarak", ultrasonicData);
      //kirim data baterai
      Firebase.setString(firebaseData, "Node/" + nodePath + "/Baterai", batteryLevel);
      Serial.println("sukses kirim data baterai & kapasitas sampah" + nodePath);
      Firebase.setString(firebaseData, "Node/" + nodePath + "/lastUpdate", timestamp);

      //seleksi kondisi apakah mendapat sinyal GPS dari sender  
      if(lattitude != "0" && longitude != "0" && longitude != "" && lattitude != "")
      {
        //TODO: buat fungsi kirim lokasi ke firebase
        Serial.println("Lattitude " + nodePath + " = " + lattitude);
        Serial.println("Longitude " + nodePath + " = " + longitude);
        Firebase.setString(firebaseData, "Node/" + nodePath + "/Lattitude", lattitude);
        Firebase.setString(firebaseData, "Node/" + nodePath + "/Longitude", longitude);
      }
      //kirim data ke lokal
    kirimdataLokal(nodePath);
    }
  }
}

void kirimdataLokal(String nodePath){
  int headerLength = 0;
  
  nodePath.toLowerCase();
  String postData = (String)"node=" + nodePath + "&baterai=" + batteryLevel + "&jarak=" + ultrasonicData + "&latt=" + lattitude + "&long=" + longitude + "&rssi=" + rssi;
  nodePath.toLowerCase();
  http.begin("http://192.168.1.171/BankSampahMentari/api.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  auto httpCode = http.POST(postData);
  String payload = http.getString();
  Serial.println(postData);
  Serial.println(payload);
  http.end();
  //menghitung ukuran paket
  headerLength += String("Content-Type: application/x-www-form-urlencoded").length() + 4;
  int bodyLength = payload.length();
  int totalLength = headerLength + bodyLength;
  Serial.println("Ukuran paket data = " + String(payload) + "Bytes");
  Serial.println("Ukuran header = " + String(headerLength) + "Bytes");
  Serial.println("Ukuran total paket = " + String(totalLength) + "Bytes"); 

}

String getTime(){
  // Get current time
  time_t now = time(nullptr);
  
  // Convert time_t to tm
  struct tm *timeinfo = localtime(&now);

  // Format time as string
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(timeString);
}
