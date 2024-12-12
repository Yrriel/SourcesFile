#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <WebServer.h>

HardwareSerial serialPort(2); // use UART2

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&serialPort);

uint8_t id;
uint8_t conditionswitch;
uint8_t option;
uint8_t getFingerprintEnroll();
uint8_t deleteFingerprint();

const char *ap_ssid = "ESP32-Config", *ap_password = "123456789";
WebServer server(80);
String savedSSID = "", savedPassword = "", DigitalSerialNumber = "";
// interval to read the fingerprint sensor
unsigned long previousMillis = 0;
const long readInterval = 1000;

// interval when to close the door lock
bool isOpen = false;
const long closeInterval = 5000;
unsigned long previousOpenMillis = 0;

// Relay Pin
const int RELAY_PIN = 23;

// Buzzer
const int buzzerPin = 22;

const char *htmlForm = R"(
  <!DOCTYPE html>
<html>
<head>
  <title>Connect to Wi-Fi</title>
  <style>
    body { font-family: Arial; margin: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #393453; }
    .container{ font-size: 40px; background-color: #6a4191; padding: 20px; border-radius: 50px; color: white; }
    .center-this{ display: flex; justify-content: center; }
    input[type=text], input[type=password] { padding: 10px; margin: 5px; width: 300px; font-size:40px; background-color: #d7abff; border: none; border-radius: 20px; }
    input[type=submit] { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; font-size: 40px; }
  </style>
</head>
<body>
  <form action="/connect" method="POST" class="container">
    <h1>Enter Wi-Fi Credentials</h1>
    SSID: <input type="text" name="ssid" required><br><br>
    Password: <input type="password" name="password" required><br><br>
    <div class="center-this"><input type="submit" value="Connect"></div>
  </form>
</body>
</html>
)";

const char *serialNumberForm = R"(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Enter Serial Number</title>
    <style>
      body { font-family: Arial; margin: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #393453; }
      .container{ font-size: 40px; background-color: #6a4191; padding: 20px; border-radius: 50px; color: white; }
      .center-this{ display: flex; justify-content: center; }
      input[type=text] { padding: 10px; margin: 5px; width: 300px; font-size:40px; background-color: #d7abff; border: none; border-radius: 20px; }
      input[type=submit] { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; font-size: 40px; border-radius: 20px; }
    </style>
  </head>
  <body>
    <form action="/saveSerial" method="POST" class="container">
    <h1>Enter Your Serial Number</h1>
    Serial Number: <input type="text" name="serial" required><br><br>
    <div class="center-this"><input type="submit" value="Save Serial Number"></div>
    </form>
  </body>
  </html>
)";

void handleFormSubmit() {
  String ssid = server.arg("ssid"), password = server.arg("password");
  savedSSID = ssid;
  savedPassword = password;

  WiFi.begin(ssid.c_str(), password.c_str());
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) delay(1000);

  if (WiFi.status() == WL_CONNECTED) {
    // server.send(200, "text/html", "<h1>Connected to Wi-Fi!</h1>");
    server.sendHeader("Location", "/serialNumber", true);
    server.send(302, "text/html", "");
  } else {
    server.send(200, "text/html", "<h1>Failed to connect! Go back and input your wifi-network</h1>");
  }
}

void handleSaveSerialNumber() {
  DigitalSerialNumber = server.arg("serial");
  server.send(200, "text/html", "<h1>Serial Number Saved!</h1><p>" + DigitalSerialNumber + "</p>");
  WiFi.softAPdisconnect(true);
  conditionswitch = 1;
}

void setup()
{
  Serial.begin(9600);
  conditionswitch = 0;
  if (savedSSID != "" && savedPassword != "") {
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) delay(1000);
    if (WiFi.status() == WL_CONNECTED) {
      conditionswitch = 1;
      return;
    }
  }

  WiFi.softAP(ap_ssid, ap_password);
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", htmlForm); });
  server.on("/connect", HTTP_POST, handleFormSubmit);
  server.on("/serialNumber", HTTP_GET, []() { server.send(200, "text/html", serialNumberForm); });
  server.on("/saveSerial", HTTP_POST, handleSaveSerialNumber);
  server.begin();
  while (!Serial)
    ; // For Yun/Leo/Micro/Zero/...
  delay(100);
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment");

  // set the data rate for the sensor serial port
  finger.begin(57600);

  if (finger.verifyPassword())
  {
    Serial.println("Found fingerprint sensor!");
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    while (1)
    {
      delay(1);
    }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);
}

uint8_t readnumber(void)
{
  uint8_t num = 0;

  while (num == 0)
  {
    while (!Serial.available())
      ;
    num = Serial.parseInt();
  }
  return num;
}

void deletionFingerprint(){
  Serial.println("Ready to Delete a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to delete this finger as...");
  id = readnumber();
  if (id == 0)
  { // ID #0 not allowed, try again!
    return;
  }
  Serial.print("Enrolling ID #");
  Serial.println(id);

  while (!deleteFingerprint())
    ;
}

void enrollmentFingerprint(){
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
  id = readnumber();
  if (id == 0)
  { // ID #0 not allowed, try again!
    return;
  }
  Serial.print("Enrolling ID #");
  Serial.println(id);

  while (!getFingerprintEnroll())
    ;
}

void loop() // run over and over again
{
  if(conditionswitch == 0){
    server.handleClient();
  }
  if(conditionswitch == 1){
  Serial.println("Ready for fingerprint!");
  Serial.println("Please type 1 for enrollment and 2 for deletion");
  option = readnumber();
  if (option < 1 || option > 2)
  { // ID #0 not allowed, try again!
    return;
  }
  if(option == 1){
    enrollmentFingerprint();
  }
  if(option == 2){
    deletionFingerprint();
  }
  }
}

uint8_t deleteFingerprint() {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
    return p;
  }

  return true;
}

uint8_t getFingerprintEnroll()
{

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Prints matched!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_ENROLLMISMATCH)
  {
    Serial.println("Fingerprints did not match");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Stored!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    Serial.println("Could not store in that location");
    return p;
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    Serial.println("Error writing to flash");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}