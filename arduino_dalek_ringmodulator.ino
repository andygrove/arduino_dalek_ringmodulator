/* Arduino Ring Modulator for Dalek voice effect
 *
 * Written by Andy Grove
 * 
 * Takes mic/line input from analog pin 1 and mixes this signal with a
 * sine wave then plays the output to digital pin 10 or 11 depending on
 * the model of Arduino being used (10 on Mega 2560, 11 on most other boards).
 *
 * Adapted from example code written by Martin Nawrath nawrath@khm.de
 * http://interface.khm.de/index.php/lab/experiments/arduino-realtime-audio-processing/
 *
 * This source file along with circuit diagrams is available from: 
 * https://github.com/andygrove/arduino_dalek_ringmodulator
 *
 * Please post any questions or comments to the following forum on the Project Dalek 
 * web site (requires a free account).
 *
 * http://www.projectdalek.com/index.php?showtopic=9746
 */
 
boolean enableRingMod = true;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

// table of 256 sine values / one sine period / stored in flash memory
PROGMEM  prog_uchar sine256[]  = {
  127,130,133,136,139,143,146,149,152,155,158,161,164,167,170,173,176,178,181,184,187,190,192,195,198,200,203,205,208,210,212,215,217,219,221,223,225,227,229,231,233,234,236,238,239,240,
  242,243,244,245,247,248,249,249,250,251,252,252,253,253,253,254,254,254,254,254,254,254,253,253,253,252,252,251,250,249,249,248,247,245,244,243,242,240,239,238,236,234,233,231,229,227,225,223,
  221,219,217,215,212,210,208,205,203,200,198,195,192,190,187,184,181,178,176,173,170,167,164,161,158,155,152,149,146,143,139,136,133,130,127,124,121,118,115,111,108,105,102,99,96,93,90,87,84,81,78,
  76,73,70,67,64,62,59,56,54,51,49,46,44,42,39,37,35,33,31,29,27,25,23,21,20,18,16,15,14,12,11,10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,10,11,12,14,15,16,18,20,21,23,25,27,29,31,
  33,35,37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,78,81,84,87,90,93,96,99,102,105,108,111,115,118,121,124

};

// we want to take samples on every nth interval
int intervalCounter = 0;

// interrupt variables accessed globally
volatile boolean f_sample;
volatile byte audioInput;
volatile byte audioOutput;
volatile byte ibb;

int sineWaveIndex;

byte light;
int iw;
int iw1;
byte bb;

byte sineWave[512];  // Audio Memory Array 8-Bit

// const double refclk=31372.549;  // =16MHz / 510
const double refclk=31376.6;      // measured

void setup()
{
  Serial.begin(57600);        // connect to the serial port
  Serial.println("Arduino Dalek Voice Changer");

  fill_sinewave();        // reload wave after 1 second

  // set adc prescaler  to 64 for 19kHz sampling frequency
  cbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  sbi(ADMUX,ADLAR);  // 8-Bit ADC in ADCH Register
  sbi(ADMUX,REFS0);  // VCC Reference
  cbi(ADMUX,REFS1);
  cbi(ADMUX,MUX0);   // Set Input Multiplexer to Channel 0
  cbi(ADMUX,MUX1);
  cbi(ADMUX,MUX2);
  cbi(ADMUX,MUX3);
  
  sbi(ADMUX,MUX0); // set multiplexer to channel 1

  // Timer2 PWM Mode set to fast PWM 
  cbi (TCCR2A, COM2A0);
  sbi (TCCR2A, COM2A1);
  sbi (TCCR2A, WGM20);
  sbi (TCCR2A, WGM21);

  cbi (TCCR2B, WGM22);

  // Timer2 Clock Prescaler to : 1 
  sbi (TCCR2B, CS20);
  cbi (TCCR2B, CS21);
  cbi (TCCR2B, CS22);

  // Timer2 PWM Port Enable
  //sbi(DDRB,3);                    // set digital pin 11 to output on Uno
  sbi(DDRB,4);                    // set digital pin 10 to output on Mega 2560

  //cli();                         // disable interrupts to avoid distortion
  cbi (TIMSK0,TOIE0);              // disable Timer0 !!! delay is off now
  sbi (TIMSK2,TOIE2);              // enable Timer2 Interrupt

  // pin 9 is for the dome lights  
  pinMode(9, OUTPUT);
}


void loop()
{
  while (!f_sample) {     // wait for Sample Value from ADC
  }                       // Cycle 15625 KHz = 64uSec 

  f_sample=false;

  if (enableRingMod) {
    
    // get the next sinewave value and substract dc so it is in the range -127 .. +127
    // note that this code runs at 15625 Hz (see notes in timer function for explanation)
    // and since the sine wave array is 512 long, the sine wave is 15625/512 = 30.5 Hz
    iw = sineWave[sineWaveIndex++] - 127;
    
    // get audiosignal and substract dc so it is in the range -127 .. +127
    iw1 = 127 - audioInput;        
  
    // multiply sine and audio and resale to 255 max value
    iw  = iw * iw1 / 256;    
    
    // amplify then add dc value again
    audioOutput = iw * 1 + 127;            
  
    // limit index 0..511
    sineWaveIndex = sineWaveIndex & 511;      

  }
  else {
    // pass through input without any modification - good for initial testing!
    audioOutput = audioInput;
  }

  // write to pin associated with timer 2 (10 or 11 depending on board)
  OCR2A=audioOutput;          

  // trigger dome lights if output signal above some threshold (hard-coded 
  // for now, but I'd like to make this variable based on a pot input so 
  // it can be adjusted easily)
  if (audioOutput > 180 && audioOutput > light) {
    light = audioOutput;
  }

  // fade away to simulate incandescent light bulb
  if (sineWaveIndex%10==0 && light>0) {
    light--;
  }
 // light = 255;

  analogWrite(9, light);

} // loop

//******************************************************************
void fill_sinewave(){
  float pi = 3.141592;
  float dx ;
  float fd ;
  float fcnt;
  dx=2 * pi / 512;                    // fill the 512 byte bufferarry
  for (iw = 0; iw <= 511; iw++){      // with  50 periods sinewawe
    fd= 127*sin(fcnt);                // fundamental tone
    fcnt=fcnt+dx;                     // in the range of 0 to 2xpi  and 1/512 increments
    bb=127+fd;                        // add dc offset to sinewawe 
    sineWave[iw]=bb;                        // write value into array

  }
}


//******************************************************************
// Timer2 Interrupt Service at 62.5 KHz
// here the audio and pot signal is sampled in a rate of:  16Mhz / 256 / 4 = 15625 Hz
// runtime : xxxx microseconds
ISR(TIMER2_OVF_vect) {
  
  if (++intervalCounter==4) {
    intervalCounter = 0;
    audioInput=ADCH;                    // get ADC channel 1
    f_sample=true;
    ibb++; 
    ibb--; 
    ibb++; 
    ibb--;    // short delay before start conversion
    sbi(ADCSRA,ADSC);              // start next conversion
  }

}
