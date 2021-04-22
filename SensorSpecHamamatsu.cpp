// SensorSpecHamamatsu.cpp
// An Arduino/Chipkit driver for the new Hamamatsu C12666MA Micro Spectrometer (released ~December 2013)
// Peter Jansen, September/2014. 
// -------------------------------------------------------------------------------------------------------------
// Nominally, the entire measurement process can be completed in a few lines of code:
//  SensorSpecHamamatsu sensor;
//  sensor.takeMeasurement();
//  sensor.debugPrint();
// There are also methods to control the integration (sample) time, the gain, and to convert the measurement
// into a SensorBuffer storage class.
// -------------------------------------------------------------------------------------------------------------
/*
// modified by mehedi mehediemran@gmail.com for Arduino and C12880MA 
 */
#include <Arduino.h>
#include "SensorSpecHamamatsu.h"

// Constructor
SensorSpecHamamatsu::SensorSpecHamamatsu() {
  // Initialize arrays
  for (int i=0; i<SPEC_CHANNELS; i++) {
    baseline[i] = 0;
    data[i] = 0; 
    average[i] = 0;
  }  
  
  peakChannel = 0;
  baselineOverexposed = false;
  dataOverexposed = 0;
  hasBaseline = false;
  numAverages = 0;
  averageQueue = QUEUE_EMPTY;
}

// Destructor
SensorSpecHamamatsu::~SensorSpecHamamatsu() {
}
  
// Setup methods
/*
// Pin mappings
#define SPEC_TRIG        D6
#define SPEC_EOS         D5
#define SPEC_ST          D7
#define SPEC_CLK         D8
#define SPEC_VIDEO       A0
#define SPEC_LED         D9      
#define SPEC_LASER       D10
*/
boolean SensorSpecHamamatsu::begin(){
  // Setup pins
  pinMode(SPEC_EOS, INPUT);
  
  pinMode(SPEC_TRIG, OUTPUT);
  pinMode(SPEC_ST, OUTPUT);
  pinMode(SPEC_CLK, OUTPUT);
  
  pinMode(SPEC_LED, OUTPUT);
  pinMode(SPEC_LASER, OUTPUT);
  
  // Default settings
  digitalWrite(SPEC_TRIG, HIGH);
  digitalWrite(SPEC_ST, HIGH);  
  digitalWrite(SPEC_CLK, HIGH);  
  digitalWrite(SPEC_LED, LOW);  
  digitalWrite(SPEC_LASER, LOW);  
  
  setGain(1);
  setIntTime(100);
}

void SensorSpecHamamatsu::setIntTime(uint16_t time) {
  // Set the integration (or sample collection) time, in milliseconds
  // 60ms might be about the shortest integration time with the current ADC timings
  intTime = time;
}

void SensorSpecHamamatsu::setGain(uint8_t highLow) {
  // Gain is either 0 (low gain) or 1 (high gain)
  gain = highLow;
  if (gain == 0) {
    digitalWrite(SPEC_TRIG, 0);
  } else {
    digitalWrite(SPEC_TRIG, 1);
  }
}
// led on and off SPEC_LED
void SensorSpecHamamatsu::ledOn(){ digitalWrite(SPEC_LED, 1);}  
void SensorSpecHamamatsu::ledOff(){digitalWrite(SPEC_LED, 0);}  
// led on and off SPEC_LASER
void SensorSpecHamamatsu::laserOn(){digitalWrite(SPEC_LASER, 1);}  
void SensorSpecHamamatsu::laserOff(){digitalWrite(SPEC_LASER, 0);} 
  
// Data methods
void SensorSpecHamamatsu::takeMeasurement() {
  Serial.println ("Starting measurement...");
  readSpectrometer(&data[0], false);
  Serial.println ("Measurement complete...");
  dataOverexposed = postProcessing(&data[0]);
  
  // Find peak channel value
  uint16_t maxValue = 0;
  for (int i=0; i<SPEC_CHANNELS; i++) {
    if (data[i] > maxValue) {
      peakChannel = i;
      maxValue = data[i];
    }
  }
}

