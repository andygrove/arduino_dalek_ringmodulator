/* Arduino Ring Modulator for Dalek voice effect
 *
 * Originally written by Andy Grove in September 2012.
 * 
 * Takes mic/line input from analog pin 1 and mixes this signal with a
 * sine wave then plays the output to digital pin 10 or 11 depending on
 * the model of Arduino being used (10 on Mega 2560, 11 on most other boards).
 *
 * BE SURE TO UNCOMMENT THE CORRECT #defines FOR THE BOARD YOU ARE USING! THEY
 * ARE JUST BELOW THIS COMMENT BLOCK.
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
 
// use these values if you are using an Arduino Uno R3 (pin 11 for audio output)
#define DDRB_NUM 3

// use these values if you are using an Arduino Mega 2560 R3 (pin 10 for audio output)
//#define DDRB_NUM 4

// pin to use for dome lights (LEDs)
#define DOME_LIGHT_PIN 9

// this is useful for testing - set this to false to pass voice through without modification
boolean enableRingMod = true;

// changing NUM_SINE_WAVE_POINTS will change the frequency of the sine wave. 512 = 30 Hz, 256 = 60Hz
#define NUM_SINE_WAVE_POINTS 512

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

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

// Audio Memory Array 8-Bit containing sine wave
byte sineWave[NUM_SINE_WAVE_POINTS];  

// const double refclk=31372.549;  // =16MHz / 510
const double refclk=31376.6;      // measured

void setup()
{
  Serial.begin(9600);        // connect to the serial port
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

  // get the sample and convert from 3.3V to 5V by multiplying by 1.5
  nextSample = audioInput * 1.5;  

  // reset the flag so that the next call to loop will wait until the 
  // interrupt code has obtained a new sample
  f_sample=false;

  if (enableRingMod) {
    
    // get the next sinewave value and substract dc so it is in the range -127 .. +127
    // note that this code runs at 15625 Hz (see notes in timer function for explanation)
    // and since the sine wave array is 512 long, the sine wave is 15625/512 = 30.5 Hz
    iw = sineWave[sineWaveIndex++] - 127;
    
    // get audiosignal and substract dc so it is in the range -127 .. +127
    iw1 = 127 - nextSample;        
  
    // multiply sine and audio and rescale so still in range -127 .. +127
    iw  = iw * iw1 / 127;    
    
    // amplify (if necessary) then add 127 so back in range of 0 .. 255
    audioOutput = iw + 127;
  
    // limit index
    if (sineWaveIndex >= NUM_SINE_WAVE_POINTS) {
      sineWaveIndex = 0;
    }
    
  }
  else {
    // pass through input without any modification - good for initial testing!
    audioOutput = nextSample;
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

  // write new PWM signal to the LEDs once every 1000 times through this loop (so approximately 15 times per second)
  if (++counter == 1000) {
    counter = 0;
    analogWrite(9, light);
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
  
  // take a sample every 4th time through, therefore audio is sampled in a rate of:  16Mhz / 256 / 4 = 15.625 KHz
  if (++intervalCounter==4) {
    
    // reset the interval counter
    intervalCounter = 0;

    // get ADC channel 1 most significant 8 bits (0..255)
    audioInput = ADCH;

    // set flag so that loop() can continue and process the signal
    f_sample=true;
    
    // start next conversion
    sbi(ADCSRA,ADSC);              
  }

}
