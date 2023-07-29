//LoRa RFM95
#include <SPI.h>
#include <LoRa.h>
#define ss 5
#define rst 14
#define dio0 2
int packetSize;

// //AES
#include <AESLib.h>
AESLib aesLib;
// // Deklarasi kunci enkripsi AES128 dan IV
byte aes_key[] = { 0x15, 0x2B, 0x7E, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xA5, 0xF7, 0x15, 0x08, 0x09, 0xCF, 0x4F, 0x3D };
byte aes_iv[N_BLOCK] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
char cleartext[256];
String input;
String output;

//millis
unsigned long task1 = 0;
unsigned long interval_1 = 5000;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 3600000;

//GPS
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
boolean flag = false;
String dataLat; 
String dataLong;
static const int RXPin = 26, TXPin = 25;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial softserial(RXPin, TXPin);
String gpsData;
String initGPS;

//ultrasonik
long duration;
float jarak;
#define triggerPin 33
#define echoPin 34
String ultrasonicData;

//baterai
#include <Wire.h> // Needed for I2C
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
SFE_MAX1704X lipo(MAX1704X_MAX17048);
float voltage = 0; // Variable to keep track of LiPo voltage
float batteryLevel = 0; // Variable to keep track of LiPo state-of-charge (SOC)

void setup() {
  Serial.begin(115200);
  Wire.begin();
  softserial.begin(GPSBaud);

  //setup ultrasonik
  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //setup LoRa
  while (!Serial);
    Serial.println("LoRa Sender");
  LoRa.setPins(ss, rst, dio0);
  LoRa.begin(915E6);
  while (!LoRa.begin(915E6)) {
    Serial.println(".");
    delay(500);
    }
  LoRa.setSignalBandwidth(125E3);
  LoRa.setSpreadingFactor(7);   
  LoRa.setSyncWord(0xF3);
  LoRa.idle();
  Serial.println("LoRa Initializing OK!");

  //cek baterry
  lipo.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

  if (lipo.begin() == false) // Connect to the MAX17043 using the default wire port
  {
    Serial.println(F("MAX17048 not detected. Please check wiring. Freezing."));
    while (1);
  }
  lipo.quickStart();

   // setup AES
  aes_init();

  //kirim data sekali sebagai tanda kehidupan
  batteryLevel = lipo.getSOC();
  ultrasonik();
  input = "@" + String(batteryLevel) + "," + ultrasonicData + ",0,0!";
  kirimData(input);
}

void loop() {
  unsigned long currentTime = millis();
  
  //Task1 : GPS
  if (flag == false){
    while (softserial.available() > 0)
    if (gps.encode(softserial.read()))
    {
    tampilkandata();
    }
  }
    //Cek komunikasi arduino dengan module gps
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("tidak ada module GPS, cek wiringmu!"));
    while(true);
  }

   //Task 2: Membaca Kapasitas baterai dan tempat sampah
  if (currentTime - task1 >= interval_1){
  batteryLevel = lipo.getSOC();
  ultrasonik();
  task1 = currentTime;
  }
  
  //Task 3: Pengiriman Data rutin
  if (currentTime - lastSendTime >= sendInterval) {
    if (jarak <= 15)
    {
      input ="@" + String(batteryLevel) + "," + ultrasonicData + "," + dataLat + "," + dataLong + "!";
    }
    else{
      input = "@" + String(batteryLevel) + "," + ultrasonicData + ",0,0!";
    }
      kirimData(input);
      lastSendTime = currentTime;
  }

  //Emergency Task
  if (batteryLevel <= 5){
    input = "@" + String(batteryLevel) + "," + ultrasonicData + ",0,0!";
    kirimData(input);
    esp_deep_sleep_start();
  }
}

//-----------------------------fungsi terpisah------------------------------------//
//enkripsi AES
void aes_init() {
  aesLib.gen_iv(aes_iv);
  // workaround for incorrect B64 functionality on first run...
  encrypt("HELLO WORLD!", aes_iv);
}

void kirimData(String data){
  //konversi data ke char
  data.toCharArray(cleartext, data.length()+1);
  byte enc_iv[N_BLOCK] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // iv_block gets written to, provide own fresh copy...
  output = encrypt(cleartext, enc_iv);

  // kirim data
  LoRa.beginPacket();
  LoRa.print(output);
  LoRa.endPacket();
  Serial.print("Sending packet: ");
  Serial.println(data);
  Serial.print("Ciphertext: ");
  Serial.println(output);
  packetSize = strlen(output.c_str()) * 8;
  Serial.print("Packet size: ");
  Serial.print(packetSize);
  Serial.println(" bits");

}

void ultrasonik() 
{
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2); 
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10); 
  digitalWrite(triggerPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  jarak = duration / 58.2;
  ultrasonicData = String(jarak);
} 

void tampilkandata()
{
  //Menampilkan data LOKASI
  if (gps.location.isValid())
  {
    Serial.println(F("Lokasi : ")); 
    dataLat = String(gps.location.lat(), 8);
    dataLong = String(gps.location.lng(), 8);
    if((dataLat != "0.00000000" && dataLong != "0.00000000") || (dataLat != "" && dataLong != "") )
      {
        Serial.print("Latitude : "); Serial.println(dataLat);
        Serial.print("Longitude : "); Serial.println(dataLong);
        gpsData = dataLat + "," + dataLong;
        Serial.print("Sending GPS data: ");
        Serial.println(gpsData);
        initGPS = "@" + String(batteryLevel) + "," + ultrasonicData + "," + dataLat + "," + dataLong + "!";
        kirimData(initGPS);
        flag = true;

        // //shutdown GPS
        // softserial.end();
        // pinMode(TXPin, INPUT);
        // pinMode(RXPin, INPUT);
      }
  }
  else
  {
    // Serial.println("INVALID"); 
  }
}

String encrypt(char * msg, byte iv[]) {  
  int msgLen = strlen(msg);
  char encrypted[2 * msgLen];
  aesLib.encrypt64((byte*)msg, msgLen, encrypted, aes_key,sizeof(aes_key), iv);  
  return String(encrypted);
}