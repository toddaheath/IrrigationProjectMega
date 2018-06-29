#include <SD.h>
#include <SHT1x.h>
#include <Wire.h>
#include <DFRobot_RGBLCD.h>
#include <GravityRtc.h>

const unsigned short SDCardPin = 53; 

const unsigned short  SolenoidOnePin=45;

const unsigned short  WaterPumpOnePin=46;
const unsigned short  WaterPumpTwoPin=47;
const unsigned short  WaterPumpThreePin=48;
const unsigned short  WaterPumpFourPin=49;

const unsigned short  TempHumidityDataPin=40;
const unsigned short  TempHumidityClockPin=39;

const unsigned short  WaterLevelLowSensorOnePin=24;
const unsigned short  WaterLevelHighSensorOnePin=25;
const unsigned short  WaterLevelLowSensorTwoPin=26;
const unsigned short  WaterLevelHighSensorTwoPin=27;

int r,g,b;
int t = 0;
float temperature_c;
float temperature_f;
float humidity;
int tankOneWaterLevelLow = 0;
int tankOneWaterLevelHigh = 0;
int tankTwoWaterLevelLow = 0;
int tankTwoWaterLevelHigh = 0;
bool pumping = false;
bool filling = false;
int lastFillingHour = 0;
int lastFillingMinute = 0;
int fillingHour = 0;
int fillingMinute = 0;
int fillingMaxDuration = 30;
int fillingMaxFreqHours = 6;

File logFile;

GravityRtc rtc;
DFRobot_RGBLCD lcd(16,2);
SHT1x sht1x(TempHumidityDataPin, TempHumidityClockPin);

void setup() {
  Serial.begin(9600);

  Serial.println("Initializing SD card...");
  pinMode(SDCardPin, OUTPUT);
  digitalWrite(SDCardPin, HIGH);

   if (!SD.begin(SDCardPin)) {
    Serial.println("Initialization SD card failed!");
    return;
  }
  Serial.println("Initialization SD card done.");
  
  pinMode(WaterLevelLowSensorOnePin,INPUT);
  pinMode(WaterLevelHighSensorOnePin,INPUT);
  pinMode(WaterLevelLowSensorTwoPin,INPUT);
  pinMode(WaterLevelHighSensorTwoPin,INPUT);
  
  pinMode(WaterPumpOnePin, OUTPUT);
  pinMode(WaterPumpTwoPin, OUTPUT);
  pinMode(WaterPumpThreePin, OUTPUT);
  pinMode(WaterPumpFourPin, OUTPUT);

  pinMode(SolenoidOnePin, OUTPUT);
  
  rtc.setup();
  //Set the RTC time automatically: Calibrate RTC time by your computer time
  rtc.adjustRtc(F(__DATE__), F(__TIME__));
  
  // initialize the lcd screen
  lcd.init();

}

