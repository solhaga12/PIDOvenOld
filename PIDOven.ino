/*

** NOTE: Tested on Arduino Nano whose I2C pins are A4==SDA, A5==SCL

*/
#include <Wire.h>
#include <DHT.h>
#include <DHT_U.h>
//#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define I2C_ADDR    0x3F // Address found with I2C Scanner
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

#define SECOND_LINE 0,1
#define CLEAR "                "
int n = 1;

LiquidCrystal_I2C	lcd(I2C_ADDR, 16, 2);

#define DHT_PIN 2
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

#define START 3
#define ON_OFF 3
#define SELECT 4
#define RIGHT 5
#define LEFT 6
#define MENU 7
#define AUTO 8
#define LED 13

#define HEATER_FRONT_LOW 9
#define HEATER_FRONT_HIGH 10
#define HEATER_BACK_LOW 11
#define HEATER_BACK_HIGH 12

#define K_FRONT_LOW 0
#define K_FRONT_HIGH 1
#define K_BACK_LOW 2
#define K_BACK_HIGH 3

// Membrane states
enum buttonIsStates
{
  IsPressed,
  IsReleased
};
enum buttonWasStates
{
  WasPressed,
  WasReleased
};
enum heaterStates
{
  HeaterOn,
  HeaterOff
};

buttonIsStates autoButtonIs = IsReleased;
bool autoButton = false;

buttonIsStates menuButtonIs = IsReleased;
buttonIsStates leftButtonIs = IsReleased;
buttonIsStates rightButtonIs = IsReleased;
buttonIsStates selectButtonIs = IsReleased;
buttonIsStates onOffButtonIs = IsReleased;

heaterStates frontHigh = HeaterOff;
heaterStates backLow = HeaterOff;
heaterStates backHigh = HeaterOff;

int time1s = 0;
int membraneBakeTimer = 0;
#define ONE_SECOND 830
#define MEMBRANE_BAKE_TIME 60 * 20
#define MEMBRANE_BAKE_TEMPERATURE 140
#define BAKE_STOP 0
#define BAKE_START 1

// K amplifiers 215,3. 41 uV/K gives then 8,83 mV/K.
// 5 V / 1024 equals 4,88 mV/bit
// so it should be 1,81 bit/K or 9 bits = 5 K
#define K2C 1.81


void setup()
{
  lcd.begin(16, 2);
  Serial.begin(9600);
                   
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home();
    
  dht.begin();

  // Heater relays
  digitalWrite(HEATER_FRONT_LOW, HIGH);
  digitalWrite(HEATER_FRONT_HIGH, HIGH);
  digitalWrite(HEATER_BACK_LOW, HIGH);
  digitalWrite(HEATER_BACK_HIGH, HIGH);

  pinMode(HEATER_FRONT_LOW, OUTPUT);
  pinMode(HEATER_FRONT_HIGH, OUTPUT);
  pinMode(HEATER_BACK_LOW, OUTPUT);
  pinMode(HEATER_BACK_HIGH, OUTPUT);

  // Keyboard
  pinMode(LED, OUTPUT);
  pinMode(START, INPUT_PULLUP);
  pinMode(SELECT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(MENU, INPUT_PULLUP);
  pinMode(AUTO, INPUT_PULLUP);


  // K Thermocouplers

}

void loop()
{
  int ovenTemperature;

  delay(ONE_SECOND);
  time1s++;

  ovenTemperature = getOvenTemperature();

  if (checkAutoButton(true)) 
  {
    lcd.home();
    lcd.printf("Membrane: %3d/%2d", MEMBRANE_BAKE_TEMPERATURE, MEMBRANE_BAKE_TIME / 60);
    if (ovenTemperature > MEMBRANE_BAKE_TEMPERATURE)
    {
      setHeaterFrontLowOff();
      setHeaterBackLowOff();
      if (BAKE_STOP == membraneBakeTimer)
      {
        membraneBakeTimer = BAKE_START;
      }
    }
    if (ovenTemperature < (MEMBRANE_BAKE_TEMPERATURE - 16))
    {
      setHeaterFrontLowOn();
      setHeaterBackLowOn();
    }
  }
  else
  {
    setHeaterFrontLowOff();
    setHeaterBackLowOff();
    membraneBakeTimer = BAKE_STOP;
    lcd.home();
    lcd.printf(CLEAR);
  }

  if (membraneBakeTimer)
  {
    membraneBakeTimer++;
    if (MEMBRANE_BAKE_TIME <= membraneBakeTimer)
    {
      setHeaterFrontLowOff();
      setHeaterBackLowOff();
      membraneBakeTimer = BAKE_STOP;
      lcd.home();
      lcd.printf("Membrane: done  ");
      checkAutoButton(false); // Ready to start a new cycle
    }
  }
}

bool checkAutoButton(bool status)
{
  if (false == status)
  {
    autoButtonIs = IsReleased;
    autoButton = false;
  }
  if (LOW == digitalRead(AUTO))
  {
    if (IsReleased == autoButtonIs)
    {
      autoButtonIs = IsPressed;
    }
  }
  else
  {
    if (IsPressed == autoButtonIs)
    {
      autoButtonIs = IsReleased;
      if (false == autoButton)
      {
        autoButton = true;
      }
      else
      {
        autoButton = false;
      }
    }
  }
  return autoButton;
}

void setHeaterFrontLowOn(void)
{
  digitalWrite(HEATER_FRONT_LOW, LOW);
}
void setHeaterFrontLowOff(void)
{
  digitalWrite(HEATER_FRONT_LOW, HIGH);
}

void setHeaterFrontHighOn(void)
{
  digitalWrite(HEATER_FRONT_HIGH, LOW);
}
void setHeaterFrontHighOff(void)
{
  digitalWrite(HEATER_FRONT_HIGH, HIGH);
}

void setHeaterBackLowOn(void)
{
  digitalWrite(HEATER_BACK_LOW, LOW);
}
void setHeaterBackLowOff(void)
{
  digitalWrite(HEATER_BACK_LOW, HIGH);
}

void setHeaterBackHighOn(void)
{
  digitalWrite(HEATER_BACK_HIGH, LOW);
}

void setHeaterBackHighOff(void)
{
  digitalWrite(HEATER_BACK_HIGH, HIGH);
}

int getOvenTemperature(void)
{
  int counts = 0;
  int maxCounts = 8;
  int kBackLow = 0;
  int kBackHigh = 0;
  int kFrontLow = 0;
  int kFrontHigh = 0;
  int dhtRoom;
  int ovenAll = 0;

  dhtRoom = (int)(dht.readTemperature());

  for (counts = 1; counts <= maxCounts; counts++)
  {
    kBackLow += (int)(analogRead(K_BACK_LOW) / K2C + dhtRoom);
    kBackHigh += (int)(analogRead(K_BACK_HIGH) / K2C + dhtRoom);
    kFrontLow += (int)(analogRead(K_FRONT_LOW) / K2C + dhtRoom);
    kFrontHigh += (int)(analogRead(K_FRONT_HIGH) / K2C + dhtRoom);
  }

  kBackLow /= maxCounts;
  kBackHigh /= maxCounts;
  kFrontLow /= maxCounts;
  kFrontHigh /= maxCounts;

  ovenAll = (kBackLow + kBackHigh + kFrontLow + kFrontHigh) / 4;

  lcd.home();
  lcd.setCursor(SECOND_LINE);
  lcd.printf("T %3d  %2d m %2d s", ovenAll, membraneBakeTimer/60, membraneBakeTimer % 60);
  return ovenAll;
}
