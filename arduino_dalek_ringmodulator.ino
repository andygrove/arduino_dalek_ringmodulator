/* Arduino Audio Loopback Test
 *
 * Arduino Realtime Audio Processing
 * 2 ADC 8-Bit Mode
 * analog input 1 is used to sample the audio signal
 * analog input 0 is used to control an audio effect
 * PWM DAC with Timer2 as analog output
 
 
 
 * KHM 2008 / Lab3/  Martin Nawrath nawrath@khm.de
 * Kunsthochschule fuer Medien Koeln
 * Academy of Media Arts Cologne

 */


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


int ledPin = 13;                 // LED connected to digital pin 13
int testPin = 7;


boolean div32;
boolean div16;
// interrupt variables accessed globally
volatile boolean f_sample;
volatile byte badc0;
volatile byte badc1;
volatile byte ibb;
int cnta;



int ii;


int icnt;
int cnt2;

float pi = 3.141592;
float dx ;
float fd ;
float fcnt;
int iw;
int iw1;
byte bb;

byte dd[512];  // Audio Memory Array 8-Bit

volatile unsigned long phaccu;   // pahse accumulator
volatile unsigned long tword_m;  // dds tuning word m
double dfreq;

// const double refclk=31372.549;  // =16MHz / 510
const double refclk=31376.6;      // measured


void setup()
{
  pinMode(ledPin, OUTPUT);      // sets the digital pin as output
  pinMode(testPin, OUTPUT);
  Serial.begin(57600);        // connect to the serial port
  Serial.println("Arduino Audio Ringmodulator");


  fill_sinewave();        // reload wave after 1 second

  //dfreq = 14.5;  
  dfreq = 30;  
  
  tword_m=pow(2,32)*dfreq/refclk;  // calulate DDS new tuning word


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
  sbi(DDRB,4);                    // set digital pin 10 to output

  //cli();                         // disable interrupts to avoid distortion
  cbi (TIMSK0,TOIE0);              // disable Timer0 !!! delay is off now
  sbi (TIMSK2,TOIE2);              // enable Timer2 Interrupt
 
  Serial.print("ADC offset=");     // trim to 127
  iw1=badc1;  
  Serial.println(iw1);

}



void loop()
{
  while (!f_sample) {     // wait for Sample Value from ADC
  }                       // Cycle 15625 KHz = 64uSec 

  //PORTD = PORTD  | 128;   // Test Output on pin 7
  f_sample=false;
  
  //phaccu=phaccu+tword_m; // soft DDS, phase accu with 32 bits
  //icnt=phaccu >> 24;     // use upper 8 bits for phase accu as frequency information
  //bb = pgm_read_byte_near(sine256 + icnt);    

  bb=dd[icnt] ;           // get the sinewave buffervalue on indexposition and substract dc
  iw = bb-128;
  iw1= 125- badc1;        // get audiosignal and substract dc

  iw  = iw * iw1/ 256;    // multiply sine and audio and resale to 255 max value
  bb = iw+128;            // add dc value again

  icnt++;                 // increment index
  icnt = icnt & 511;      // limit index 0..511

    cnt2++;               // let the led blink about every second
  if (cnt2 >= 15360){
    cnt2=0;
    //icnt=0;
    //PORTB = PORTB ^ 32;   // Toggle LED on Pin 11

  }

  OCR2A=bb;            // Sample Value to PWM Output

  //PORTD = PORTD  ^ 128;   // Test Output on pin 7


} // loop
//******************************************************************
void fill_sinewave(){
  dx=2 * pi / 512;                    // fill the 512 byte bufferarry
  for (iw = 0; iw <= 511; iw++){      // with  50 periods sinewawe
    fd= 127*sin(fcnt);                // fundamental tone
    fcnt=fcnt+dx;                     // in the range of 0 to 2xpi  and 1/512 increments
    bb=127+fd;                        // add dc offset to sinewawe 
    dd[iw]=bb;                        // write value into array

  }
}


//******************************************************************
// Timer2 Interrupt Service at 62.5 KHz
// here the audio and pot signal is sampled in a rate of:  16Mhz / 256 / 2 / 2 = 15625 Hz
// runtime : xxxx microseconds
ISR(TIMER2_OVF_vect) {

  //PORTB = PORTB  | 1 ;

  div32=!div32;                            // divide timer2 frequency / 2 to 31.25kHz
  if (div32){ 
    div16=!div16;  // 
    if (div16) {                       // sample channel 0 and 1 alternately so each channel is sampled with 15.6kHz
      badc0=ADCH;                    // get ADC channel 0
      sbi(ADMUX,MUX0);               // set multiplexer to channel 1
    }
    else
    {
      badc1=ADCH;                    // get ADC channel 1
      cbi(ADMUX,MUX0);               // set multiplexer to channel 0
      f_sample=true;
    }
    ibb++; 
    ibb--; 
    ibb++; 
    ibb--;    // short delay before start conversion
    sbi(ADCSRA,ADSC);              // start next conversion
  }

}
