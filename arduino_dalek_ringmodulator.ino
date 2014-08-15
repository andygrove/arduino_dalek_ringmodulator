/* Arduino Ring Modulator a.k.a Dalek Voice Changer
 *
 * Originally written by Andy Grove in September 2012.
 *
 * See the README.md for list of required electronics components and link to 
 * schematics diagram.
 * 
 * Takes mic/line input from analog pin 1 (A1) and mixes this signal with a
 * sine wave then plays the output to digital pin 10 or 11 depending on
 * the model of Arduino being used (10 on Mega 2560, 11 on most other boards).
 *
 * There is now an optional to connect a potentiometer to pin A0 to control the 
 * sine wave frequency.
 *
 * BE SURE TO UNCOMMENT THE CORRECT #defines FOR THE BOARD AND VOLTAGE YOU 
 * ARE USING! THEY ARE JUST BELOW THIS COMMENT BLOCK!
 *
 * Adapted from example code written by Martin Nawrath nawrath@khm.de
 * http://interface.khm.de/index.php/lab/experiments/arduino-realtime-audio-processing/
 *
 * This source file along with circuit diagrams is available from: 
 * https://github.com/andygrove/arduino_dalek_ringmodulator
 *
 * Please post any questions or comments to the following forum on the Project Dalek 
 * web site (requires a free account).
 * http://www.projectdalek.com/index.php?showtopic=9746
 *
 * To connect with me on Google+:
 * https://www.google.com/+AndyGrove73
 */

 #include <arduino.h>
 
// IMPORTANT! MAKE SURE YOU CHOOSE THE CORRECT VALUES HERE, ESPECIALLY AREF! USING THE WRONG 
// VALUES COULD POTENTIONALLY DAMAGE YOUR ARDUINO!!

// if you have the mic connected to 3.3V (recommended since the 5V line is noisy) then you should 
// also connect AREF to 3.3V and uncomment the following line. If you have any Arduino pins connected 
// to 5V then make sure the following line is commented out.
#define AREF_EXTERNAL

// use these values if you are using an Arduino Uno R3 (pin 11 for audio output)
#define DDRB_NUM 3

// use these values if you are using an Arduino Mega 2560 R3 (pin 10 for audio output)
//#define DDRB_NUM 4

// pin to use for dome lights (LEDs)
#define DOME_LIGHT_PIN 9

// the code now supports some different effects modes, mostly to help with debugging
#define MODE_NO_EFFECT 1 // this mode passes the mic input directly to the audio out without modification
#define MODE_RING_MOD  2 // this is the default ring modulator mode where the mic input is mixed with a sine wave then written to audio out
#define MODE_SINE_WAVE 3 // this mode writes the sine wave directly to audio out
#define MODE_EXPERIMENTAL 4 // trying out new stuff here

// select which mode you want here
const int mode = MODE_RING_MOD;

// const double refclk=31372.549;  // =16MHz / 510
const double refclk=31376.6;      // measured

// constants for common frequencies ... calculation is: n = 31376/2/frequency
#define FREQUENCY_15_HZ 1045 // 
#define FREQUENCY_25_HZ  612 // actually 25.64 (NSD)
#define FREQUENCY_30_HZ  523 // Genesis

// for variable frequency controlled by a pot, comment out this next line
#define FIXED_FREQ  FREQUENCY_30_HZ

#ifdef FIXED_FREQ
#define NUM_SINE_WAVE_POINTS FIXED_FREQ
#else
#define NUM_SINE_WAVE_POINTS 1568 // refclk/2/10
#define MIN_INCREMENT 1 // 10 Hz
#define MAX_INCREMENT 5 // 50 Hz
#endif

// Audio Memory Array 8-Bit containing sine wave
byte sineWave[NUM_SINE_WAVE_POINTS];  

// the dome light will come on when the audio signal goes above this number..a quiet signal 
// is around 127 so this number needs to be > 127 and < 255
const int domeLightThreshold = 180;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

// we want to take samples on every nth interval
int intervalCounter = 0;

// interrupt variables accessed globally
volatile boolean f_sample;
volatile byte audioInput;
volatile byte pot;
volatile byte ibb;

byte audioOutput;

// how fast to step through the sine wave values... controlled by pot
int sineWaveIncrement = 1;

int sineWaveIndex = 0;

byte light;
int iw;
int iw1;
byte bb;

void setup()
{
  Serial.begin(9600);        // connect to the serial port
  Serial.println("Arduino Dalek Voice Changer");

  // generate sine wave values
  fill_sinewave();

  // set adc prescaler  to 64 for 19kHz sampling frequency
  cbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);


  sbi(ADMUX,ADLAR);  // 8-Bit ADC in ADCH Register

#ifdef AREF_EXTERNAL
  // AREF set to external reference voltage (whatever is connected to the AREF pin)
  cbi(ADMUX,REFS0);
  cbi(ADMUX,REFS1);
#else
  // AREF set to VCC (5v on 5V Arduino boards, 3.3V on 3.3V boards)
  sbi(ADMUX,REFS0);
  cbi(ADMUX,REFS1);
