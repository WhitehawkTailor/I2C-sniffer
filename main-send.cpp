/**

    @author Ákos Szabó (Whitehawk Tailor) - aaszabo@gmail.com
    This is an I2C sniffer that logs traffic on I2C BUS.
    IDE: Visual Studio Code + PlatformIO
    Platform: Arduino for ESP32
    Board: Heltec WiFi Lora32 v2
    GPIO12 SDA
    GPIO13 SCL
    This is not connecting to the BUS as an I2C device. It is neither a Master, nor a Slave.
    It just listens and logs the communication.
    The application does not interrupt the I2C communication.
    Two pins as input are attached to SDC and SDA lines.
    Since the I2C communications runs on 400kHz so,
    the tool that runs this program should be fast.
    This was tested on an ESP32 bord Heltec WiFi Lora32 v2
    ESP32 core runs on 240MHz.
    It means there are 600 ESP32 cycles during one I2C clock tick.
    The program uses interrupts to detect
    the raise edge of the SCL - bit transfer
    the falling edge of SDA if SCL is HIGH- START
    the raise edge of SDA if SCL id HIGH - STOP
    In the interrupt routines there is just a few line of code
    that mainly sets the status and stores the incoming bits.
    Otherwise the program gets timeout panic in interrupt handler and
    restart the CPU.
    v1.0
    Basic operation and put message to eh serial output
    v1.1
    See additional parts marked with the version title v1.1
    The additionals makes possible to send the logged I2C communication content to a server via HTTP request.
    */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
//#define I2CTEST //use it to run a blinking LED test on SDA and SCL pins

#define PIN_SDA 12 //BLUE
#define PIN_SCL 13 //Yellow

#define I2C_IDLE 0
//#define I2C_START 1
#define I2C_TRX 2
//#define I2C_RESP 3
//#define I2C_STOP 4

static volatile byte i2cStatus = I2C_IDLE;//Status of the I2C BUS
static uint32_t lastStartMillis = 0;//stoe the last time
static volatile byte dataBuffer[9600];//Array for storing data of the I2C communication
static volatile uint16_t bufferPoiW=0;//points to the first empty position in the dataBufer to write
static uint16_t bufferPoiR=0;//points to the position where to start read from
static volatile byte bitCount = 0;//counter of bit appeared on the BUS
static volatile uint16_t byteCount =0;//counter of bytes were writen in one communication.
static volatile byte i2cBitD =0;//Container of the actual SDA bit
static volatile byte i2cBitD2 =0;//Container of the actual SDA bit
static volatile byte i2cBitC =0;//Container of the actual SDA bit
static volatile byte i2cClk =0;//Container of the actual SCL bit
static volatile byte i2cAck =0;//Container of the last ACK value
static volatile byte i2cCase =0;//Container of the last ACK value
static volatile uint16_t falseStart = 0;//Counter of false start events
//static volatile byte respCount =0;//Auxiliary variable to help detect next byte instead of STOP
//these variables just for statistic reasons
static volatile uint16_t sclUpCnt = 0;//Auxiliary variable to count rising SCL
static volatile uint16_t sdaUpCnt = 0;//Auxiliary variable to count rising SDA
static volatile uint16_t sdaDownCnt = 0;//Auxiliary variable to count falling SDA

//v1.1
//Store the begining of the communication string. Modify it according to your setup.
String msgToServer = "http://192.168.0.19:5544/I2cServer?ADDR=0100000&data=";
const char* ssid = "ssid";
const char* password = "pass";
const int deviceID = 1;

////////////////////////////
//// Interrupt handlers
/////////////////////////////