void SensorSpecHamamatsu::takeBaseline() {
  readSpectrometer(&baseline[0], false);  
  baselineOverexposed = postProcessing(&baseline[0]);
  hasBaseline = true;
}


// These averaging functions perform similar to above, but are meant for a constant stream of data -- when takeMeasurementAveraging()
// is called frequently, and takeMeasurementBaseline() is called infrequently.  The data isn't guaranteed to be in the appropriate
// buffer until at most 4 subsequent function calls, but it's averaged and more stable. 
void SensorSpecHamamatsu::takeBaselineAveraging() {
  averageQueue = QUEUE_BASELINE;
  takeMeasurementAveraging(); 
}

void SensorSpecHamamatsu::takeMeasurementAveraging() {
  
  boolean isAvgReady = takeAverageMeasurement();
  if (isAvgReady) {
    if (averageQueue == QUEUE_BASELINE) {
      // Store measurement in baseline (higher priority but infrequent condition)
      for (int i=0; i<SPEC_CHANNELS; i++) {
        baseline[i] = average[i];
      }
      baselineOverexposed = postProcessing(&baseline[0]);
      hasBaseline = true;    
      averageQueue = QUEUE_EMPTY;  
    } else {
      // Store measurement in experimental data (normal condition)
      for (int i=0; i<SPEC_CHANNELS; i++) {
        data[i] = average[i];
      }      
      dataOverexposed = postProcessing(&data[0]);
  
      // Find peak channel value
      uint16_t maxValue = 0;
      for (int i=0; i<SPEC_CHANNELS; i++) {
        if (data[i] > maxValue) {
          peakChannel = i;
          maxValue = data[i];
        }
      }      
      averageQueue = QUEUE_EMPTY;
    }
  }
    
}

// Limited-blocking averaging function
boolean SensorSpecHamamatsu::takeAverageMeasurement() {
  // Check to see if this is the first measurement
  if (numAverages == 0) {
    for (int i=0; i<SPEC_CHANNELS; i++) {
      average[i] = 0; 
    }    
  }
  
  // Collect a measurement, and add it to the average
  readSpectrometer(&average[0], true);
  numAverages += 1;
  
  // If the average has accumulated 4 measurements, average them, and return true to signify the average measurement is ready
  if (numAverages == 4) {
    for (int i=0; i<SPEC_CHANNELS; i++) {
      average[i] = average[i] >> 2; 
    }
    numAverages = 0;
    return true;
  }
  return false;
}

boolean SensorSpecHamamatsu::postProcessing(uint16_t* data) {  
  boolean flagOverexposed = false;
  
  for (int i=0; i<SPEC_CHANNELS; i++) {
    // Check for overexposure
    if (data[i] > SPEC_MAX_VALUE) flagOverexposed = true;
    // Subtract the offset
    if (data[i] > SPEC_MIN_VALUE) {
      data[i] = data[i] - SPEC_MIN_VALUE;
    } else {
      data[i] = 0;
    }
  }
  
  return flagOverexposed;
}

void SensorSpecHamamatsu::populateSensorBuffer(SensorBuffer* sb, uint8_t mode) {
  // This function populates a SensorBuffer with spectrometer data (resampled to buffer size). 
  // MODE is either SPEC_DATA, SPEC_BASELINE, or SPEC_DELTA (data - baseline)
  // Step 1: Clear data
  sb->setAll(0.0f);
  int maxVal = SPEC_MAX_VALUE - SPEC_MIN_VALUE;      // Maximum possible value to attain
  int sbSize = sb->getSize();
  float sbPerChannel = 1.0f;
  if (sbSize < SPEC_CHANNELS) {
    sbPerChannel = (float)SPEC_CHANNELS / (float)sbSize;
  }
  
  float resampleCounter = sbPerChannel;
  uint16_t maxSample = 0;
  
  if (mode == SPEC_DATA) {
    // Measurement data
    for (int i=0; i<SPEC_CHANNELS; i++) {
      maxSample = max(maxSample, data[i]);
      if (i > floor(resampleCounter)) {
        sb->put( (float)maxSample ); // / maxVal );
        resampleCounter += sbPerChannel;
        maxSample = 0;
      }
    }
  } else if (mode == SPEC_BASELINE) {
    // Baseline data
    for (int i=0; i<SPEC_CHANNELS; i++) {
      maxSample = max(maxSample, baseline[i]);
      if (i > floor(resampleCounter)) {
        sb->put( (float)maxSample ); // / maxVal );
        resampleCounter += sbPerChannel;
        maxSample = 0;
      }
    }
  } else {
    // Measurement - baseline
    for (int i=0; i<SPEC_CHANNELS; i++) {
      maxSample = max(maxSample, data[i] - baseline[i]);
      if (i > floor(resampleCounter)) {
        sb->put( (float)maxSample ); // / maxVal );
        resampleCounter += sbPerChannel;
        maxSample = 0;
      }
    }    
  }
    
}
    