#endif

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
  sbi(DDRB,DDRB_NUM);              

  //cli();                         // disable interrupts to avoid distortion
  cbi (TIMSK0,TOIE0);              // disable Timer0 !!! delay is off now
  sbi (TIMSK2,TOIE2);              // enable Timer2 Interrupt

  // dome lights  
  pinMode(DOME_LIGHT_PIN, OUTPUT);
}

int counter = 0;
int nextSample = 0;

void loop()
{
  // wait for Sample Value from ADC
  // Cycle 15625 KHz = 64uSec 
  while (!f_sample) {     
  }                

  // reset the flag so that the next call to loop will wait until the 
  // interrupt code has obtained a new sample
  f_sample=false;

  // get next sample
  nextSample = audioInput;

  if (mode == MODE_NO_EFFECT) {
    // pass through input without any modification - good for initial testing!
    audioOutput = nextSample;
    
  } else if (mode == MODE_SINE_WAVE) {
    // get the next point from the sine wave data
    sineWaveIndex += sineWaveIncrement;
    if (sineWaveIndex >= NUM_SINE_WAVE_POINTS) {
      sineWaveIndex -= NUM_SINE_WAVE_POINTS;
    }
    // pass sine wave directly to audio out
    audioOutput = sineWave[sineWaveIndex];
  } else if (mode == MODE_RING_MOD) {
    
    // get the next point from the sine wave data
    sineWaveIndex += sineWaveIncrement;
    if (sineWaveIndex >= NUM_SINE_WAVE_POINTS) {
      sineWaveIndex -= NUM_SINE_WAVE_POINTS;
    }

    // get the next sinewave value and substract dc so it is in the range -127 .. +127
    iw = sineWave[sineWaveIndex] - 127;
    
    // get audiosignal and substract dc so it is in the range -127 .. +127
    iw1 = nextSample - 127;        
  
    // multiply sine and audio and rescale so still in range -127 .. +127
    iw  = iw * iw1 / 127;    
    
    // amplify (if necessary) then add 127 so back in range of 0 .. 255
    audioOutput = iw + 127;
  }
  
  // write to pin associated with timer 2 (10 or 11 depending on board)
  OCR2A=audioOutput;          

  // trigger dome lights if output signal above some threshold (hard-coded 
  // for now, but I'd like to make this variable based on a pot input so 
  // it can be adjusted easily)
  if (audioOutput > domeLightThreshold && audioOutput > light) {
    light = audioOutput;
  }

  // fade away to simulate incandescent light bulb
  if (counter%10==0 && light>0) {
    light--;
  }

  // write new PWM signal to the LEDs once every 1000 times through this loop (so approximately 15 times per second)
  if (++counter == 1000) {
    counter = 0;
    analogWrite(9, light);
    
#ifndef FIXED_FREQ    
    // also update sine wave increment based on pot value
    // 1 through 10 translates to 10 Hz through 100 Hz in 10 Hz increments
    sineWaveIncrement = map(pot, 0, 255, MIN_INCREMENT, MAX_INCREMENT);
#endif
  }

} // loop

/**
 * Generates one complete sine wave into the 512 byte array.
 */
void fill_sinewave(){
  float pi = 3.141592;
  float dx ;
  float fd ;
  float fcnt;
  dx=2 * pi / NUM_SINE_WAVE_POINTS;   // fill the  byte bufferarry
  for (iw = 0; iw < NUM_SINE_WAVE_POINTS; iw++){      // with 50 periods sinewawe
    fd= 127*sin(fcnt);                // fundamental tone
    fcnt=fcnt+dx;                     // in the range of 0 to 2xpi  and 1/512 increments
    bb=127+fd;                        // add dc offset to sinewawe 
    sineWave[iw]=bb;                  // write value into array
    // uncomment this to see the sine wave numbers in the serial monitor
    /*
    Serial.print("Sine: ");
    Serial.println(bb);
    */
  }
}

/**
 * Timer2 Interrupt Service at 62.5 KHz 
 * runtime : xxxx microseconds
 */
ISR(TIMER2_OVF_vect) {
  
  intervalCounter++;

  if (intervalCounter>1) {
    if (intervalCounter==2) {
      // get pot reading to control frequency
      pot = ADCH;
      // set multiplexer to channel 1
      sbi(ADMUX,MUX0);               

    }
    else if (intervalCounter==3) {
      // take a sample every 4th time through, therefore audio is sampled in a rate of:  16Mhz / 256 / 4 = 15.625 KHz

      // reset the interval counter
      intervalCounter = 0;
      
      // get ADC channel 1 most significant 8 bits (0..255)
      audioInput = ADCH;

      // set multiplexer to channel 0
      cbi(ADMUX,MUX0);               
  
      // set flag so that loop() can continue and process the signal
      f_sample=true;
   
    }
    // short delay before the next conversion
    ibb++;
    ibb--;
    ibb++;
    ibb--;
    
    // start next conversion
    sbi(ADCSRA,ADSC);      
  }    
}
