
#include  <SoftwareSerial.h>
SoftwareSerial CAT(10, 11); // RX, TX

#define DEBUG 1
//#define MIRRORCAT 1
//#define USELCD 1

#define BROADCAST_ADDRESS    0x00 //Broadcast address
#define CONTROLLER_ADDRESS   0xE0 //Controller address

#define START_BYTE       0xFE //Start byte
#define STOP_BYTE       0xFD //Stop byte

#define CMD_TRANS_FREQ      0x00 //Transfers operating frequency data
#define CMD_TRANS_MODE      0x01 //Transfers operating mode data

#define CMD_READ_FREQ       0x03 //Read operating frequency data
#define CMD_READ_MODE       0x04 //Read operating mode data

#define CMD_WRITE_FREQ       0x05 //Write operating frequency data
#define CMD_WRITE_MODE       0x06 //Write operating mode data

const uint32_t decMulti[]    = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

#define BAUD_RATES_SIZE 4
const uint16_t baudRates[BAUD_RATES_SIZE]       = {19200, 9600, 4800, 1200};

uint8_t  radio_address;     //Transiever address
uint16_t  baud_rate;        //Current baud speed
uint32_t  readtimeout;      //Serial port read timeout
uint8_t  read_buffer[12];   //Read buffer
uint32_t  frequency;        //Current frequency in Hz

const char* mode[] = {"LSB","USB","AM","CW","FSK","FM","WFM"};

void configRadioBaud(uint16_t  baudrate)
{
  if(baud_rate != baudrate){
    CAT.end();
    CAT.begin(baudrate);
  }
  
  baud_rate = baudrate;

  switch (baud_rate) {
    case 4800:
       readtimeout = 50000;
       break;
    case 9600:
       readtimeout = 40000;
       break;
    case 19200:
       readtimeout = 30000;
       break;
    default:
       readtimeout = 100000;
       break;
  }
}

uint8_t readLine(void)
{
  uint8_t byte;
  uint8_t counter = 0;
  uint32_t ed = readtimeout;

  while (true)
  {
    while (!CAT.available()) {
      if (--ed == 0)return 0;
    }
    ed = readtimeout;
    byte = CAT.read();
    if (byte == 0xFF)continue; //TODO skip to start byte instead
#ifdef MIRRORCAT
    Serial.print(byte, BYTE);
#endif

    read_buffer[counter++] = byte;
    if (STOP_BYTE == byte) break;

    if (counter >= sizeof(read_buffer))return 0;
  }
  return counter;
}

void sendCatRequest(uint8_t requestCode)
{
  uint8_t req[] = {START_BYTE, START_BYTE, radio_address, CONTROLLER_ADDRESS, requestCode, STOP_BYTE};
#ifdef DEBUG    
  Serial.print(">");
#endif
  for (uint8_t i = 0; i < sizeof(req); i++) {
    CAT.write(req[i]);
#ifdef DEBUG    
    if (req[i] < 16)Serial.print("0");
    Serial.print(req[i], HEX);
    Serial.print(" ");
#endif
  }
#ifdef DEBUG    
  Serial.println();
#endif
}

bool searchRadio()
{

  for(uint8_t baud=0; baud<BAUD_RATES_SIZE; baud++){
#ifdef DEBUG    
    Serial.print("Try baudrate ");
    Serial.println(baudRates[baud]);
#endif    
    configRadioBaud(baudRates[baud]);
    sendCatRequest(CMD_READ_FREQ);
    
    if (readLine() > 0)
    {
      if(read_buffer[0]==START_BYTE && read_buffer[1]==START_BYTE){
        radio_address = read_buffer[3];
      }
      return true;
    }
  }

  radio_address = 0xFF;
  return false;
}

void printFrequency(void)
{
  frequency = 0;
  //FE FE E0 42 03 <00 00 58 45 01> FD ic-820
  //FE FE 00 40 00 <00 60 06 14> FD ic-732
  for (uint8_t i = 0; i < 5; i++) {
    if(read_buffer[9 - i] == 0xFD)continue;//spike
#ifdef DEBUG    
    if (read_buffer[9 - i] < 16)Serial.print("0");
    Serial.print(read_buffer[9 - i], HEX);
#endif

    frequency += (read_buffer[9 - i] >> 4) * decMulti[i * 2];
    frequency += (read_buffer[9 - i] & 0x0F) * decMulti[i * 2 + 1];
  }
#ifdef DEBUG    
  Serial.println();
#endif  
}

void printMode(void)
{
  //FE FE E0 42 04 <00 01> FD
#ifdef DEBUG    
  Serial.println(mode[read_buffer[5]]);
#endif
  //read_buffer[6] -> 01 - Wide, 02 - Medium, 03 - Narrow

}

void setup()
{
  Serial.begin(115200);
  configRadioBaud(9600);
  radio_address = 0x00;

  while(radio_address == 0x00) {
    if (!searchRadio()) {
#ifdef DEBUG    
      Serial.println("Radio not found");
#endif      
    } else {
#ifdef DEBUG    
      Serial.print("Radio found at ");
      Serial.print(radio_address, HEX);
      Serial.println();
#endif

      sendCatRequest(CMD_READ_FREQ);
      delay(100);
      sendCatRequest(CMD_READ_MODE);
      break;
    }
  }
  
}

void processCatMessages()
{
  /*
    <FE FE E0 42 04 00 01 FD  - LSB
    <FE FE E0 42 03 00 00 58 45 01 FD  -145.580.000

    FE FE - start bytes
    00/E0 - target address (broadcast/controller)
    42 - source address
    00/03 - data type
    <data>
    FD - stop byte
  */

  while (CAT.available()) {
    uint8_t knowncommand = 1;
    uint8_t r;
    if (readLine() > 0) {
      if (read_buffer[0] == START_BYTE && read_buffer[1] == START_BYTE) {
        if (read_buffer[3] == radio_address) {
          if (read_buffer[2] == BROADCAST_ADDRESS) {
            switch (read_buffer[4]) {
              case CMD_TRANS_FREQ:
                printFrequency();
                break;
              case CMD_TRANS_MODE:
                printMode();
                break;
              default:
                knowncommand = false;
            }
          } else if (read_buffer[2] == CONTROLLER_ADDRESS) {
            switch (read_buffer[4]) {
              case CMD_READ_FREQ:
                printFrequency();
                break;
              case CMD_READ_MODE:
                printMode();
                break;
              default:
                knowncommand = false;
            }
          }
        } else {
#ifdef DEBUG    
          Serial.print(read_buffer[3]);
          Serial.println(" also on-line?!");
#endif          
        }
      }
    }

#ifdef DEBUG    
    //if(!knowncommand){
    Serial.print("<");
    for (uint8_t i = 0; i < sizeof(read_buffer); i++) {
      if (read_buffer[i] < 16)Serial.print("0");
      Serial.print(read_buffer[i], HEX);
      Serial.print(" ");
      if (read_buffer[i] == STOP_BYTE)break;
    }
    Serial.println();
    //}
#endif    
  }

#ifdef MIRRORCAT
  while(Serial.available()){
    CAT.print(Serial.read(), BYTE);
  }
#endif  
}

void loop()
{
  processCatMessages();
}



