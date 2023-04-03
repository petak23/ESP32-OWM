#include <Arduino.h>
#include "pins.h"
#include "forecast_record.h"
#include "lang.h"
#include "definitions.h"
#include <ArduinoJson.h>
#include <HTTPClient.h> // In-built
#include <WiFi.h>       // In-built

String Time_str = "--:--:--";
String Date_str = "-- --- ----";
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0, vref = 1100;
#define max_readings 24 // Limited to 3-days here, but could go to 5-days = 40
Forecast_record_type WxConditions[1];
Forecast_record_type WxForecast[max_readings];

boolean UpdateLocalTime()
{
  struct tm timeinfo;
  char time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 5000))
  { // Wait for 5-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin = timeinfo.tm_min;
  CurrentSec = timeinfo.tm_sec;
  // See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M")
  {
    sprintf(day_output, "%s, %02u %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo); // Creates: '@ 14:05:49'   and change from 30 to 8 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    sprintf(time_output, "%s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '@ 02:05:49pm'
    sprintf(time_output, "%s", update_time);
  }
  Date_str = day_output;
  Time_str = time_output;
  return true;
}

bool DecodeWeather(WiFiClient &json, String Type)
{
  Serial.print(F("\nCreating object...and "));
  DynamicJsonDocument doc(64 * 1024);                      // allocate the JsonDocument 65536 = 64*1024
  DeserializationError error = deserializeJson(doc, json); // Deserialize the JSON document
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println(" Decoding " + Type + " data");
  if (Type == "weather")
  {
    // All Serial.println statements are for diagnostic purposes and some are not required, remove if not needed with //
    WxConditions[0].lon = root["coord"]["lon"].as<float>();
    Serial.println(" Lon: " + String(WxConditions[0].lon));
    WxConditions[0].lat = root["coord"]["lat"].as<float>();
    Serial.println(" Lat: " + String(WxConditions[0].lat));
    WxConditions[0].Main0 = root["weather"][0]["main"].as<const char *>();
    Serial.println("Main: " + String(WxConditions[0].Main0));
    WxConditions[0].Forecast0 = root["weather"][0]["description"].as<const char *>();
    Serial.println("For0: " + String(WxConditions[0].Forecast0));
    // WxConditions[0].Forecast1   = root["weather"][1]["description"].as<const char*>(); Serial.println("For1: " + String(WxConditions[0].Forecast1));
    // WxConditions[0].Forecast2   = root["weather"][2]["description"].as<const char*>(); Serial.println("For2: " + String(WxConditions[0].Forecast2));
    WxConditions[0].Icon = root["weather"][0]["icon"].as<const char *>();
    Serial.println("Icon: " + String(WxConditions[0].Icon));
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();
    Serial.println("Temp: " + String(WxConditions[0].Temperature));
    WxConditions[0].Pressure = root["main"]["pressure"].as<float>();
    Serial.println("Pres: " + String(WxConditions[0].Pressure));
    WxConditions[0].Humidity = root["main"]["humidity"].as<float>();
    Serial.println("Humi: " + String(WxConditions[0].Humidity));
    WxConditions[0].Low = root["main"]["temp_min"].as<float>();
    Serial.println("TLow: " + String(WxConditions[0].Low));
    WxConditions[0].High = root["main"]["temp_max"].as<float>();
    Serial.println("THig: " + String(WxConditions[0].High));
    WxConditions[0].Windspeed = root["wind"]["speed"].as<float>();
    Serial.println("WSpd: " + String(WxConditions[0].Windspeed));
    WxConditions[0].Winddir = root["wind"]["deg"].as<float>();
    Serial.println("WDir: " + String(WxConditions[0].Winddir));
    WxConditions[0].Cloudcover = root["clouds"]["all"].as<int>();
    Serial.println("CCov: " + String(WxConditions[0].Cloudcover)); // in % of cloud cover
    WxConditions[0].Visibility = root["visibility"].as<int>();
    Serial.println("Visi: " + String(WxConditions[0].Visibility)); // in metres
    WxConditions[0].Rainfall = root["rain"]["1h"].as<float>();
    Serial.println("Rain: " + String(WxConditions[0].Rainfall));
    WxConditions[0].Snowfall = root["snow"]["1h"].as<float>();
    Serial.println("Snow: " + String(WxConditions[0].Snowfall));
    // WxConditions[0].Country     = root["sys"]["country"].as<const char*>();            Serial.println("Ctry: " + String(WxConditions[0].Country));
    WxConditions[0].Sunrise = root["sys"]["sunrise"].as<int>();
    Serial.println("SRis: " + String(WxConditions[0].Sunrise));
    WxConditions[0].Sunset = root["sys"]["sunset"].as<int>();
    Serial.println("SSet: " + String(WxConditions[0].Sunset));
    WxConditions[0].Timezone = root["timezone"].as<int>();
    Serial.println("TZon: " + String(WxConditions[0].Timezone));
  }
  if (Type == "forecast")
  {
    Serial.println(json);
    Serial.print(F("\nReceiving Forecast period - ")); //------------------------------------------------
    JsonArray list = root["list"];
    for (byte r = 0; r < max_readings; r++)
    {
      Serial.println("\nPeriod-" + String(r) + "--------------");
      WxForecast[r].Dt = list[r]["dt"].as<int>();
      WxForecast[r].Temperature = list[r]["main"]["temp"].as<float>();
      Serial.println("Temp: " + String(WxForecast[r].Temperature));
      WxForecast[r].Low = list[r]["main"]["temp_min"].as<float>();
      Serial.println("TLow: " + String(WxForecast[r].Low));
      WxForecast[r].High = list[r]["main"]["temp_max"].as<float>();
      Serial.println("THig: " + String(WxForecast[r].High));
      WxForecast[r].Pressure = list[r]["main"]["pressure"].as<float>();
      Serial.println("Pres: " + String(WxForecast[r].Pressure));
      WxForecast[r].Humidity = list[r]["main"]["humidity"].as<float>();
      Serial.println("Humi: " + String(WxForecast[r].Humidity));
      // WxForecast[r].Forecast0         = list[r]["weather"][0]["main"].as<char*>();        Serial.println("For0: " + String(WxForecast[r].Forecast0));
      // WxForecast[r].Forecast1         = list[r]["weather"][1]["main"].as<char*>();        Serial.println("For1: " + String(WxForecast[r].Forecast1));
      // WxForecast[r].Forecast2         = list[r]["weather"][2]["main"].as<char*>();        Serial.println("For2: " + String(WxForecast[r].Forecast2));
      WxForecast[r].Icon = list[r]["weather"][0]["icon"].as<const char *>();
      Serial.println("Icon: " + String(WxForecast[r].Icon));
      // WxForecast[r].Description       = list[r]["weather"][0]["description"].as<char*>(); Serial.println("Desc: " + String(WxForecast[r].Description));
      // WxForecast[r].Cloudcover        = list[r]["clouds"]["all"].as<int>();               Serial.println("CCov: " + String(WxForecast[r].Cloudcover)); // in % of cloud cover
      // WxForecast[r].Windspeed         = list[r]["wind"]["speed"].as<float>();             Serial.println("WSpd: " + String(WxForecast[r].Windspeed));
      // WxForecast[r].Winddir           = list[r]["wind"]["deg"].as<float>();               Serial.println("WDir: " + String(WxForecast[r].Winddir));
      WxForecast[r].Rainfall = list[r]["rain"]["3h"].as<float>();
      Serial.println("Rain: " + String(WxForecast[r].Rainfall));
      WxForecast[r].Snowfall = list[r]["snow"]["3h"].as<float>();
      Serial.println("Snow: " + String(WxForecast[r].Snowfall));
      WxForecast[r].Period = list[r]["dt_txt"].as<const char *>();
      Serial.println("Peri: " + String(WxForecast[r].Period));
    }
    //------------------------------------------
    float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure; // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0;                   // Remove any small variations less than 0.1
    WxConditions[0].Trend = "=";
    if (pressure_trend > 0)
      WxConditions[0].Trend = "+";
    if (pressure_trend < 0)
      WxConditions[0].Trend = "-";
    if (pressure_trend == 0)
      WxConditions[0].Trend = "0";

    // if (Units == "I")
    //   Convert_Readings_to_Imperial();
  }
  return true;
}

bool obtainWeatherData(WiFiClient &client, const String &RequestType)
{
  const String units = (Units == "M" ? "metric" : "imperial");
  client.stop(); // close connection before sending a new request
  HTTPClient http;
  // String uri = "/data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + APIKEY + "&mode=json&units=" + units + "&lang=" + Language;
  String uri = "/data/2.5/" + RequestType + "?id=" + CITY_ID + "&APPID=" + APIKEY + "&mode=json&units=" + units + "&lang=" + LANGUAGE;
  if (RequestType != "weather")
  {
    uri += "&cnt=" + String(max_readings);
  }
  http.begin(client, server, 80, uri); // http.begin(uri,test_root_ca); //HTTPS example connection
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    if (!DecodeWeather(http.getStream(), RequestType))
      return false;
    Serial.println(http.getStream());
    client.stop();
    http.end();
    return true;
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}

void InitialiseSystem()
{
  // StartTime = millis();
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println(String(__FILE__) + "\nStarting...");
  /*epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer)
    Serial.println("Memory alloc failed!");
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);*/
}

boolean SetupTime()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                                 // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                   // Set the TZ environment variable
  delay(100);
  return UpdateLocalTime();
}