/**

    This is the rising SCL interrupt handler
    Rising SCL makes reading the SDA

*/
void IRAM_ATTR i2cTriggerOnRaisingSCL()
{
sclUpCnt++;

//is it a false trigger?
if(i2cStatus==I2C_IDLE)
{
  falseStart++;
  //return;//this is not clear why do we have so many false START
}


//get the value from SDA
i2cBitC =  digitalRead(PIN_SDA);

//decide wherewe are and what to do with incoming data
i2cCase = 0;//normal case

if(bitCount==8)//ACK case
  i2cCase = 1;

if(bitCount==7 && byteCount==0 )// R/W if the first address byte
  i2cCase = 2;

bitCount++;

switch (i2cCase)
{
  case 0: //normal case
    dataBuffer[bufferPoiW++] = '0' + i2cBitC;//48
  break;//end of case 0 general
  case 1://ACK
    if(i2cBitC)//1 NACK SDA HIGH
      {
        dataBuffer[bufferPoiW++] = '-';//45
      }
      else//0 ACK SDA LOW
      {
        dataBuffer[bufferPoiW++] = '+';//43
      } 
    byteCount++;
    bitCount=0;
  break;//end of case 1 ACK
  case 2:
    if(i2cBitC)
    {
      dataBuffer[bufferPoiW++] = 'R';//82
    }
    else
    {
      dataBuffer[bufferPoiW++] = 'W';//87
    }
  break;//end of case 2 R/W

}//end of switch

}//END of i2cTriggerOnRaisingSCL()

/**

    This is the SDA interrupt handler

    This is for recognizing I2C START and STOP

    This is called when the SDA line is changing

    It is decided inside the function wheather it is a rising or falling change.

    If SCL is on High then the falling change is a START and the rising is a STOP.

    If SCL is LOW, then this is the action to set a data bit, so nothing to do.
    */
    void IRAM_ATTR i2cTriggerOnChangeSDA()
    {
    //make sure that the SDA is in stable state
    do
    {
    i2cBitD = digitalRead(PIN_SDA);
    i2cBitD2 = digitalRead(PIN_SDA);
    } while (i2cBitD!=i2cBitD2);

    if(i2cBitD)//RISING if SDA is HIGH (1)
    {

     i2cClk = digitalRead(PIN_SCL);
     if(i2cStatus=!I2C_IDLE && i2cClk==1)//If SCL still HIGH then it is a STOP sign
     {      
      //i2cStatus = I2C_STOP;
      i2cStatus = I2C_IDLE;
      bitCount = 0;
      byteCount = 0;
      bufferPoiW--;
      dataBuffer[bufferPoiW++] = 's';//115
      dataBuffer[bufferPoiW++] = '\n'; //10
     }
     sdaUpCnt++;

    }
    else //FALLING if SDA is LOW
    {

     i2cClk = digitalRead(PIN_SCL);
     if(i2cStatus==I2C_IDLE && i2cClk)//If SCL still HIGH than this is a START
     {
      i2cStatus = I2C_TRX;
      //lastStartMillis = millis();//takes too long in an interrupt handler and caused timeout panic and CPU restart
      bitCount = 0;
      byteCount =0;
      dataBuffer[bufferPoiW++] = 'S';//83 STOP
      //i2cStatus = START;    
     }
     sdaDownCnt++;

    }
    }//END of i2cTriggerOnChangeSDA()

////////////////////////////////
//// Functions
////////////////////////////////

/**

    Reset all important variable
    */
    void resetI2cVariable()
    {
    i2cStatus = I2C_IDLE;
    bufferPoiW=0;
    bufferPoiR=0;
    bitCount =0;
    falseStart = 0;
    }//END of resetI2cVariable()

/**

    @desc Write out the buffer to the serial console

*/
void processDataBuffer()
{
if(bufferPoiW == bufferPoiR)//There is nothing to say
return;

uint16_t pw = bufferPoiW;
//print out falseStart
Serial.printf("\nprocessDataBuffer\nSCL up: %d SDA up: %d SDA down: %d false start: %d\n", sclUpCnt, sdaUpCnt, sdaDownCnt, falseStart);
//print out the content of the buffer 
for(int i=bufferPoiR; i< pw; i++)
{
  Serial.write(dataBuffer[i]);
  bufferPoiR++;   
}

//if there is no I2C action in progress and there wasn't during the Serial.print then buffer was printed out completly and can be reset.
if(i2cStatus == I2C_IDLE && pw==bufferPoiW)
{
  bufferPoiW =0;
  bufferPoiR =0;
} 

}//END of processDataBuffer()