void loop() {
  rtc.read();

  //!!! The SD library supports a max file name size of 8.3 characters. !!!
  char logFilename[12]; 
  sprintf(logFilename, "%d%d%d%s", rtc.year, rtc.month, rtc.day, ".log");

  Serial.print("Current log filename: ");
  Serial.println(logFilename);

  if(SD.exists(logFilename))
  {
    Serial.println("Log file exists...");
  }
  else
  {
    Serial.println("Log file does not exist...");
  }
  

  logFile = SD.open(logFilename, FILE_WRITE);
  if (logFile)
  {
    Serial.println("Log file opened");
  }
  else
  {
    Serial.println("Error opening log file.");
  }
  
  r= (abs(sin(3.14*t/180)))*255;          //get R,G,B value
  g= (abs(sin(3.14*(t+60)/180)))*255;
  b= (abs(sin(3.14*(t+120)/180)))*255;
  t=t+3;
  lcd.setRGB(r, g, b);                  //Set R,G,B Value
  lcd.setCursor(0,0);
  lcd.print("Todd's Irrig Sys");
  lcd.setCursor(0,1);
  //R:0-255 G:0-255 B:0-255
  
  char lcdOutput[16];
  if ( rtc.second % 2 == 0 )
  {
    sprintf(lcdOutput, "%s%d%s%d%s%d%s", "Date: ", rtc.month, "/", rtc.day, "/", rtc.year, "  ");
  }
  else
  {
    sprintf(lcdOutput, "%s%d%s%d%s%d%s", "Time: ", rtc.hour, ":", rtc.minute, ":", rtc.second, "  ");
  }
  lcd.print(lcdOutput);

  char currentTimestamp[19];
  sprintf(currentTimestamp, "%d%s%d%s%d%s%d%s%d%s%d", rtc.year, "-", rtc.month, "-", rtc.day, "T", rtc.hour, ":", rtc.minute, ":", rtc.second);

  PrintLog("Current timestamp: ");
  PrintLogLine(currentTimestamp);
  
  tankOneWaterLevelLow = digitalRead(WaterLevelLowSensorOnePin);
  tankOneWaterLevelHigh = digitalRead(WaterLevelHighSensorOnePin);
  tankTwoWaterLevelLow = digitalRead(WaterLevelLowSensorTwoPin);
  tankTwoWaterLevelHigh = digitalRead(WaterLevelHighSensorTwoPin);

  // Read values from the sensor
  temperature_c = sht1x.readTemperatureC();
  temperature_f = sht1x.readTemperatureF();
  humidity = sht1x.readHumidity();

  PrintLog("Temperature (C): ");
  PrintLogLine(temperature_c);
  
  PrintLog("Temperature (F): ");
  PrintLogLine(temperature_f);
  
  PrintLog("Humidity: ");
  PrintLogLine(humidity);

  FillTanksIfNeeded();
  WaterIfNeeded();
  
  if(logFile)
  {
    logFile.close();
  }
  delay(1);
}

void FillTanksIfNeeded()
{
  if(filling == true)
  {
    PrintLog("The tanks started filling at: ");
    PrintLog(fillingHour);
    PrintLog(":");
    PrintLog(fillingMinute);
    PrintLogLine("...");
  }
    
  if(lastFillingHour > 0)
  {
    PrintLog("The tanks last completed filling at: ");
    PrintLog(lastFillingHour);
    PrintLog(":");
    PrintLog(lastFillingMinute);
    PrintLogLine("...");
  }

    //The tanks are self equalizing, so once one is low, the other tank should also be almost low.
    //High level and low level water sensors have only been installed on two of the tanks.
    //Since the tanks are setup to self equalize, this provides enough redundancy.
    if(tankOneWaterLevelLow == 0 || tankTwoWaterLevelLow == 0)
    {
      PrintLogLine("One of the tanks is low on water...");
  
      PrintLog("Water Low Level Reading of Tank 1: ");
      PrintLogLine(tankOneWaterLevelLow);
    
      PrintLog("Water High Level Reading of Tank 1: ");
      PrintLogLine(tankOneWaterLevelHigh);
  
      PrintLog("Water Low Level Reading of Tank 2: ");
      PrintLogLine(tankTwoWaterLevelLow);
  
      PrintLog("Water High Level Reading of Tank 2: ");
      PrintLogLine(tankTwoWaterLevelLow);

      StartFillingTanks();
    }
    //The tanks are self equalizing, so once one is full, the other tank should also be almost full.
    else if(tankOneWaterLevelLow > 0 || tankTwoWaterLevelLow > 0)
    {
      PrintLogLine("One of the tanks is full...");
      StopFillingTanks();
    }
    else
    {
      StopFillingTanksWhenFillingForExcessiveTime();
    }
}

//If a sensor fails, I don't want the tanks to keep trying to fill beyond full. So this is a saftey measure.
void StopFillingTanksWhenFillingForExcessiveTime()
{
  if(filling == true)
    {
      if(fillingHour > 0)
      {
        if(fillingHour == rtc.hour && ((fillingMinute + fillingMaxDuration) < rtc.minute))
        {
          PrintLog("The tanks have been filling for over ");
          PrintLog(fillingMaxDuration);
          PrintLogLine(" minutes. The water level sensors may not be working. Stopping the fill...");
          StopFillingTanks();
        }
        else if((fillingHour + 1) == rtc.hour && ((fillingMinute - 60 + fillingMaxDuration) < rtc.minute))
        {
          PrintLog("The tanks have been filling for over ");
          PrintLog(fillingMaxDuration);
          PrintLogLine(" minutes. The water level sensors may not be working. Stopping the fill...");
          StopFillingTanks();
        }
      }  
    }
}

