#include <Arduino.h>
#include <OneWire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "C:\Users\magnu\OneDrive\Documenten\Arduino\MQTT-Temperature\src\main\secrets.h"

#define DS18S20_Pin D2
float temperatuur = 0;


OneWire ds(DS18S20_Pin); 
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long beginTijd = 0;
unsigned long tijdsDuur = 8000;
bool delayActief = false;

void beginDelay() {
  beginTijd = millis();
  Serial.println("Delay gestart...");
}

bool delayEinde() {
  if (delayActief && (millis() - beginTijd >= tijdsDuur)) {
    delayActief = false;
    Serial.println("Delay klaar.");
    return true;
  }
  Serial.println("Delay klaar.");
  return false;
}

void setup_wifi()
{
  WiFi.begin(ssid, wachtwoord);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Verbinding maken met WiFi...");
  }
  Serial.println("Verbonden met WiFi");
}

void callback(char *onderwerp, byte *payload, unsigned int lengte)
{
  Serial.println("Bericht ontvangen op onderwerp: " + String(onderwerp));
}

void opnieuwVerbinden()
{
  while (!client.connected())
  {
    Serial.println("MQTT-broker | Verbinden...");
    if (client.connect("ESP32Client", mqttGebruiker, mqttWachtwoord))
    {
      Serial.println("MQTT-broker | Verbonden");
      client.subscribe(mqttOnderwerp);
    }
    else
    {
      Serial.println("MQTT-broker | Niet verbonden. Retry in 5s");
      delay(1000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Uit deepsleep.");
  } else {
    setup_wifi();
    client.setServer(mqttServer, mqttPoort);
    client.setCallback(callback);
  }

}

float getTemp()     //  https://github.com/milesburton/Arduino-Temperature-Control-Library/blob/master/examples/oneWireSearch/oneWireSearch.ino
{
  // Geeft de temperatuur terug van één DS18S20 in graden Celsius

  byte data[12];
  byte adres[8];

  if (!ds.search(adres))
  {
    // Geen sensoren
    ds.reset_search();
    Serial.println("1");    
    return -1000;
  }

  if (OneWire::crc8(adres, 7) != adres[7])
  {
    Serial.println("CRC is niet geldig!");
    Serial.println("2");
    return -1000;
  }

  if (adres[0] != 0x10 && adres[0] != 0x28)
  {
    Serial.print("Apparaat wordt niet herkend");
    Serial.println("3");
    return -1000;
  }

  ds.reset();
  ds.select(adres);
  ds.write(0x44, 1); // start conversie, met parasitaire voeding aan het einde

  byte aanwezig = ds.reset();
  ds.select(adres);
  ds.write(0xBE); // Lees krasblok

  for (int i = 0; i < 9; i++)
  { // we hebben 9 bytes nodig
    data[i] = ds.read();
  }

  ds.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempLezen = ((MSB << 8) | LSB); // met tweecomplementen
  float temperatuur = tempLezen / 16;

  return temperatuur;
}

void tempcontrole_foto() {
  static int count = 0;
  count++;
  Serial.print("Temperature reading #");
  Serial.println(count);
  float temperatuur = getTemp();
  Serial.print("Temperature: ");
  Serial.println(temperatuur);
  //Serial.println("FOTO GENOMEN");
  if (!delayActief) {
    beginDelay();
    delayActief = true;
    return;
  }

  if (millis() - beginTijd >= tijdsDuur) {
    if (delayEinde()) {
      if (temperatuur >= 20) {
        Serial.println("FOTO GENOMEN");
        char payload[10];
        dtostrf(temperatuur, 4, 2, payload); // Zet float om naar string
        if (client.publish(mqttOnderwerp, payload)) {
          Serial.println("Temperatuur Gepubliceerd");
        } else {
          Serial.println("Niet gelukt met Publiseren");
        }
        delay(2000);
        esp_sleep_enable_timer_wakeup(10 * 1000000); //60s
        esp_deep_sleep_start();
      } else{
        Serial.println("FOTO NIET GENOMEN");
        char payload[10];
        dtostrf(temperatuur, 4, 2, payload); // Zet float om naar string
        if (client.publish(mqttOnderwerp, payload)) {
          Serial.println("Temperatuur Gepubliceerd");
        } else {
          Serial.println("Niet gelukt met Publiseren");
        }
        delay(2000);
        esp_sleep_enable_timer_wakeup(15 * 1000000);  //900s 15 min
        esp_deep_sleep_start();
      }
    }
  }
}




// INTEGRATION TEST
void integrationTest() {
  Serial.println("\nIntegration Test:");
  tempcontrole_foto(); // Starts the delay
  delay(8000); // Wait for 8 seconds
  tempcontrole_foto(); // Should publish the temperature
  Serial.println("Expected: Capture and publish image");
  Serial.println("Actual: ..."); 
}

// STRESS TEST
void stressTest() {
  Serial.println("\nStress Test:");
  Serial.println("Reducing delay for stress test...");
  tijdsDuur = 1000; // Reduce delay duration for stress test

  for (int i = 0; i < 5; i++) {
    Serial.println("\nReading temperature...");
    temperatuur = getTemp();
    Serial.print("Temperature: ");
    Serial.println(temperatuur);
    
    Serial.println("Performing temperature control and photo check...");
    tempcontrole_foto();
    delay(2000); // Add a delay to simulate processing time
  }

  Serial.println("Resetting delay for normal operation...");
  tijdsDuur = 8000; // Reset delay duration to normal value
}

//    https://www.softwaretestinghelp.com/the-difference-between-unit-integration-and-functional-testing/ 


void loop()
{
  //integrationTest(); 
  //stressTest(); 
  temperatuur = getTemp();
  //Serial.println(temperatuur);
  tempcontrole_foto();
  if (!client.connected())
  {
    opnieuwVerbinden();
  }
  client.loop();


}
