#include "CustomMouse.h"



int btn_pins[NUMBTN] = { BTN1, BTN2 };
char btn_keys[NUMBTN] = { MOUSE_LEFT, MOUSE_RIGHT };

// button pins & debounce buffers
bool btn_state[NUMBTN] = { false, false };
uint8_t btn_buffers[NUMBTN] = {0xFF, 0xFF};

unsigned long lastButtonCheck = 0;

void adns_com_begin(){
  digitalWrite(SS, LOW);
}

void adns_com_end(){
  digitalWrite(SS, HIGH);
}

byte adns_read_reg(byte reg_addr){
  adns_com_begin();
  
  // send adress of the register, with MSBit = 0 to indicate it's a read
  SPI.transfer(reg_addr & 0x7f );
  delayMicroseconds(100); // tSRAD
  // read data
  byte data = SPI.transfer(0);
  
  delayMicroseconds(1); // tSCLK-NCS for read operation is 120ns
  adns_com_end();
  delayMicroseconds(19); //  tSRW/tSRR (=20us) minus tSCLK-NCS

  return data;
}

void adns_write_reg(byte reg_addr, byte data){
  adns_com_begin();
  
  //send adress of the register, with MSBit = 1 to indicate it's a write
  SPI.transfer(reg_addr | 0x80 );
  //sent data
  SPI.transfer(data);
  
  delayMicroseconds(20); // tSCLK-NCS for write operation
  adns_com_end();
  delayMicroseconds(100); // tSWW/tSWR (=120us) minus tSCLK-NCS. Could be shortened, but is looks like a safe lower bound 
}

void adns_upload_firmware(){
  // send the firmware to the chip, cf p.18 of the datasheet
  Serial.println("Uploading firmware...");

  //Write 0 to Rest_En bit of Config2 register to disable Rest mode.
  adns_write_reg(Config2, 0x20);
  
  // write 0x1d in SROM_enable reg for initializing
  adns_write_reg(SROM_Enable, 0x1d); 
  
  // wait for more than one frame period
  delay(10); // assume that the frame rate is as low as 100fps... even if it should never be that low
  
  // write 0x18 to SROM_enable to start SROM download
  adns_write_reg(SROM_Enable, 0x18); 
  
  // write the SROM file (=firmware data) 
  adns_com_begin();
  SPI.transfer(SROM_Load_Burst | 0x80); // write burst destination adress
  delayMicroseconds(15);
  
  // send all bytes of the firmware
  unsigned char c;
  for(int i = 0; i < firmware_length; i++){ 
    c = (unsigned char)pgm_read_byte(firmware_data + i);
    SPI.transfer(c);
    delayMicroseconds(15);
  }

  //Read the SROM_ID register to verify the ID before any other register reads or writes.
  adns_read_reg(SROM_ID);

  //Write 0x00 to Config2 register for wired mouse or 0x20 for wireless mouse design.
  adns_write_reg(Config2, 0x00);

  // set initial CPI resolution
  adns_write_reg(Config1, 0x15);
  
  adns_com_end();
}

void performStartup(void){
  adns_com_end(); // ensure that the serial port is reset
  adns_com_begin(); // ensure that the serial port is reset
  adns_com_end(); // ensure that the serial port is reset
  adns_write_reg(Power_Up_Reset, 0x5a); // force reset
  delay(50); // wait for it to reboot
  // read registers 0x02 to 0x06 (and discard the data)
  
  adns_read_reg(Motion);
  adns_read_reg(Delta_X_L);
  adns_read_reg(Delta_X_H);
  adns_read_reg(Delta_Y_L);
  adns_read_reg(Delta_Y_H);
  // upload the firmware
  adns_upload_firmware();
  delay(10);
  adns_write_reg(Power_Up_Reset, 0x5a);
  Serial.println("Optical Chip Initialized");
}

void dispRegisters(void){
  int oreg[7] = {
    0x00,0x3F,0x2A,0x02  };
  char* oregname[] = {
    "Product_ID","Inverse_Product_ID","SROM_Version","Motion"  };
  byte regres;

  digitalWrite(SS,LOW);

  int rctr=0;
  for(rctr=0; rctr<4; rctr++){
    SPI.transfer(oreg[rctr]);
    delay(1);
    //Serial.println("---");
    //Serial.println(oregname[rctr]);
    //Serial.println(oreg[rctr],HEX);
    regres = SPI.transfer(0);
    //Serial.println(regres,BIN);  
    //Serial.println(regres,HEX);  
    delay(1);
  }
  digitalWrite(SS,HIGH);
}

int convTwosComp(int b){
  //Convert from 2's complement
  if(b & 0x80){
    b = -1 * ((b ^ 0xff) + 1);
    }
  return b;
}

void buttons_init(){
  for(int i=0;i < NUMBTN; i++)
  {
    pinMode(btn_pins[i], INPUT_PULLUP);
  }
}

void check_buttons_state(){
  unsigned long elapsed = micros() - lastButtonCheck;
  
  // Update at a period of 1/8 of the DEBOUNCE time
  if(elapsed < (DEBOUNCE * 1000UL / 8))
    return;
  
  lastButtonCheck = micros();
    
  // Fast Debounce (works with 0 latency most of the time)
  for(int i=0;i < NUMBTN ; i++)
  {
    int state = digitalRead(btn_pins[i]);
    btn_buffers[i] = btn_buffers[i] << 1 | state; 

    if(!btn_state[i] && btn_buffers[i] == 0xFE)  // button pressed for the first time
    {
      Mouse.press(btn_keys[i]);
      btn_state[i] = true;
    }
    else if( (btn_state[i] && btn_buffers[i] == 0x01) // button released after stabilized press
            // force release when consequent off state (for the DEBOUNCE time) is detected 
            || (btn_state[i] && btn_buffers[i] == 0xFF) ) 
    {
      Mouse.release(btn_keys[i]);
      btn_state[i] = false;
    }
  }
}

