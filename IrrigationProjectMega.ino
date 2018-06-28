#include <SPI.h>
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

const unsigned short  WaterLevelSensorOnePin=24;
const unsigned short  WaterLevelSensorTwoPin=25;
const unsigned short  WaterLevelSensorThreePin=26;
const unsigned short  WaterLevelSensorFourPin=27;

int r,g,b;
int t = 0;
float temperature_c;
float temperature_f;
float humidity;
int tankOneWaterLevel = 0;
int tankTwoWaterLevel = 0;
int tankThreeWaterLevel = 0;
int tankFourWaterLevel = 0;
bool pumping = false;
bool filling = false;
int lastFillingHour = 0;
int lastFillingMinute = 0;
int fillingHour = 0;
int fillingMinute = 0;
int fillingMaxDuration = 30;
int fillingMaxFreqHours = 6;

// set up variables using the SD utility library functions:
File logFile;
Sd2Card card;
SdVolume volume;
SdFile root;

GravityRtc rtc;             //RTC Initialization
DFRobot_RGBLCD lcd(16,2);   //16 characters and 3 lines of show
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
  
  pinMode(WaterLevelSensorOnePin,INPUT);
  pinMode(WaterLevelSensorTwoPin,INPUT);
  pinMode(WaterLevelSensorThreePin,INPUT);
  pinMode(WaterLevelSensorFourPin,INPUT);
  
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
  
  tankOneWaterLevel = digitalRead(WaterLevelSensorOnePin);
  tankTwoWaterLevel = digitalRead(WaterLevelSensorTwoPin);
  tankThreeWaterLevel = digitalRead(WaterLevelSensorThreePin);
  tankFourWaterLevel = digitalRead(WaterLevelSensorFourPin);

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
    if(tankOneWaterLevel == 0 || tankTwoWaterLevel == 0 || tankThreeWaterLevel == 0 || tankFourWaterLevel == 0)
    {
      PrintLogLine("One of the tanks is low on water...");
  
      PrintLog("Water Level of Tank 1: ");
      PrintLogLine(tankOneWaterLevel);
    
      PrintLog("Water Level of Tank 2: ");
      PrintLogLine(tankTwoWaterLevel);
  
      PrintLog("Water Level of Tank 3: ");
      PrintLogLine(tankThreeWaterLevel);
  
      PrintLog("Water Level of Tank 4: ");
      PrintLogLine(tankFourWaterLevel);

      StartFillingTanks();
    }
    else
    {
      StopFillingTanks();
    }

    if(lastFillingHour > 0)
    {
        PrintLog("The tanks last completed filling at: ");
        PrintLog(lastFillingHour);
        PrintLog(":");
        PrintLog(lastFillingMinute);
        PrintLogLine("...");
    }
            
    if(filling == true)
    {
      PrintLog("The tanks started filling at: ");
      PrintLog(fillingHour);
      PrintLog(":");
      PrintLog(fillingMinute);
      PrintLogLine("...");
 
      if(fillingHour > 0)
      {
        if(fillingHour == rtc.hour && ((fillingMinute + fillingMaxDuration) < rtc.minute))
        {
          PrintLogLine("The tanks have been filling for over 20 minutes. The water level sensors may not be working. Stopping the fill...");
          StopFillingTanks();
        }
        else if((fillingHour + 1) == rtc.hour && ((fillingMinute - 60 + fillingMaxDuration) < rtc.minute))
        {
          PrintLogLine("The tanks have been filling for over 20 minutes. The water level sensors may not be working. Stopping the fill...");
          StopFillingTanks();
        }
      }  
    }
}


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