uint8_t StartWiFi()
{
  Serial.println("\r\nConnecting to: " + String(WIFI_SSID));
  IPAddress dns(8, 8, 8, 8); // Use Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(500);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else
    Serial.println("WiFi connection *** FAILED ***");
  return WiFi.status();
}

void setup()
{
  delay(5000);

  InitialiseSystem();
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true)
  {
    /*bool WakeUp = false;
    if (WakeupHour > SleepHour)
      WakeUp = (CurrentHour >= WakeupHour || CurrentHour <= SleepHour);
    else
      WakeUp = (CurrentHour >= WakeupHour && CurrentHour <= SleepHour);*/
    // if (WakeUp)
    //{
    byte Attempts = 1;
    bool RxWeather = false;
    bool RxForecast = false;
    WiFiClient client; // wifi client object
    while ((RxWeather == false || RxForecast == false) && Attempts <= 2)
    { // Try up-to 2 time for Weather and Forecast data
      if (RxWeather == false)
        RxWeather = obtainWeatherData(client, "weather");
      if (RxForecast == false)
        RxForecast = obtainWeatherData(client, "forecast");
      Attempts++;
    }
    Serial.println("Received all weather data...");
    /*if (RxWeather && RxForecast)
    {                     // Only if received both Weather or Forecast proceed
      StopWiFi();         // Reduces power consumption
      epd_poweron();      // Switch on EPD display
      epd_clear();        // Clear the screen
      DisplayWeather();   // Display the weather data
      edp_update();       // Update the display to show the information
      epd_poweroff_all(); // Switch off all power to EPD
    }*/
    //}
  }
  // BeginSleep();
}

void loop()
{
  // put your main code here, to run repeatedly:
}