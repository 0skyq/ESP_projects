//sky code with my database

#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "SKY"
#define WIFI_PASSWORD "00000000"


#define API_KEY "AIzaSyDw8YOffUR4VBr-IeX9pxB96Ajfl6ydzGw"
#define DATABASE_URL "https://eal-lab-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define NUM_LEDS 6
#define NUM_DETECTORS 3
#define NUM_OF_SAMPLES 2000

const int ledPins[NUM_LEDS] = {23,22,21,19,18,5};  
const int detectorPins[NUM_DETECTORS] = {34,35,32};
float detectorValue[NUM_OF_SAMPLES][NUM_LEDS];

const int pulse_width = 3;
const int delay_bet_LEDs = 3;


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String parentPath;
String testPath;
FirebaseJsonArray arr;

bool signupOK = false;
int num;
bool Start = false;
int Num_of_samples;

unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 1000;

void WiFisetup() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(300);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

void Databasesetup(){
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Signup OK");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback; 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  testPath = "/testData";
}

void setup(){
  Serial.begin(115200);
  WiFisetup();
  Databasesetup();
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
 
}

void Sensor_read(int y){

  Serial.println("Sensor reading....");
  for(int n = 0; n<y;n++){
    Serial.print("sample number : ");
    Serial.print(n);
    for (int ledIndex = 0; ledIndex < NUM_LEDS; ledIndex++) {

      for (int i = 0; i < NUM_LEDS; i++) {
        digitalWrite(ledPins[i], LOW);
        delay(delay_bet_LEDs);
      }

      digitalWrite(ledPins[ledIndex], HIGH);
      delay(pulse_width);  
      int detectorIndex = ledIndex / 2;  
      detectorValue[n][ledIndex] = 0.25*analogRead(detectorPins[detectorIndex]);
    }

    Serial.print("[ ");
    for(int i = 0;i<NUM_LEDS;i++){ 
     Serial.print(detectorValue[n][i]);
     Serial.print(",");
    }
    Serial.print("]");
    Serial.println();
  }
}

void resetDetectorValues() {
  for (int i = 0; i < NUM_OF_SAMPLES; i++) {
    for (int j = 0; j < NUM_LEDS; j++) {
      detectorValue[i][j] = 0;
    }
  }
}

void loop(){
   
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    if(Firebase.RTDB.getString(&fbdo,"/Start/Details/Number")){
      if(fbdo.dataType() == "int"){
        Num_of_samples = fbdo.intData();
     }  
    }

    if (Firebase.RTDB.getBool(&fbdo, "/Start/Digital/")) {
      if (fbdo.dataType() == "boolean") {
        Start = fbdo.boolData();
      }
    }

    if(Start ==1 && num < Num_of_samples ){
      Serial.print("Number of samples:   ") ;
      Serial.println(Num_of_samples)   ; 
      Firebase.RTDB.setString(&fbdo, "Start/Transmission","Ongoing");
      
      Sensor_read(Num_of_samples);
      Serial.println("-----Readings DONE ---------");

      for(int i =0;i<Num_of_samples;i++){

        //Serial.println("Transmission Ongoing");
        parentPath = testPath+"/"+String(num);
        arr.add(detectorValue[i][0], detectorValue[i][1],detectorValue[i][2],detectorValue[i][3],detectorValue[i][4],detectorValue[i][5]);
        Serial.printf("Set Jsonarray... %s\n", Firebase.RTDB.setArray(&fbdo, parentPath.c_str(), &arr) ? "ok" : fbdo.errorReason().c_str());
      
        num++;
        arr.clear();
      }

    }
 
    if(Num_of_samples== num){
      Firebase.RTDB.setBool(&fbdo, "Start/Digital",false);
      Firebase.RTDB.setString(&fbdo, "Start/Transmission","Done");
      Serial.println("Transmission Done");
      num=0;
      for (int i = 0; i < NUM_LEDS; i++) {
        digitalWrite(ledPins[i], LOW);
      }
      resetDetectorValues();

    }
  } 

  if ((WiFi.status() != WL_CONNECTED)){
    Serial.println("Reconnecting to WiFi...");
    WiFisetup();
  }
}