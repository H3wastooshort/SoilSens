#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D6

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// WiFi connection
const char* ssid = "ahah";
const char* password = "nodeldynobe";

#define MQTT_SERVER "io.adafruit.com"
#define MQTT_USR "HACKER3000"
#define MQTT_KEY "nope"
//topic to publish to for the sensor
char* tempTopic = "HACKER3000/f/Weather";
char* humTopic = "HACKER3000/f/Weather";
char* baseTopic = "HACKER3000/f/Weather";


//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  digitalWrite(D4, !digitalRead(D4));
  Serial.println(topic);
  Serial.println(length);
}

WiFiServer server(80);
WiFiClient wifiClient;
PubSubClient client(MQTT_SERVER, 1883, callback, wifiClient);

int ulReqcount = 0;
int ulReconncount = 0;

void setup() {
  // put your setup code here, to run once:
  ulReqcount = 0;
  ulReconncount = 0;
  pinMode(D4, OUTPUT);
  // start serial
  Serial.begin(115200);
  Serial.println("WLAN Temperatur und Feuchtigkeitslogger by HACKER3000");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "E");
      delay(10);
    }
  }
  Serial.println("");
  // inital connect
  WiFi.mode(WIFI_STA);
  WiFiStart();

  sensors.begin();

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  server.begin();
  Serial.println("Server started");
}

void sendMQTT() {
  float temp = sensors.getTempCByIndex(0);
  int hum = analogRead(A0);
  char* st = "SoilTemp";
  char* sh = "SoilHum";
  char* ctemp = "";
  char* chum = "";
  char result[8]; // Buffer big enough for 7-character float
  dtostrf(temp, 6, 2, result); // Leave room for too large numbers!

  sprintf(ctemp, "%s = %s", st, result);
  sprintf(chum, "%s = %i", sh, hum);

  client.publish(tempTopic, ctemp);
  client.publish(humTopic, chum);
}

void WiFiStart() {
  ulReconncount++;

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  Serial.print("Reconnect Counter is ");
  Serial.println(ulReconncount);
  digitalWrite(D4, HIGH);
  WiFi.begin(ssid, password);
  delay(5000);
  digitalWrite(D4, LOW);
  //make sure we are connected to WIFI before attemping to reconnect to MQTT
  // Loop until we're reconnected to the MQTT server
  while (!client.connected()) {
    Serial.print("+");
    digitalWrite(D4, !digitalRead(D4));
    // Generate client name based on MAC address and last 8 bits of microsecond counter
    String clientName;
    clientName += "SoilSens-";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    clientName += macToStr(mac);

    //if connected, subscribe to the topic(s) we want to be notified about
    if (client.connect(clientName.c_str(), MQTT_USR, MQTT_KEY)) {
      //subscribe to topics here
    }
  }
  client.publish(baseTopic, "Hi");
}

