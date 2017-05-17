//autor: Maksymilian Lach
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <dht11.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

const char* ssid = "FunBox-B0B2";
const char* password = "krzak112233";
const char* host = "stacjapogody.lo2przemysl.edu.pl";

//DS18B20 setup
#define ONE_WIRE_BUS 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

//DHT11 setup
dht11 DHT11;
#define DHT11PIN 13

//BMP180 setup
Adafruit_BMP280 bme;

void setup() {

  //******** main program **********
  //Musi być umieszczone w funkcji setup, ponieważ na koniec wywołujemy tryp powerdown, a po wybudzeniu procesor
  //restartuje sie

  //DS18B20 inicjalizacja
  sensors.begin();
  sensors.getAddress(insideThermometer, 0);
  sensors.setResolution(insideThermometer, 11);

  // BME280 inicjalizacja
  bme.begin(0x76);

  //Podłączamy się do routera
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED);

  //Odczytujemy czas z serwera funkcją time_request [definicja w dalszej części kodu]
  String time_str = time_request();
  //time_str jest formatu: hh:mm:ss
  int hh = atoi(time_str.substring(0, time_str.indexOf(":")).c_str());
  time_str.remove(0, time_str.indexOf(":") + 1);
  int mm = atoi(time_str.substring(0, time_str.indexOf(":")).c_str());
  time_str.remove(0, time_str.indexOf(":") + 1);
  int ss = atoi(time_str.c_str());
  int sleepTimeS = 0; //czas na jaki musimy uśpić moduł

  //Odczytujemy wszystkie dane z czujników.
  sensors.requestTemperatures();
  float ds1_measure1 = sensors.getTempC(insideThermometer);
  int check_flag = DHT11.read(DHT11PIN);
  int humidity = DHT11.humidity;

  // 1)jeśli jest godzina hh:00 hh:15, hh:30, hh:45, to można wysłac dane, zostawiamy 5 sekundowy margines błędu
  // 2)lub jeśli jest hh:59:ss, hh:14:ss, hh:29:ss, hh:44:ss, gdzie ss >= 45 to możemy zaczekać odpowienią ilość sekund
  //   na wysłanie
  if ((!mm % 15 && ss <= 5) || ((mm == 14 || mm == 29 || mm == 44 || mm == 59) && ss >= 45) )
  {
    float bme1_pressure = bme.readPressure() / 100;
    if (ss <= 5) //czyli maksymalnie 5s za późno
    {
      post_request(ds1_measure1, bme1_pressure, 0, humidity); // wysyałmy post request
      sleepTimeS = 14 * 60 + 55 - ss; //obliczamy czas na jaki usypiamy moduł
    }
    else
    {
      //jest za wczesnie; czekamy
      delay((60 - ss) * 1000);
      //wysyłamy
      post_request(ds1_measure1, bme1_pressure, 0, humidity);
      sleepTimeS = 15 * 60 - 5;
    }

  }
  else
  {
    //jesli godzina jest inna niż w warunkach wyżej to obliczamy czas do na usypienie
    sleepTimeS = 60;
    mm++;
    while (mm % 15)
    {
      sleepTimeS += 60;
      mm++;
    }
    if (ss <= 5)
    {
      sleepTimeS -= 4 + ss;
    }
    else
    {
      sleepTimeS -= 5 + ss;
    }
  }
  ESP.deepSleep(sleepTimeS * 1000000);
}
void loop()
{

}
String time_request() //wynkcja odczytująca czas s servera
{
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort))
  {
    return "error";
  }
  client.println("GET /dodacgodzina/index.php HTTP/1.1");
  client.println("Host: stacjapogody.lo2przemysl.edu.pl");
  client.println();
  delay(250);
  while (client.available())
  {
    String line = client.readStringUntil('\r');
    if (line.indexOf("Now:") > 0)
    {
      line = line.substring(line.indexOf(":") + 1);
      return line;
    }
  }
  return "error";
}

void post_request(float temp, int cisn, int opad, int wilg) // funkcja wysyłąjąca dane
{
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort))
    return;
  String data = "temp=" + String(temp, DEC) + "&cisn=" + String(cisn, DEC)
                + "&opad" + String(opad, DEC) + "&wilg=" + String(wilg, DEC);
  client.println("POST /dodacbaza/index.php HTTP/1.1");
  client.println("Host: stacjapogody.lo2przemysl.edu.pl");
  //client.println("Accept: */*");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(data.length());
  client.println();
  client.print(data);
  delay(500);
  if (client.connected()) {
    client.stop();
  }
  client.read();
}