int SensorSpecHamamatsu::spectralChannelToWavelength(int32_t channelNum) {
  // A quick way to calculate which wavelength each spectral channel represents.
  // This is only a first approximation -- the actual equation requires some overhead, but this is an okay first pass for now.
  uint32_t wavelength = (uint32_t)SPEC_MINWAVELENGTH + (channelNum * (uint32_t)SPEC_WAVELENGTH_DELTA);
  return wavelength / 100;    
}
    
int SensorSpecHamamatsu::wavelengthToSpectralChannel(int32_t wavelength) {
  // Wavelength should be an integer, in nanometers -- decimals not supported
  uint32_t wavelength_ = ((uint32_t)wavelength * 100) - (uint32_t)SPEC_MINWAVELENGTH;  
  uint32_t channelNum = (wavelength_ / (uint32_t)SPEC_WAVELENGTH_DELTA) + 1;
  
  // Bound checking
  if (channelNum < 0) channelNum = 0;
  if (channelNum >= SPEC_CHANNELS) channelNum = SPEC_CHANNELS-1; 
  return channelNum;
}  

// Returned as a normalized reflectance proportion (0-1, where 0 is 0% reflectance, and 1 is 100% reflectance)
// Both baseline[] and data[] must be populated with valid data for this to be valid
float SensorSpecHamamatsu::getReflectanceAtWavelength(int32_t wavelength) {  
//  return getBaselineExpProportion(wavelength);
    float val = getBaselineExpProportion(wavelength);
//##    Serial.print(val, 2); Serial.print("ref   ");
//##    Serial.println("");
    return val;
}

// Returned as a normalized absorbance proportion (0-1, where 0 is 0% reflectance, and 1 is 100% reflectance)
// Both baseline[] and data[] must be populated with valid data for this to be valid
float SensorSpecHamamatsu::getAbsorbanceAtWavelength(int32_t wavelength) {
  return 1 - getBaselineExpProportion(wavelength);
}

// Helper function for reflectance and absorbance calculations
float SensorSpecHamamatsu::getBaselineExpProportion(int32_t wavelength) {
  int channelNum = wavelengthToSpectralChannel(wavelength);
  float baselineVal = (float)baseline[channelNum];
  float expVal = (float)data[channelNum];
  
//##
/*
  int i = channelNum;
    Serial.print(i, DEC); Serial.print(": "); 
    Serial.print(wavelength, DEC); Serial.print("nm   ");    
    Serial.print(baseline[i], DEC); Serial.print("b   ");    
    Serial.print(data[i], DEC); Serial.print("e   ");
*/

  if (baselineVal == 0) return 0;          // prevent divide by zero 
  return (expVal / baselineVal);
}


// Analysis methods
float SensorSpecHamamatsu::calculateReflectanceIndex(int32_t wavelength_1, int32_t wavelength_2) {
  // Calculates reflectance difference indices of the form R_1 - R_2 / (R_1 + R_2) 
  // E.g. for chlorophyll, R1 is 750nm, and R2 is 705nm.
  float r1 = getReflectanceAtWavelength(wavelength_1);
  float r2 = getReflectanceAtWavelength(wavelength_2);       
  
  return (r1 - r2) / (r1 + r2);
}


