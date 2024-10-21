#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DHT.h>

const char* ssid = "";
const char* password = "";

#define DEBOUNCE_INTERVAL 50
volatile unsigned long last_interrupt_time = 0;

//#define DHTPIN 23
#define DHTPIN 2
#define DHTTYPE DHT11

//#define GEIGER_PIN 12
#define GEIGER_PIN 10
int clicksCount = 0;
volatile unsigned long startTime = 0;

#define DEBUG false

DHT dht(DHTPIN, DHTTYPE);

WebServer server(80);

IPAddress staticIP(192, 168, 0, 5);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

void IRAM_ATTR handleGeigerInterrupt() {
  unsigned long interrupt_time = millis();

  noInterrupts();
  if (interrupt_time - last_interrupt_time > DEBOUNCE_INTERVAL) {
    clicksCount++;
    if (DEBUG)
      Serial.println("Geiger pulse detected!");
  }

  last_interrupt_time = interrupt_time;

  interrupts();
}

float conversionFactor = 0.00331;  // 0.00331 µSv per click

float calculateRadiation(float cpm) {

  float radiation = cpm * conversionFactor;

  float backgroundRadiation = 20.0;                                        // Standard background radiation in cpm
  float backgroundRadiation_uSv = backgroundRadiation * conversionFactor;  // Convert background radiation to µSv
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

float humidity = 0;
float temperature = 0;
float tempSum = 0;
float humiditySum = 0;
double countTempData = 0.0;

float cpm = 0;
float rad = 0;

void handleGetData() {
  if (countTempData == 0) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
  } else {
    humidity = humiditySum / countTempData;
    temperature = tempSum / countTempData;
  }

  tempSum = 0;
  humiditySum = 0;
  countTempData = 0.0;

  cpm = calculateCPM();
  rad = calculateRadiation(cpm);

  if (DEBUG)
    Serial.println(cpm);
  if (DEBUG)
    Serial.print(rad);
  if (DEBUG)
    Serial.println(" µSv");

  if (DEBUG)
    Serial.println(humidity);
  if (DEBUG)
    Serial.println(temperature);
}

unsigned long previousMillis = 0;
const long interval = 5 * 60 * 1000;  //5 min in milisec

unsigned long previousMillisTemp = 0;
const long tempGatherInterval = 60 * 1000;  //1 min in milisec

void setup() {
  startTime = millis();
  if (DEBUG)
    Serial.begin(115200);
  delay(10);

  if (DEBUG)
    Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  WiFi.config(staticIP, gateway, subnet, dns);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (DEBUG)
      Serial.println("Connecting to WiFi...");
  }
  if (DEBUG)
    Serial.println("");
  if (DEBUG)
    Serial.println("WiFi connected");
  if (DEBUG)
    Serial.println(WiFi.localIP());
  dht.begin();

  pinMode(GEIGER_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), handleGeigerInterrupt, RISING);  //Callback when the geiger detects a click

  server.on("/data", HTTP_GET, [&]() {
    String response = "{\"Temperature\":" + String(temperature) + ",\"Humidity\":" + String(humidity) + ",\"CPM\":" + String(cpm) + ",\"Radiation\":" + String(rad) + "}";
    server.send(200, "application/json", response);
  });

  server.begin();
  if (DEBUG)
    Serial.println("HTTP server started");

  if (DEBUG)
    Serial.println("Gathering Data (5 min)...");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    handleGetData();
  }

  if (currentMillis - previousMillisTemp >= tempGatherInterval) {
    if (DEBUG)
      Serial.println("Collecting temp and humidity data.");
    previousMillisTemp = currentMillis;
    tempSum += dht.readTemperature();
    humiditySum += dht.readHumidity();

    countTempData++;
  }
}
