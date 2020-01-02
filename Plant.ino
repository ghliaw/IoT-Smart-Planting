char sensorNO[] = "011";

#include "DHT.h"
#define dhtPin D2      //讀取DHT11 Data
#define dhtType DHT22 //選用DHT11   
DHT dht(dhtPin, dhtType); // Initialize DHT sensor

//LoRa 
#include <SoftwareSerial.h>
SoftwareSerial Serial2(13, 15); // RX2, TX2  ;  D7,D8
#include <string.h>

//Parse
char readcharbuffer[500];
int readbuffersize;
char temp_input;
int number = 1;
//Compare LoRa Response whether Data is we want
//mac rx 2 <data>, there is downlink data.
char rx_str[30] = ">> radio_rx 011";

//char rx_str[20] = ">> ";


//water flow
uint8_t water_flow_sensorPin = D1;//流量計IN腳位
float calibrationFactor = 1.3;//脈衝校準因數//12V脈衝因數為7
volatile byte pulseCount;   //脈衝數
float flowRate; //總流量
unsigned int flowMilliLitres; //水流MilliLitres
unsigned long totalMilliLitres; //總MilliLitres
unsigned long oldTime;
int watervalue,light;

//relay
const int relayInput = D4; //water
const int lightpin = D3; //燈IN腳位


void setup() {
  Serial.begin(115200);//設定鮑率115200
  dht.begin();//啟動DHT
  
  Serial2.begin(115200);//設定LoRa鮑率115200
  
  pinMode(relayInput,OUTPUT);
  pinMode(lightpin,OUTPUT);

  // set the data rate for the SoftwareSerial port
  pinMode(D6, OUTPUT);  //for rest EK-S76SXB
  digitalWrite(D6, LOW);
  delay(100);
  pinMode(D6, INPUT);  //for rest EK-S76SXB  
  delay(2000);

   
  Serial.println("rf set_sync 12");
  Serial2.print("rf set_sync 12");
  delay(1500);
  Serial.println("rf set_freq 926500000");
  Serial2.print("rf set_freq 926500000");
  delay(1500);
  Serial.println("rf set_sf 7");
  Serial2.print("rf set_sf 7");
  delay(1500);
  Serial.println("rf set_bw 125");
  Serial2.print("rf set_bw 125"); 
  delay(1500);

  readbuffersize = Serial2.available();
  while(readbuffersize){
    temp_input = Serial2.read();
    Serial.print(temp_input);
    readbuffersize--;
  }
    
  delay(3000);

  //water flow
  pinMode(relayInput, OUTPUT); //pin腳參數
  pinMode(water_flow_sensorPin, INPUT);//水流計pin腳參數
  digitalWrite(water_flow_sensorPin, HIGH);//讓水流計pin腳處高電位
  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;
  // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
 // attachInterrupt(digitalPinToInterrupt(water_flow_sensorPin), pulseCounter, FALLING);
  
}

