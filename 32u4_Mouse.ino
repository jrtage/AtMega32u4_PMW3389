#include "CustomMouse.h"

byte initComplete=0;
volatile int xydat[2];
volatile byte movementflag=0;
byte testctr=0;
unsigned long currTime;
unsigned long timer;
unsigned long pollTimer;
unsigned long pollingRate = 1000; //Polling rate in us (1000us = 1kHz)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);  
  while(!Serial);

  pinMode(SS, OUTPUT);
  
  attachInterrupt(9, UpdatePointer, FALLING);

  SPI.begin();
  SPI.setDataMode(SPI_MODE3);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  
  performStartup();  

  delay(1000);
  
  dispRegisters();
  initComplete=9;

  Mouse.begin();
  buttons_init();

}

void loop() {
  currTime = micros();
  check_buttons_state();

  if(currTime > pollTimer){
    UpdatePointer();
    xydat[0] = convTwosComp(xydat[0]) * -1;
    xydat[1] = convTwosComp(xydat[1]) * -1;
      /*if(xydat[0] != 0 || xydat[1] != 0){
        Serial.print("x = ");
        Serial.print(xydat[0]);
        Serial.print(" | ");
        Serial.print("y = ");
        Serial.println(xydat[1]);
      }*/
    pollTimer = currTime + pollingRate;
    Mouse.move(xydat[0], xydat[1], 0);
  }

}

void UpdatePointer(void){
  if(initComplete==9){

    //write 0x01 to Motion register and read from it to freeze the motion values and make them available
    adns_write_reg(Motion, 0x01);
    adns_read_reg(Motion);

    xydat[0] = (int)adns_read_reg(Delta_X_L);
    xydat[1] = (int)adns_read_reg(Delta_Y_L);
    
    movementflag=1;
    }
}
