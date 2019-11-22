#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
constexpr uint16_t sleepTimeInSeconds = 300;
bool wifiConnection = false;
bool mqttConnection = false;
uint32_t mqtt_delay = 100;
uint16_t dht_delay = 1000;
uint16_t sleep_delay = 10000;
uint16_t dhtSampleDelay = 1000;
uint16_t dht_timer = 0;
uint8_t dhtPin = 2;
uint8_t sampleCount = 0;
double *temperatureArray = NULL;
double *backupTemperatureArray = NULL;
double *humidityArray = NULL;
double *backupHumidityArray = NULL;
double temperature = 0;
double humidity = 0;

const char* ssid = "*************";
const char* password = "****************";
const char* mqtt_server = "******************";
const char* mqttClient_hostname = "ESP-01S remote temperature station #1";
const char* mqttClient_id = "xx";
const char* mqttClient_password = "xx";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht;

void calculateAverages(){
  uint8_t dataCountTemp = 0;
  uint8_t dataCountHumidity = 0;
  for(uint8_t i = 0;i<sampleCount;i++){
    if(temperatureArray[i] > 0){
      temperature += temperatureArray[i];
      dataCountTemp++;
    }
  }
  for(uint8_t i = 0;i<sampleCount;i++){
    if(humidityArray[i] > 0){
      humidity += humidityArray[i];
      dataCountHumidity++;
    }
  }
  char popisek[50];
  sprintf(popisek,"NumberOfMeasurements:%d ! Sum:%f", dataCountTemp, temperature);
  client.publish("home/temperature/station1/debug",popisek);
  temperature /= dataCountTemp;
  humidity /= dataCountHumidity;
}

void sendData(){
  calculateAverages();
  //Serial.printf("Reported Temperature: %f \n", temperature);
  client.publish("home/temperature/station1/value",String(temperature).c_str());
  //Serial.printf("Reported Humidity: %f \n", humidity);
  client.publish("home/humidity/station1/value",String(humidity).c_str());
  delay(300);
}

void getWifiInfo(){
  Serial.println("");
	Serial.println("WiFi connected");
	Serial.print("IP address:  ");
	Serial.println(WiFi.localIP());
	Serial.println("");
}

bool establishWifiConnection(){
	/*Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);*/
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	if(WiFi.status() == WL_CONNECTED){
	//getWifiInfo();     //Výpis informací o připojení na sériovku
	return true;
	}
	else {
	//Serial.println("");
	//Serial.println("Couldn't establish Wifi connection at the moment.");
	return false;
	}
}

bool establishMQTTConnection(){
	if(wifiConnection){
    //Serial.print(F("Attempting MQTT connection..."));
    String clientId = mqttClient_hostname;
    if (client.connect(clientId.c_str(), mqttClient_id, mqttClient_password)) {
      //Serial.println("connected");
      // Once connected, publish an announcement...
      String statusToPublish = "alive";
      client.publish("home/temperature/station1/state", statusToPublish.c_str());
      // ... and resubscribe
      return true;
    } else {
      //Serial.print("failed, rc=");
      //Serial.print(client.state());
      //Serial.println("try again later");
      return false;
    }
	}
	return false;
}

void setup() {
  // put your setup code here, to run once:
  //Serial.begin(9600);
  temperatureArray = (double*)malloc(1*sizeof(double));
  humidityArray = (double*)malloc(1*sizeof(double));
  //Serial.println("Awake");
  pinMode(dhtPin,INPUT);
  dht.setup(dhtPin);
  dhtSampleDelay = dht.getMinimumSamplingPeriod();
  wifiConnection = establishWifiConnection();
  client.setServer(mqtt_server, 1883);
  if(wifiConnection){
    mqttConnection = establishMQTTConnection();
  }
  sampleCount = 0;
}

void loop() {
  if(millis()>sleep_delay){
    sendData();
    //sleep_delay = millis();
    //Sending data before dying
    //Serial.println("Going to sleep");
    free(temperatureArray);
    free(humidityArray);
    ESP.deepSleep(sleepTimeInSeconds * 1000000);
    // It takes a while for the ESP to go to sleep, so this while() prevents it from doing anything else
    while(1){yield();}
  }
  //loops++;
  if(!wifiConnection){ // Kontrola připojení Wifi. Pokud není připojení aktivní, dojde ke kontrole aktuálního stavu
    if(WiFi.status() == WL_CONNECTED){
      wifiConnection = true; // Stav Wifi připojení se změní na funkční
      //getWifiInfo(); // Funkce vypíše informace o Wifi připojení
      mqtt_delay = millis(); // Při připojení dojde k pozdržení pokusu o připojení k MQTT serveru, aby nedošlo k chybě při příliš brzkém pokusu o připojení
    }
  }else{ // Pokud je Wifi aktivní dojde k případnému připojení k MQTT serveru
    if(!mqttConnection && (mqtt_delay + 500 < millis())){   
      mqttConnection = establishMQTTConnection();     // Funkce se pokusí o připojení na MQTT Server
      mqtt_delay = millis();   // Pokud nedošlo k úspěšnému připojení, je další pokus iniciován po 5 sekundách
    }
  }
  if(millis()>(dht_timer+dhtSampleDelay)){
    /*
    Serial.print("cas millis:");
    Serial.println(millis());
    Serial.print("cas dht_timer:");
    Serial.println(dht_timer);
    Serial.print("Cas dht_delay:");
    Serial.println(dht_delay);
    */
    dht_timer = millis();
    sampleCount++;
    backupTemperatureArray = temperatureArray;
    temperatureArray = (double*) realloc(temperatureArray, sampleCount * sizeof(double));
    if(temperatureArray == NULL){
      free(backupTemperatureArray);
      }
    temperatureArray[sampleCount-1] = dht.getTemperature(); 
    //temperatureArray[sampleCount-1] = random(20, 35); 
    backupHumidityArray = humidityArray;  
    humidityArray = (double*) realloc(humidityArray, sampleCount * sizeof(double));
    if(humidityArray == NULL){
      free(backupHumidityArray);
      }
    humidityArray[sampleCount-1] = dht.getHumidity();     
    //humidityArray[sampleCount-1] = random(40,60);
  }
}