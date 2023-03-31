#include <Arduino.h>
#include "pins.h"
#include "lang.h"
#include "definitions.h"
#include <ArduinoJson.h>
#include <HTTPClient.h> // In-built
#include <WiFi.h>       // In-built

String Time_str = "--:--:--";
String Date_str = "-- --- ----";
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0, vref = 1100;
#define max_readings 24 // Limited to 3-days here, but could go to 5-days = 40

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
    // if (!DecodeWeather(http.getStream(), RequestType))
    //   return false;
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