void handleWeb() {

  ///////////////////////////////////
  // Check if a client has connected
  ///////////////////////////////////
  WiFiClient client = server.available();
  if (!client)
  {
    return;
  }
  digitalWrite(D4, HIGH);
  // Wait until the client sends some data
  Serial.println("new client");
  unsigned long ultimeout = millis() + 250;
  while (!client.available() && (millis() < ultimeout) )
  {
    delay(1);
  }
  if (millis() > ultimeout)
  {
    Serial.println("client connection time-out!");
    return;
  }

  /////////////////////////////////////
  // Read the first line of the request
  /////////////////////////////////////
  String sRequest = client.readStringUntil('\r');
  //Serial.println(sRequest);
  client.flush();

  // stop client, if request is empty
  if (sRequest == "")
  {
    Serial.println("empty request! - stopping client");
    client.stop();
    return;
  }

  // get path; end of path is either space or ?
  // Syntax is e.g. GET /?show=1234 HTTP/1.1
  String sPath = "", sParam = "", sCmd = "";
  String sGetstart = "GET ";
  int iStart, iEndSpace, iEndQuest;
  iStart = sRequest.indexOf(sGetstart);
  if (iStart >= 0)
  {
    iStart += +sGetstart.length();
    iEndSpace = sRequest.indexOf(" ", iStart);
    iEndQuest = sRequest.indexOf("?", iStart);

    // are there parameters?
    if (iEndSpace > 0)
    {
      if (iEndQuest > 0)
      {
        // there are parameters
        sPath  = sRequest.substring(iStart, iEndQuest);
        sParam = sRequest.substring(iEndQuest, iEndSpace);
      }
      else
      {
        // NO parameters
        sPath  = sRequest.substring(iStart, iEndSpace);
      }
    }
  }


  ///////////////////////////
  // format the html response
  ///////////////////////////
  String sResponse, sResponse2, sHeader;

  /////////////////////////////
  // format the html page for /
  /////////////////////////////
  if (sPath == "/")
  {
    ulReqcount++;
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");
    sResponse = "<html><head><meta http-equiv='refresh' content='5; URL=/'><title>Soil Data</title></head><body><h1>Soil Data</h1><p>";
    sResponse += "Temperature: ";
    sResponse += sensors.getTempCByIndex(0);
    sResponse += "<br> Humidity: ";
    sResponse += analogRead(A0);
    sResponse += "<br> Requests to this Site: ";
    sResponse += ulReqcount;
    sResponse += "<br> WiFi Reconnects: ";
    sResponse += ulReconncount;
    sResponse += "<br> Free RAM: ";
    sResponse += (uint32_t)system_get_free_heap_size();
    sResponse += "</p></body></html>";
    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length()).c_str());
    client.print(sResponse);
  }

  ///////////////////////////////////
  // format the html page for /static
  ///////////////////////////////////

  else if (sPath == "/static")
  {
    ulReqcount++;
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");
    sResponse = "<title>Soil Data</title></head><body><h1>Soil Data</h1><p>";
    sResponse += "Temperature: ";
    sResponse += sensors.getTempCByIndex(0);
    sResponse += "<br> Humidity: ";
    sResponse += analogRead(A0);
    sResponse += "<br> Requests to this Site: ";
    sResponse += ulReqcount;
    sResponse += "<br> WiFi Reconnects: ";
    sResponse += ulReconncount;
    sResponse += "<br> Free RAM: ";
    sResponse += (uint32_t)system_get_free_heap_size();
    sResponse += "</p></body></html>";
    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length()).c_str());
    client.print(sResponse);
  }

  else if (sPath == "/tmp")

    /////////////////////////////
    // format the html page for /temp
    /////////////////////////////
  {
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");
    sResponse = "<html><head><title>Soil Temperature</title></head><body><p>";
    sResponse += sensors.getTempCByIndex(0);
    sResponse += "</p></body></html>";
    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length()).c_str());
    client.print(sResponse);
  }
  else if (sPath == "/hum")

    /////////////////////////////
    // format the html page for /hum
    /////////////////////////////
  {
    sResponse = analogRead(A0);
    sResponse = "<html><head><title>Soil Humidity</title></head><body><p>";
    sResponse += analogRead(A0);
    sResponse += "</p></body></html>";
    client.print(MakeHTTPHeader(sResponse.length()).c_str());
    client.print(sResponse);
  }
  else
    ////////////////////////////
    // 404 for non-matching path
    ////////////////////////////
  {
    sResponse = "<html><head><meta http-equiv='refresh' content='5; URL=/'><title>404 Not Found</title></head><body><h1>Hast du gedacht hier gibt es noch Was</h1><p>Falsch gedacht!</p></body></html>";

    sHeader  = F("HTTP/1.1 404 Not found\r\nContent-Length: ");
    sHeader += sResponse.length();
    sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");

    // Send the response to the client
    client.print(sHeader);
    client.print(sResponse);
  }

  // and stop the client
  client.stop();
  Serial.println("Client disconnected");
  digitalWrite(D4, LOW);
}

//////////////////////////
// create HTTP 1.1 header
//////////////////////////
String MakeHTTPHeader(unsigned long ulLength)
{
  String sHeader;

  sHeader  = F("HTTP/1.1 200 OK\r\nContent-Length: ");
  sHeader += ulLength;
  sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");

  return (sHeader);
}

//generate unique name from MAC addr
String macToStr(const uint8_t* mac) {

  String result;

  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);

    if (i < 5) {
      result += ':';
    }
  }

  return result;
}

void loop() {
  // put your main code here, to run repeatedly:
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFiStart();
  }
  handleWeb();
  ArduinoOTA.handle();
  //reconnect if connection is lost
  if (!client.connected() && WiFi.status() == 3) {
    WiFiStart();
  }

  unsigned long thisMillis = 0;
  unsigned long lastMillis = 0;
  thisMillis = millis();

  if (thisMillis - lastMillis > 60000)
  {
    sendMQTT();
    lastMillis = thisMillis;
  }

  //maintain MQTT connection
  client.loop();
  //MUST delay to allow ESP8266 WIFI functions to run
  delay(10);
}