//Right now this is set up to run the pumps for 5 minutes at 7am and 6pm. This will be updated as needed.
void WaterIfNeeded()
{
  if(rtc.hour == 7)
  {
    //Water for 5 minutes to start.
    if(rtc.minute >= 0 and rtc.minute <= 5)
    {
      //Morning watering...
      StartPumpWater();
    }
    else
    {
      //Morning watering complete...
      StopPumpWater();
    }
  }
  else if(rtc.hour == 18)
  {
    //Water for 5 minutes to start.
    if(rtc.minute >= 0 and rtc.minute <= 5)
    {
      //Evening watering...
      StartPumpWater();
    }
    else
    {
      //Evening watering complete...
      StopPumpWater();
    }
  }
}

//This fills the tanks. 
//If filling is already occuring the request will be ignored.
//If a caller tries to call this method more than once every six hours the request will be ignored.
void StartFillingTanks()
{
  if(filling == false)
  {
    if(lastFillingHour == 0 || (lastFillingHour + fillingMaxFreqHours) < rtc.hour)
    {
      PrintLogLine("Filling tanks...");
      digitalWrite(SolenoidOnePin, HIGH);
      PrintLogLine("Filling started...");
      filling = true;
      fillingHour = rtc.hour;
      fillingMinute = rtc.minute;     
    }
    else
    {
      PrintLogLine("The tanks were filled less than six hours ago, the tanks will not be filled yet.");
    }
  }
}

//This stops filling the tanks. 
//If filling is not currently occuring the request will be ignored.
void StopFillingTanks()
{
  if(filling == true)
  {
    PrintLogLine("Stopping tank filling...");
    digitalWrite(SolenoidOnePin, LOW);
    PrintLogLine("Filling stopped...");
    filling = false;
    lastFillingHour = fillingHour;
    lastFillingMinute = fillingMinute;
    fillingHour = 0;
    fillingMinute = 0;
  }
}

//This turns on the water pumps for all four tanks to start irrigating.
void StartPumpWater()
{
  if(pumping == false)
  {
    PrintLogLine("Starting pumps...");
    digitalWrite(WaterPumpOnePin, HIGH);
    digitalWrite(WaterPumpTwoPin, HIGH);
    digitalWrite(WaterPumpThreePin, HIGH);
    digitalWrite(WaterPumpFourPin, HIGH);
    PrintLogLine("Pumps started...");
    pumping = true;
  }
}

//This turns off the water pumps on all four tanks to stop irrigating.
void StopPumpWater()
{
  if(pumping == true)
  {
    PrintLogLine("Stopping pumps...");
    digitalWrite(WaterPumpOnePin, LOW);
    digitalWrite(WaterPumpTwoPin, LOW);
    digitalWrite(WaterPumpThreePin, LOW);
    digitalWrite(WaterPumpFourPin, LOW);
    PrintLogLine("Pumps stopped...");
    pumping = false;
  }
}

void PrintLog(const char * input)
{
  if(logFile)
  {
    logFile.print(input);
  } 
  Serial.print(input);
}

void PrintLog(int input)
{
  if(logFile)
  {
    logFile.print(input);
  } 
  Serial.print(input);
}

void PrintLog(float input)
{
  if(logFile)
  {
    logFile.print(input);
  } 
  Serial.print(input);
}

void PrintLogLine(const char * input)
{
  if(logFile)
  {
    logFile.println(input);
  }
  Serial.println(input);
}

void PrintLogLine(int input)
{
  if(logFile)
  {
    logFile.println(input);
  }
  Serial.println(input);
}

void PrintLogLine(float input)
{
  if(logFile)
  {
    logFile.println(input);
  }
  Serial.println(input);
}

