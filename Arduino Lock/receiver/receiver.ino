#include <RH_ASK.h>
#include <EEPROM.h>
#include <SPI.h>

RH_ASK driver(2000, 3, 2, 4);
const char id[] = "id:002";

const unsigned long MAX_IDS = 10;
String ids[MAX_IDS];
unsigned long nextIdIndex = 0;
unsigned long nextIdAddr = 0;

const char idIdentifier[] = "id:";
const char commandIdentifier[] = "command:";
const char valueIdentifier[] = "value:";

const int pairBtn = 5;
const int red = 7;
const int green = 8;

bool locked = true;
bool lastLocked = locked;

void setup(){
  Serial.begin(9600);

  // read all ids from EEPROM
  for(int i=0; i<MAX_IDS; i++){
    static int nextAddr = 0;
    char buf[100];
    EEPROM.get(nextAddr, buf);

    Serial.print("Reading: ");
    Serial.print(buf);
    Serial.print(", Address: ");
    Serial.println(nextAddr);

    if(strstr(buf, idIdentifier) != NULL){
      ids[i] = String(buf);
    }else{
      nextIdIndex = i;
      nextIdAddr = nextAddr;
      Serial.print("nextIdIndex: ");
      Serial.println(nextIdIndex);
      Serial.print("nextIdAddr: ");
      Serial.println(nextIdAddr);
      break;
    }
    
    nextAddr += strlen(buf)+1;
  }
  
  driver.init();

  pinMode(MISO, OUTPUT);  // have to send on master in, *slave out*
  SPCR |= _BV(SPE); // turn on SPI in slave mode
  SPI.attachInterrupt();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE3);

  pinMode(pairBtn, INPUT_PULLUP);
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);

  digitalWrite(red, HIGH);
  digitalWrite(green, LOW);  
  
  Serial.println("Ready to go...");
}

void loop(){
  checkRFReceive();
  checkBtn();

  if(lastLocked != locked){
    lastLocked = locked;
    updateState(locked);
  }
}

void checkRFReceive(){
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if(driver.recv(buf, &buflen)){
    String rcv;
    for(int i=0; i<buflen; i++)
      rcv += (char)buf[i];

    Serial.print("Received: ");
    Serial.println(rcv);

    if(strstr(rcv.c_str(), idIdentifier) != NULL){
      char id[100];
      char command[100];
      char value[100];
    
      char* pch = strtok(rcv.c_str(), ",");
      while (pch != NULL){
        if(strstr(pch, idIdentifier) != NULL){
          strcpy(id, pch);
        }
        else if(strstr(pch, commandIdentifier) != NULL){
          String pCommand = String(pch);
          pCommand.remove(0, strlen(commandIdentifier));      
          strcpy(command, pCommand.c_str());
        }
        else if(strstr(pch, valueIdentifier) != NULL){
          String pCommand = String(pch);
          pCommand.remove(0, strlen(valueIdentifier));      
          strcpy(value, pCommand.c_str());
        }
        
//        Serial.println (pch);
        pch = strtok (NULL, ",");
      }

      // add id to list if id doesn't exist
      if(!checkIdExists(id)){
        if(!strstr(id, idIdentifier) && !strcmp(command, "pair")){
          if(nextIdIndex < MAX_IDS && nextIdAddr < EEPROM.length()-20){  // -20 is used for extra space to prevent overflow
            Serial.print("Writing: ");    
            Serial.print(id);
            Serial.print(", Address: ");
            Serial.println(nextIdAddr);
            
            EEPROM.put(nextIdAddr, id);
            nextIdAddr += strlen(id)+1;
  
            ids[nextIdIndex] = String(id);
            nextIdIndex++;
          }
        }
      }

      // execute commands if id exists
      if(checkIdExists(id)){
        if(!strcmp(command, "lock")){
          if(!strcmp(value, "1")){
            locked = true;
          }
          if(!strcmp(value, "0")){
            locked = false;
          }          
        }
      }
    }
  }
}

void checkBtn() {
  static int state;
  static int lastButtonState;
  
  static unsigned long lastDebounceTime = 0;
  static unsigned long debounceDelay = 50;
  static unsigned long longPressDelay = 2000;
  
  int reading = digitalRead(pairBtn);

  if (reading != lastButtonState){
    lastDebounceTime = millis();
    lastButtonState = reading;
  }

  if ((millis() - lastDebounceTime) > longPressDelay) {
    if (state < 2) {
      state = 2;
      
      if (reading == LOW) {
        Serial.println("LONG PRESS");
      }
    }
  }
  else if ((millis() - lastDebounceTime) > debounceDelay) {
    if (state < 1) {
      state = 1;

      if (reading == LOW) {
        Serial.println("SHORT PRESS");

        char command[100];
        strcpy(command, id);
        strcat(command, ",command:pair");
        sendMessage(command);
      }
    }
  } else {
    state = 0;
  }
}

void updateState(bool state){
  locked = state;

  if(locked){
    Serial.println("Locked");
    digitalWrite(red, HIGH);
    digitalWrite(green, LOW);
  } else {
    Serial.println("Unlocked");
    digitalWrite(red, LOW);
    digitalWrite(green, HIGH);
  }
  
  char command[100];
  strcpy(command, id);
  strcat(command, ",command:lock,value:");
  strcat(command, locked ? "1" : "0");
  sendMessage(command);
}

void sendMessage(const char* msg){
  Serial.print("Sending: ");
  Serial.println(msg);
  for(int i=0; i<10; i++)
    driver.send((uint8_t*)msg, strlen(msg));
  driver.waitPacketSent();
}

bool checkIdExists(const char* id){
  for(int i=0; i<MAX_IDS; i++){
    if(!strcmp(ids[i].c_str(), id))
      return true;
  }
  return false;
}

ISR(SPI_STC_vect){
  switch(SPDR){    
    case 0x10:
      SPDR = locked ? 0x01 : 0x00;
//      Serial.print("Locked state is: ");
//      Serial.println(locked);
      break;
    case 0x20:
      locked = true;
      break;
    case 0x30:
      locked = false;
      break;
  }
}
