//-------------------------------------------------------LIBS--------------------------------------------------------------------------------------------------------------------
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <nRF24L01.h>
#include <RF24.h>
//-------------------------------------------------------PINS--------------------------------------------------------------------------------------------------------------------
// SPI (MISO,CLK, MISO are the default ones)
#define RFPIN_CE      8
#define RFPIN_CSN     9
// I2C (SDA, SCL are the default ones)
#define LED           2
//-------------------------------------------------------VARS-----------------------------------------------------------------------------------------------------------------
#define STATE_WHOAMI 0
#define STATE_VERIFY 1
#define STATE_DATA   2
#define STATE_RESET  3
uint8_t STATE = STATE_WHOAMI;

uint16_t rng = 0;
uint8_t ID = 255;

float  temp = 0;
uint32_t  pres = 0;
float  humi = 0;
uint32_t  gasR = 100000;

//-------------------------------------------------------RF24-----------------------------------------------------------------------------------------------------------------
RF24 radio(RFPIN_CE, RFPIN_CSN); // Create a Radio
const byte address[6] = "69420";

//-------------------------------------------------------BME680-----------------------------------------------------------------------------------------------------------------
#define SEALEVELPRESSURE_HPA (1013.25)
long bmeMeasComplete = 0;
Adafruit_BME680 bme;

void setup() {
    Serial.begin(115200);
    Serial.println("ExternalSensorDebug");

    while(!radio.begin()) Serial.println("RF - failed to Open");
    radio.setDataRate( RF24_250KBPS );
    radio.setRetries(3,5); // delay, count
    radio.openWritingPipe(address);

    bme.begin();
    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
    bmeMeasComplete = bme.beginReading();

    rng = random(0x0000,0xFFFF);
}

//====================

void loop() {
    if(STATE == STATE_WHOAMI){
      uint8_t data[21] = {0x5A,0xFF,(uint8_t)((rng)>>8),(uint8_t)(rng),0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA5};
      bool rslt = radio.write(&data, 21);
      if (rslt){
        Serial.println("RF - Registration sent");
        radio.openReadingPipe(0,address);
        radio.startListening();
        STATE = STATE_VERIFY;
      } else {Serial.println("RF - Station not reached");}
    }else
    if(STATE == STATE_VERIFY){
      if(radio.available()){
        uint8_t data[21];
        radio.read(&data, 21);
        for(uint8_t i = 0; i < 21; i++) Serial.print(String(data[i],HEX) + " ");
        Serial.println(" |>");
        if(data[0] == 0x5A && data[20] == 0xA5){
          if((data[1]) == 0xF0 && (data[2]<<8 | data[3]) == rng){
            ID = data[5];
            STATE = STATE_DATA;
          }
        }
      }
    }else
    if(STATE == STATE_DATA){

      if(millis()%1000 == 0){
        bool rslt = false;
        do{
          radio.stopListening();
          radio.setDataRate( RF24_250KBPS );
          radio.setRetries(3,5); // delay, count
          radio.openWritingPipe(address);

          
            if(bme.endReading()){
              temp = bme.temperature;
              pres = bme.pressure;
              humi = bme.humidity;
              gasR = bme.gas_resistance;
              bmeMeasComplete = bme.beginReading();
            }else{
              bmeMeasComplete = bme.beginReading();
            }
          
          
          uint8_t data[21] = {0x5A,0x0A,0x00,ID,(uint8_t)((uint32_t)(temp*100)>>24),(uint8_t)((uint32_t)(temp*100)>>16),(uint8_t)((uint32_t)(temp*100)>>8),(uint8_t)((uint32_t)(temp*100)),(uint8_t)(pres>>24),(uint8_t)(pres>>16),(uint8_t)(pres>>8),(uint8_t)(pres),(uint8_t)((uint32_t)(humi*100)>>24),(uint8_t)((uint32_t)(humi*100)>>16),(uint8_t)((uint32_t)(humi*100)>>8),(uint8_t)((uint32_t)(humi*100)),(uint8_t)(gasR>>24),(uint8_t)(gasR>>16),(uint8_t)(gasR>>8),(uint8_t)(gasR),0xA5};
          rslt = radio.write(&data, 21);
          
          radio.openReadingPipe(0,address);
          radio.startListening();
          delay(10);
        }while(!rslt);
      }
      
      uint8_t data[21];
      radio.read(&data, 21);
      if(data[0] == 0x5A){
        for(uint8_t i = 0; i < 21; i++) Serial.print(String(data[i],HEX) + " ");
        Serial.println(" |>");
      }
      
      if(data[0] == 0x5A && data[20] == 0xA5){
        if((data[1]) == 0x00 && data[2] == 0x00 && data[3] == ID){
          STATE = STATE_WHOAMI;
          ID = 255;
          rng = random(0x0000,0xFFFF);
          Serial.println("RF - RESET");
          delay(500);
        }
      }
      
    }
}
