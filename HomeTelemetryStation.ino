#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DHT.h>

const char* ssid = "ditoge03";
const char* password = "mitko111";

#define DHTPIN 23          
#define DHTTYPE DHT11     

#define GEIGER_PIN 12 
int clicksCount = 0;
volatile unsigned long startTime = 0;   

DHT dht(DHTPIN, DHTTYPE);

WebServer server(80);

IPAddress staticIP(192, 168, 0, 5); 
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

void IRAM_ATTR handleGeigerInterrupt() {
    clicksCount++;
}

float conversionFactor = 0.00331; // 0.00331 µSv per click

float calculateRadiation(float cpm) {

    float radiation = cpm * conversionFactor;

    float backgroundRadiation = 20.0; // Standard background radiation in cpm
    float backgroundRadiation_uSv = backgroundRadiation * conversionFactor; // Convert background radiation to µSv
    radiation -= backgroundRadiation_uSv;

    if (radiation < 0) {
        radiation = 0;
    }

    return radiation;
}

float calculateCPM() {
    unsigned long elapsedTime = millis() - startTime;

    // Calculate CPM (Counts Per Minute)
    float cpm = (clicksCount / (float)elapsedTime) * 60000.0;

    // Reset total clicks and start time for the next counting period
    clicksCount = 0;
    startTime = millis();

    return cpm;
}

void handleGetData() {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    float cpm=calculateCPM();
    float rad=calculateRadiation(cpm);

    Serial.println(cpm);
    Serial.print(rad);
    Serial.println(" µSv");

    Serial.println(humidity);
    Serial.println(temperature);

    String response = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity)+ ",\"cpm\":" + String(cpm)+ ",\"rad\":" + String(rad) + "}";
    server.send(200, "application/json", response);
}

void setup() {
    startTime=millis();
    Serial.begin(115200);
    delay(10);

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

     WiFi.config(staticIP, gateway, subnet, dns);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
      Serial.println("Connecting to WiFi...");
  }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
    dht.begin();

    pinMode(GEIGER_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), handleGeigerInterrupt, RISING); //Callback when the geiger detects a click

    server.on("/data", HTTP_GET, handleGetData);
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}