void loop() {
  char lora_string[30];
  
  float h = dht.readHumidity();//讀取濕度
  float t = dht.readTemperature();//讀取攝氏溫度
  if (isnan(h) || isnan(t)) {
    Serial.println("無法從DHT傳感器讀取！");
    return;
  }
  Serial.print("攝氏溫度: ");
  Serial.print(t);
  Serial.print("*C\t");
  Serial.print("濕度: ");
  Serial.print(h);
  Serial.print("%\t");

  int soil_h = analogRead(A0); //讀取土壤濕度
  Serial.print("土壤濕度: ");
  Serial.println(soil_h);
  Serial.print("\t");
  int soil_s = 0;  //土壤狀態
  if(soil_h > 800){soil_s = soil_s+3;}
  else if(450 < soil_h < 801){soil_s =soil_s+2;}
  else if(soil_h < 451){soil_s =soil_s+1;}
  Serial.print("土壤");
  Serial.println(soil_s);


  sprintf(lora_string,"rf tx %sA%02d%02d%02d%02d", sensorNO ,(int)t, (int)h, (int)soil_s, (int)light);
  Serial2.write(lora_string);  //tx now
  Serial.println(lora_string);  //tx now
  delay(1500);

  readbuffersize = Serial2.available();
  while(readbuffersize){
    temp_input = Serial2.read();
    Serial.print(temp_input);
    readbuffersize--;
  }  readbuffersize = Serial2.available();
  while(readbuffersize){
    temp_input = Serial2.read();
    Serial.print(temp_input);
    readbuffersize--;
  }
  strcpy(lora_string, "");
  strcpy(readcharbuffer, ""); 
  Serial.println("==========");
  Serial2.write("rf rx_con on"); 
  Serial.println("rf rx_con on"); 
  delay(6000);
  Serial.println("Time Stop!"); 

  
  // received LoRa response and filter information
  unsigned long prev;
  int parse_ok = 0;
  // Initial call to setup the target string
  parseInput(rx_str,0);
  while(!parse_ok){
    int timeout = 0;
    prev = millis();
    while(Serial2.available() == 0){
      if(millis() - prev > 2500) {
        timeout = 1;
        break;
      }
    }
    if(timeout) break;
    temp_input = Serial2.read();
    Serial.print(temp_input);
    // call parseInput()
    if(parseInput(NULL , temp_input)) parse_ok = 1;

  }
  
  if(parse_ok){
    int i = 0;
    temp_input = 0;
    while(temp_input != '\n'){
      while(Serial2.available() == 0);
      temp_input = Serial2.read();
      Serial.print(temp_input);
      lora_string[i++] = temp_input;
    }
    lora_string[i-1] = '\0';


    Serial.println("====");
    Serial.println(lora_string);
    Serial.println("====");
    
    char del_w[2],c_w[3],c_l[3];
    sscanf(lora_string, "%1s%4s%2s", del_w , c_w, c_l);

    light =  atoi(c_l);
    watervalue= atoi(c_w);
    Serial.print("watervalue ");
    Serial.println(watervalue); 
    Serial.print("light  ");
    Serial.println(light); 


   }
  else Serial.println("Parse timeout!");
  delay(3500);
  


  Serial2.write("rf rx_con off"); 
  Serial.print("rf rx_con off"); 

  delay(100);

  if(light == 1){digitalWrite(lightpin,HIGH);}
  else{digitalWrite(lightpin,LOW);}

  Serial.println(totalMilliLitres);
  int LoRaWaterFlow = watervalue;    //修改這裡 ＞ 給水流值
  while (totalMilliLitres < LoRaWaterFlow) {
    digitalWrite(relayInput,HIGH);
    water_flow(LoRaWaterFlow);
    if (totalMilliLitres >= LoRaWaterFlow) {
    digitalWrite(relayInput,LOW);
    Serial.println("已完成澆水");
    sprintf(lora_string,"rf tx %sF%04d", sensorNO ,(int)totalMilliLitres);
    Serial2.write(lora_string);  //tx now  
    Serial.println(lora_string);  //tx now
    totalMilliLitres = 0;
    LoRaWaterFlow = 0;
    flowMilliLitres = 0;
    watervalue = 0;
    pulseCount = 0;
    flowRate = 0;
    oldTime= 0;
    }
}
   delay(1000);

}

#define MAX_TARGET_STRING_LENGTH  20
int parseInput(const char *target, const char input) {
  static int state, final;
  static char internal_str[MAX_TARGET_STRING_LENGTH+1];
  
  // Initial call: copy target string to internal str & 
  // set state = 0 and final = the length of target string;
  if (target!=NULL) {
    // If the length of target string is larger than the maximum allowed value,
    // return -1. 
    if (strlen(target) > MAX_TARGET_STRING_LENGTH)
      return(-1);
    //Copy target string to internal string
    strcpy(internal_str, target);
    // initialize state and final 
    state = 0;
    final = strlen(target);
    return 0;
  } 
  
  // Check if state has already been equal to final,
  // i.e. the last string match is finished and need to re-initialize,
  // then return -2
  if(state == final)
    return -2;
    
  // Compare input character with internal_str[state]:
  // if equal, state++
  // else, set state back to 0
  if(input == internal_str[state])
    state++;
  else
    state = 0;
  
  // If state is equal to final, 
  // i.e. the cumulative input characters match the internal string,
  // then reutrn 1;
  // else, return 0
  if(state==final)
    return 1;
  else
    return 0; 
}

ICACHE_RAM_ATTR void water_flow(int val){
    attachInterrupt(digitalPinToInterrupt(water_flow_sensorPin), pulseCounter, FALLING);

    if((millis() - oldTime) > 1000)    // Only process counters once per second
  { 
    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(digitalPinToInterrupt(water_flow_sensorPin));
        
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.

    //    Serial.print(" . ");
    //  Serial.print(pulseCount);
    //   Serial.print(" . ");

    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount ) / calibrationFactor;
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();
    
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 100;
    
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
      
    unsigned int frac;
    
  // Print the flow rate for this second in litres / minute
  //  Serial.print("Flow rate: ");
  //  Serial.print(int(flowRate));  // Print the integer part of the variable
  //  Serial.print("L/min");
  //  Serial.print("\t");       // Print tab space
  // Print the cumulative total of litres flowed since starting
  //   Serial.print("Output Liquid Quantity: ");        
  // Serial.print(totalMilliLitres);
  //  Serial.println("mL"); 
  // Serial.print("\t");       // Print tab space
  // Serial.print(totalMilliLitres/1000);
  // Serial.print("L");

    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
  }
 
  Serial.print ("所需澆水量 = ");
  Serial.print (val);
  Serial.print (", 目前澆水量 = ");
  Serial.println (totalMilliLitres);
  delay (1000);
 }



/*
Insterrupt Service Routine
 */
ICACHE_RAM_ATTR void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}