// readADC0 for c12880ma sensor 
uint16_t SensorSpecHamamatsu::readADC() {
  uint16_t value = 0;
  value = analogRead(SPEC_VIDEO); 
  return value;    
}


void SensorSpecHamamatsu::readSpectrometer(uint16_t* data, boolean accumulateMode) {
  int delay_time = 35;     // delay per half clock (in microseconds).  This ultimately conrols the integration time. 
  int idx = 0;
  int read_time = 35;      // Amount of time that the readAD7940() procedure takes (in microseconds)
  
  // Step 1: start leading clock pulses
  for (int i=0; i<SPEC_CHANNELS; i++) {
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);  
  }
  
  // Step 2: Send start pulse to signal start of integration/light collection
  digitalWrite(SPEC_CLK, LOW);
  delayMicroseconds(delay_time);
  digitalWrite(SPEC_CLK, HIGH);
  digitalWrite(SPEC_ST, LOW);
  delayMicroseconds(delay_time);  
  digitalWrite(SPEC_CLK, LOW);
  delayMicroseconds(delay_time);
  digitalWrite(SPEC_CLK, HIGH);
  digitalWrite(SPEC_ST, HIGH);
  delayMicroseconds(delay_time);  
  
  // Step 3: Integration time -- sample for a period of time determined by the intTime parameter
  int blockTime = delay_time * 8;
  int numIntegrationBlocks = (intTime * 1000) / blockTime;
  for (int i=0; i<numIntegrationBlocks; i++) {
    // Four clocks per pixel  
    // First block of 2 clocks -- measurement
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);      
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);    
    
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);      
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);    
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);      
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);        
  }


  // Step 4: Send start pulse to signal end of integration/light collection
  digitalWrite(SPEC_CLK, LOW);
  delayMicroseconds(delay_time);
  digitalWrite(SPEC_CLK, HIGH);
  digitalWrite(SPEC_ST, LOW);
  delayMicroseconds(delay_time);  
  digitalWrite(SPEC_CLK, LOW);
  delayMicroseconds(delay_time);
  digitalWrite(SPEC_CLK, HIGH);
  digitalWrite(SPEC_ST, HIGH);
  delayMicroseconds(delay_time);  

  // Step 5: Read Data 2 (this is the actual read, since the spectrometer has now sampled data)
  idx = 0;
  for (int i=0; i<SPEC_CHANNELS; i++) {
    // Four clocks per pixel  
    // First block of 2 clocks -- measurement
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);      
    digitalWrite(SPEC_CLK, LOW);
    
    // Analog value is valid on low transition
    if (accumulateMode == false) {
      data[idx] = readADC();
    } else {
      data[idx] += readADC();
    }
    idx += 1;
    if (delay_time > read_time) delayMicroseconds(delay_time-read_time);     // Read takes about 135uSec    
    
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);

    // Second block of 2 clocks -- idle
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);      
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);      
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);    
  }

  // Step 6: trailing clock pulses
  for (int i=0; i<SPEC_CHANNELS; i++) {
    digitalWrite(SPEC_CLK, LOW);
    delayMicroseconds(delay_time);
    digitalWrite(SPEC_CLK, HIGH);
    delayMicroseconds(delay_time);  
  }
  
}
    
// Debug 
void SensorSpecHamamatsu::debugPrint() {
  Serial.println ("Spectral Channel, Baseline, Data");  
  for (int i=0; i<SPEC_CHANNELS; i++) {
    int wavelength = spectralChannelToWavelength(i);
    Serial.print(i, DEC); Serial.print(": "); 
    Serial.print(wavelength, DEC); Serial.print("nm   ");    
    Serial.print(baseline[i], DEC); Serial.print("b   ");    
    Serial.print(data[i], DEC); Serial.print("e   ");
    Serial.print(getReflectanceAtWavelength(wavelength), 2); Serial.print("ref   ");
    Serial.print(getAbsorbanceAtWavelength(wavelength), 2); Serial.print("abs   ");
        
    Serial.println("");    
  }
}
void SensorSpecHamamatsu::rawPrint() {
  for (int i=0; i<SPEC_CHANNELS; i++) {
    Serial.print(data[i], DEC);
  }
  }  
  
