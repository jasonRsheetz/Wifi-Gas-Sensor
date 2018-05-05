#include <SDS011.h>
#include <TempSensor.h>
#include <SoftwareSerial.h>
#include <avr/io.h>
#include <avr/interrupt.h>


//software serial object for wifi module
SoftwareSerial mySerialWifi(2, 3); // RX, TX



TempSensor temp;
SDS011 PmSensor;

//declare variables
uint8_t oneMinute = 60;
uint8_t elapsedSeconds = 0;
uint8_t elapsedMinutes = 0;
uint8_t sensorOffTime = 1;
bool sensorOn = false;
uint8_t warmUpTime = 1;
uint16_t O3count = 0;
uint8_t decimal = 0;
String APIkey = "E2TKCCBA49LSZK2Q";
String channelID = "379840";
float pm10 = 0;
float pm25 = 0;


void setup() 
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // set the data rate for the SoftwareSerial port
  mySerialWifi.begin(9600);
  
  
 //set port b2 as an output
DDRB |= (1 << PB2);

InitTimer1();
InitADC();

//start the PM sensor off in sleep mode
PmSensor.Sleep();
}

void loop() { // run over and over


//if enough time has passed, turn the sensor on to warm up
  if(elapsedMinutes >= sensorOffTime)
  {
    //turn the gas sensor on
    PORTB |= (1 << PB2);
    sensorOn = true;
    
    //turn the PM sensor on
    PmSensor.Wake();
  }

//if the sensor has been on for 5 minutes, sample, turn off the sensor, and upload data
if((sensorOn == true) && (elapsedMinutes >= warmUpTime))
  {
    //turn interrupts off
    cli();
    
    //get O3 data
    GetADC();
    
    //turn the O3 sensor off
    PORTB &= ~(1 << PB2);
    sensorOn = false;
    
    //get PM data
    PmSensor.Read(&pm10, &pm25);
    
    //turn the pm sensor off
    PmSensor.Sleep();
    
    //upload the data
    UploadOzone();
    UploadPM25();
    UploadPM10();
    UploadTemp();
    UploadHumidity();


    //reset the counter
    elapsedMinutes = 0;

    //start interrupts
    sei();
  }
  
  
  if (mySerialWifi.available()) {
    Serial.write(mySerialWifi.read());
  }
  if (Serial.available()) {
    mySerialWifi.write(Serial.read());
  }
}


//---------------------BEGIN UploadData FUNCTION---------------------//
//UploadData
//purpose: to upload values to thingspeak
//parameters: none
//returns: nothing
void UploadOzone()
{

//establish connection with thingspeak.com
mySerialWifi.println("AT+CIPSTART=\"TCP\",\"184.106.153.149\",80");
delay(3000);

//the length of the web address for thingspeak 40 characters, plus to for the newline and carriage return characters
uint8_t webAddressLength = 42;
//find the length of the data
String dataToString = String(O3count);
uint8_t dataLength =dataToString.length()+webAddressLength;
String dataLengthToString = String(dataLength);
String dataLengthcmd = "AT+CIPSEND=";
dataLengthcmd += dataLengthToString;
Serial.println(dataLengthcmd);
//send the data length
mySerialWifi.println(dataLengthcmd);
delay(3000);

String dataToSend = "GET /update?key=E2TKCCBA49LSZK2Q&field1=";
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
//---------------------END initTimer1 ISR FUNCTION---------------------//


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