/**

    v1.1
    @desc This method returns a String that contains everything that is available in the dataBuffer
    This function also resets the buffer, so this and processDataBuffer cannot be used in the same time.
    @ret String value. Returns the whole content of the dataBuffer as a string.

*/
String getStringFromDataBuffer()
{
String ret = "";
if(bufferPoiW == bufferPoiR)//There is nothing to say
return "";

uint16_t pw = bufferPoiW;
//print out falseStart
Serial.printf("\ngetStringFromDataBuffer\nSCL up: %d SDA up: %d SDA down: %d false start: %d\n", sclUpCnt, sdaUpCnt, sdaDownCnt, falseStart);
//print out the content of the buffer 
for(int i=bufferPoiR; i< pw; i++)
{
  ret += (char)dataBuffer[i];   
  bufferPoiR++;   
}

//if there is no I2C action in progress and there wasn't during the Serial.print then buffer was printed out completly and can be reset.
if(i2cStatus == I2C_IDLE && pw==bufferPoiW)
{
  bufferPoiW =0;
  bufferPoiR =0;
}

return ret;

}//END of processDataBuffer()

/**

    v1.1
    This is an empty method.
    Write here any communication with a server

*/
void sendMsgOut(String argStr)
{
  argStr.replace(" ","");
  Serial.println();
  Serial.println(argStr);
  Serial.println();
}

/////////////////////////////////
//// MAIN entry point of the program
/////////////////////////////////
void setup()
{
    Serial.begin(115200);
    
    #ifdef I2CTEST
    pinMode(PIN_SCL, OUTPUT);   
    pinMode(PIN_SDA, OUTPUT); 
    #else
    //Define pins for SCL, SDA
    pinMode(PIN_SCL, INPUT_PULLUP);   
    pinMode(PIN_SDA, INPUT_PULLUP);
    //pinMode(PIN_SCL, INPUT);   
    //pinMode(PIN_SDA, INPUT);


    //reset variables
    resetI2cVariable();

    //Atach interrupt handlers to the interrupts on GPIOs
    attachInterrupt(PIN_SCL, i2cTriggerOnRaisingSCL, RISING); //trigger for reading data from SDA
    attachInterrupt(PIN_SDA, i2cTriggerOnChangeSDA, CHANGE); //for I2C START and STOP


    //v1.1 Extension for WiFi sending
    WiFi.begin(ssid, password);
    Serial.println("Connecting");
    while(WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());


    #endif



}//END of setup

/**

    LOOP
    v1.0
    @desc Writes I2C mcommunication to the serial if there is any.
    
    v1.1
    @desc uses sendMsgOut to send I2C mcommunication to the specified server and format given by the msgToServer variable
    */
    void loop()
    {

    #ifdef I2CTEST //do this in case it is testing the lines
    digitalWrite(PIN_SCL, HIGH); //13 Yellow
    digitalWrite(PIN_SDA, HIGH); //12 Blue
    delay(500);
    digitalWrite(PIN_SCL, HIGH); //13 Yellow
    digitalWrite(PIN_SDA, LOW); //12 Blue
    delay(500);
    #else
    //if it is in IDLE, then write out the databuffer to the serial consol
    if(i2cStatus == I2C_IDLE)
    {
    //v1.0
    //processDataBuffer();

      //v1.1
      //Get the content of the dataBuffer as a string and put it into a message
      //Send out the message
      //make sure that the msgToServer string variable is set properly above in the global variable section.
      sendMsgOut( msgToServer + getStringFromDataBuffer());

      //Delay the loop
         Serial.print("\rStart delay    ");   
         delay(5000);
         Serial.print("\rEnd delay    ");
         delay(500);
     }

    #endif

}//END of loop
