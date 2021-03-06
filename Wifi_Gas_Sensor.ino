#include <LiquidCrystal.h>

#include <SDS011.h>
#include <TempSensor.h>
#include <SoftwareSerial.h>
#include <avr/io.h>
#include <avr/interrupt.h>

//software serial object for wifi module
SoftwareSerial mySerialWifi(2, 3); // RX, TX
SoftwareSerial PmSerial(9, 8); // RX, TX

LiquidCrystal lcd(4, 5, 6, 10, 11, 12, 13);
TempSensor temp;
SDS011 PmSensor; 

//declare variables
uint8_t oneMinute = 60;
uint8_t elapsedSeconds = 0;
uint8_t elapsedMinutes = 0;
uint8_t sensorOffTime = 25;
bool sensorOn = false;
uint8_t warmUpTime = 5;
uint16_t O3count = 0;
uint8_t decimal = 0;
String APIkey = "E2TKCCBA49LSZK2Q";
String channelID = "379840";
float p10;
float p25;
int pm10int = 0;
int pm25int = 0;
boolean upload = false;
int num_data = 4;
int temperature = 0;
int humidity = 0;

int tempRow = 0;
int humidityRow = 1;
int O3Row = 2;
int pmRow = 3;
int tempColumn = 12;
int humidityColumn = 9;
int O3Column = 6;
int pm10Column = 5;
int pm25Column = 14;


void setup() 
{
  lcd.begin(20, 4);
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // set the data rate for the SoftwareSerial port
  mySerialWifi.begin(9600);
  PmSerial.begin(9600);
  
 //set port PD7 as an output
DDRD |= (1 << PD7);

InitTimer1();
InitADC();

//start the PM sensor off in sleep mode
PmSensor.Sleep();

//start with a sample, set elapsed time to 25 minutes
elapsedMinutes = sensorOffTime;

lcd.setCursor(0, tempRow);
lcd.print("Temperature: ");
lcd.setCursor(0, humidityRow);
lcd.print("Humidity: ");
lcd.setCursor(0, O3Row);
lcd.print("Ozone: ");
lcd.setCursor(0, pmRow);
lcd.print("PM10: ");
lcd.setCursor(10, pmRow);
lcd.print("2.5: ");
}

void loop() { // run over and over
    
//if enough time has passed, turn the sensor on to warm up
  if(elapsedMinutes >= sensorOffTime)
  {
    //turn the gas sensor on
    PORTD |= (1 << PD7);
    sensorOn = true;
  }

//if the sensor has been on for 5 minutes, sample, turn off the sensor, and upload data
if((sensorOn == true) && (elapsedMinutes >= warmUpTime + sensorOffTime))
  {
    //turn interrupts off
    cli();
    
    //turn the PM sensor on
    PmSensor.Wake();
    
    //get O3 data
    GetADC();
    
    //turn the O3 sensor off
    PORTD &= ~(1 << PD7);
    sensorOn = false;


    //get PM data
    ReadPm();
    p10 = p10*100;
    p25 = p25*100;
    pm10int = int(p10);
    pm25int = int(p25);
 
    //turn the pm sensor off
    PmSensor.Sleep();


    //get temp and humidity
    temp.Read();
    temperature =temp.GetTemp();
    humidity = temp.GetHumidity();
    
    //reset the counter
    elapsedMinutes = 0;

    //start interrupts
    sei();
    
    upload = true;
    
    //update the lcd
    lcd.setCursor(tempColumn, tempRow);
  lcd.print(temperature);
  lcd.setCursor(humidityColumn, humidityRow);
  lcd.print(humidity);
  lcd.setCursor(O3Column, O3Row);
  lcd.print(O3count);
  lcd.setCursor(pm10Column, pmRow);
  lcd.print(pm10int);
  lcd.setCursor(pm25Column, pmRow);
  lcd.print(pm25int);

    //upload the data
    for(uint8_t i = 0; i <= num_data; i++)
    {
     
      switch(i)
      {
        case 0  :  UploadData(O3count, i);
        Serial.print("03 = ");
        Serial.println(O3count);
        break;
        case 1  :  UploadData(pm10int, i);
        Serial.print("pm10 = ");
        Serial.println(pm10int);
        break;
        case 2  :  UploadData(pm25int, i);
        Serial.print("pm25 = ");
        Serial.println(pm25int);
        break;
        case 3  :  UploadData(temperature, i);
        Serial.print("temperature = ");
        Serial.println(temperature);
        break;
        case 4  :  UploadData(humidity, i);
        Serial.print("humidity = ");
        Serial.println(humidity);
        break;
      }
      
      //wait 15 seconds
      _delay_ms(20000);
      
    }

//put the wifi module to sleep, 30 seconds 
mySerialWifi.println("AT+GSLP=1500000");
  }
  
  
  if (mySerialWifi.available()) {
    Serial.write(mySerialWifi.read());
  }
  if (Serial.available()) {
    mySerialWifi.write(Serial.read());
  }
}

