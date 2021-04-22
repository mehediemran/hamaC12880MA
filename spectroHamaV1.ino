// Example code for the Hamamatsu C12666MA Micro Spectrometer (released ~December 2013)

#include <Arduino.h>
#include "SensorBuffer.h"
#include "SensorSpecHamamatsu.h"

#define SW_BASE       12
#define SW_MEASURE    11
int sw1Pressed = 0; // base
int sw2Pressed = 0; // measure
int swVal;

SensorSpecHamamatsu sensor;

void setup() {
  sensor.begin();
  pinMode(SW_BASE, INPUT_PULLUP);
  pinMode(SW_MEASURE, INPUT);
  sensor.ledOff();
  Serial.begin(115200);
  Serial.println("Initializing... "); 

 

  // Example 1: Calculate chlorophyll levels 
 
  Serial.println ("Press sw 1 to take baseline measurement...");
  
  while(digitalRead(12) == HIGH){}
  sensor.ledOn();
  sensor.takeBaseline();  
  sensor.ledOff();
  Serial.println ("baseline measurement done...");
 
 /* 
  sw2Pressed = 0;
  
  Serial.println ("Press sw 2 to take experimental measurement...");
  while(sw2Pressed!=1){
    if(digitalRead(SW_MEASURE) == LOW)
      {
        sw2Pressed = 1; // set flag for baseline
      } // sw pressed 
    else
    sw2Pressed = 0; // sw not pressed pressed
  }
  
  if(sw2Pressed == 1){ 
    sensor.takeMeasurement();
    sw2Pressed = 0;// reset the input flag
  }
 
  
  sensor.debugPrint();
  
  Serial.print ("Chlorophyll non-descructive index: ");  
  float ChlNDI = sensor.calculateReflectanceIndex(CHL_NDI_R1, CHL_NDI_R2);
  Serial.println (ChlNDI, 3);

  Serial.print ("Photochemical reflectance Index: ");    
  float PRI = sensor.calculateReflectanceIndex(PRI_R1, PRI_R2);
  Serial.println (PRI, 3);


  Serial.println ("Send any key for continuous measurement...");
  while(!Serial.available()); 
  while(Serial.available()) { Serial.read(); }  
  */

}

int count = 0;
void loop() { // loop

  Serial.println ("Press sw 2 to take experimental measurement...");
  while(digitalRead(SW_MEASURE) == HIGH){}
  Serial.print("measurement: "); Serial.println(count, DEC);
  sensor.ledOn();
  sensor.takeMeasurement();                 // Update every cycle
  sensor.ledOff();
  Serial.println ("experimental measurement done...");
  //sensor.takeMeasurementAveraging();      // Updates once every 4 cycles, but better signal quality
  sensor.debugPrint();
    /*
    Serial.print ("Chlorophyll non-descructive index: ");  
    float ChlNDI = sensor.calculateReflectanceIndex(CHL_NDI_R1, CHL_NDI_R2);
    Serial.println (ChlNDI, 3);
  */
    Serial.print ("Photochemical reflectance Index: ");    
    float PRI = sensor.calculateReflectanceIndex(PRI_R1, PRI_R2);
    Serial.println (PRI, 3);
  
   count += 1;
   delay(1000);
 
}
