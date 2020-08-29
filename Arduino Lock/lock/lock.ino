#include <RH_ASK.h> // fixed compilation error with servo b/c of timer1 conflict: https://forum.arduino.cc/index.php?topic=553617.0
#include <MillisTimer.h>
#include <EEPROM.h>
#include <SPI.h> 
#include <RFID.h>
#include <Servo.h>

RH_ASK driver(2000, 3, 2, 4);
const char id[] = "id:001";

const unsigned long MAX_IDS = 10;
String ids[MAX_IDS];
unsigned long nextIdIndex = 0;
unsigned long nextIdAddr = 0;

const char idIdentifier[] = "id:";
const char commandIdentifier[] = "command:";
const char valueIdentifier[] = "value:";

MillisTimer timer1;

RFID rfid(10, 9);       //D10:pin of tag reader SDA. D9:pin of tag reader RST 
unsigned char status; 
unsigned char str[MAX_LEN]; //MAX_LEN is 16: size of the array 

const String accessGranted [2] = {"159101445113", "29131002136"};  //RFID serial numbers to grant access to
const int accessGrantedSize = 2;                                //The number of serial numbers

Servo lockServo;                //Servo for locking mechanism
const int lockPos = 15;               //Locked position limit
const int unlockPos = 75;             //Unlocked position limit
boolean locked = true;

const int pairBtn = 5;
const int servoPin = 6;
const int relay = A0;
const int red = 7;
const int green = 8;

void setup() 
{ 
  Serial.begin(9600);     //Serial monitor is only required to get tag ID numbers and for troubleshooting

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
  
  timer1.setInterval(5000);       // set timer to trigger every 1000 millis
  timer1.expiredHandler(timerCb);   // call flash() function when timer runs out
  timer1.setRepeats(0);           // repeat forever if set to 0
  timer1.start();                 // start the timer when the sketch starts

  SPI.begin();            //Start SPI communication with reader
  rfid.init();            //initialization

  pinMode(pairBtn, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);

  lockServo.attach(servoPin);

  lockServo.write(lockPos);
  digitalWrite(relay, HIGH);
  digitalWrite(red, HIGH);
  digitalWrite(green, LOW);  

  Serial.println("Reading to go...");
}

void loop() 
{
  timer1.run();                   // trigger the timer only if it has run out
  checkRFReceive();
  checkRFID();
  checkBtn();
}

void timerCb(){
//  Serial.println("TIMER");
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
            updateState(true);
          }
          else if(!strcmp(value, "0")){
            updateState(false);
          }          
        }
      }
    }
  }
}

void checkRFID(){
  if (rfid.findCard(PICC_REQIDL, str) == MI_OK)   //Wait for a tag to be placed near the reader
  { 
    String temp = "";                             //Temporary variable to store the read RFID number
    if (rfid.anticoll(str) == MI_OK)              //Anti-collision detection, read tag serial number 
    { 
      Serial.print("Card ID: "); 
      for (int i = 0; i < 4; i++)                 //Record and display the tag serial number 
      { 
        temp = temp + (0x0F & (str[i] >> 4)); 
        temp = temp + (0x0F & str[i]); 
      } 
      Serial.println (temp);
      checkAccess (temp);     //Check if the identified tag is an allowed to open tag
    } 
    rfid.selectTag(str); //Lock card to prevent a redundant read, removing the line will make the sketch read cards continually
  }
  rfid.halt();
}

void checkAccess (String temp)    //Function to check if an identified tag is registered to allow access
{
  boolean granted = false;
  for (int i=0; i <= (accessGrantedSize-1); i++)    //Runs through all tag ID numbers registered in the array
  {
    if(accessGranted[i] == temp)            //If a tag is found then open/close the lock
    {
      Serial.println ("Access Granted");
      granted = true;

      updateState(!locked);
    }
  }
  if (granted == false)     //If the tag is not found
  {
    Serial.println ("Access Denied");
    
//    for(int i=0; i<2; i++){
//      digitalWrite(red, HIGH);      //Red LED sequence
//      delay(200);
//      digitalWrite(red, LOW);
//      delay(200);
//    }
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
    lockServo.write(lockPos);
    digitalWrite(relay, HIGH);
    digitalWrite(red, HIGH);
    digitalWrite(green, LOW);
  } else {
    Serial.println("Unlocked");
    lockServo.write(unlockPos);
    digitalWrite(relay, LOW);
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