void ReadPm()
{
  byte buff;

  int value;
  int len = 0;
  int pm10_serial = 0;
  int pm25_serial = 0;
  int checksum_is;
  int checksum_ok = 0;
  int error = 1;
  
  while ((PmSerial.available() > 0) && (PmSerial.available() >= (10-len))) {
    buff = PmSerial.read();
    value = int(buff);
    switch (len) {
      case (0): if (value != 170) { len = -1; }; break;
      case (1): if (value != 192) { len = -1; }; break;
      case (2): pm25_serial = value; checksum_is = value; break;
      case (3): pm25_serial += (value << 8); checksum_is += value; break;
      case (4): pm10_serial = value; checksum_is += value; break;
      case (5): pm10_serial += (value << 8); checksum_is += value; break;
      case (6): checksum_is += value; break;
      case (7): checksum_is += value; break;
      case (8): if (value == (checksum_is % 256)) { checksum_ok = 1; } else { len = -1; }; break;
      case (9): if (value != 171) { len = -1; }; break;
    }
    len++;

  
    if (len == 10 && checksum_ok == 1) {
      p10 = (float)pm10_serial/10.0;
      p25 = (float)pm25_serial/10.0;
      len = 0; checksum_ok = 0; pm10_serial = 0.0; pm25_serial = 0.0; checksum_is = 0;
      error = 0;

    }
    //yield();
          
}

delay(3000);
  
}




//---------------------BEGIN UploadData FUNCTION---------------------//
//UploadData
//purpose: to upload values to thingspeak
//parameters: two integers, one for the data, the other for the thingspeak channel number
//returns: nothing
void UploadData(int _data, int _field)
{

  //establish connection with thingspeak.com
  mySerialWifi.println("AT+CIPSTART=\"TCP\",\"184.106.153.149\",80");
  delay(2000);

  //the length of the web address for thingspeak 40 characters, plus to for the newline and carriage return characters
  uint8_t webAddressLength = 42;
  //find the length of the data
  String dataToString = String(_data);
  uint8_t dataLength =dataToString.length()+webAddressLength;
  String dataLengthToString = String(dataLength);
  String dataLengthcmd = "AT+CIPSEND=";
  dataLengthcmd += dataLengthToString;
  Serial.println(dataLengthcmd);
  //send the data length
  mySerialWifi.println(dataLengthcmd);
  delay(2000);

  _field++;
  String dataToSend = "GET /update?key=E2TKCCBA49LSZK2Q&field";
  dataToSend += String(_field);
  dataToSend += "=";
  
  dataToSend += dataToString;
  mySerialWifi.println(dataToSend);
  delay(500);
  mySerialWifi.println("AT+CIPCLOSE");

}
//---------------------END UploadData FUNCTION---------------------//



//---------------------BEGIN initTimer1 FUNCTION---------------------//
//InitTImer1
//purpose: to count in 1 second intervals
//parameters: none
//returns: nothing
void InitTimer1(void)
{
  //disable interrupts
  cli();
  
  //clear timer registers
  TCCR1A = 0;
  TCCR1B = 0;
  
  //set the output compare register
  OCR1A = 15642;
  
  //turn on clear timer on compare mode
  TCCR1B |= (1 << WGM12);
  
  //prescale to 1024
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);
  
  //enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  
  //enable global interrupts
  sei();
}

//---------------------END initTimer1 FUNCTION---------------------//


//---------------------BEGIN initTimer1 ISR FUNCTION---------------------//
ISR(TIMER1_COMPA_vect)
{
  //increase the seconds count
    elapsedSeconds++; 
    
    //if one minutes is reached, increase the minute count
    if(elapsedSeconds == oneMinute)
    {
      elapsedMinutes++;
      elapsedSeconds = 0;
    }
    
}
//---------------------END initTimer1ISR FUNCTION---------------------//



//---------------------BEGIN InitADC FUNCTION---------------------//
void InitADC()
{  
  //reference voltage on AVCC 
  ADMUX |= (1 << REFS0);          
  
  //ADC clock prescaler / 128 
  ADCSRA |= (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
  
   //enable ADC
  ADCSRA |= (1 << ADEN); 
}

void GetADC()
{
  ADCSRA |= (1 << ADSC);
  loop_until_bit_is_clear(ADCSRA, ADSC);
  O3count = ADC;
}
//---------------------END InitADC FUNCTION---------------------//
