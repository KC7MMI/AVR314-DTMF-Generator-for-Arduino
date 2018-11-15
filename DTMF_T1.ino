//***************************************************************************
//* A P P L I C A T I O N   N O T E   F O R   T H E   A V R   F A M I L Y
//*
//* Number               : AVR314
//* File Name            : "dtmf.c"
//* Title                : DTMF Generator
//* Original Date        : 00.06.27
//* Target MCU           : Any AVR with SRAM, 8 I/O pins and PWM 
//*
//* DESCRIPTION
//* This Application note describes how to generate DTMF tones using a single
//* 8 bit PWM output.
//*
//***************************************************************************
/**************************************************************************** 
 * U P D A T E   F O R   A T M E G A 3 2 8   /   A R D U I N O   F A M I L Y
 * 
 * Version Author        : Benjamin Russell KC7MMI
 * File Name             : DTMF_T1.ino
 * Date                  : 2018-11-11
 * Version               : 2.0
 * Target MCU            : ATmega328P
 * 
 * DESCRIPTION
 * This is a port of the original application note issued by Atmel in 2000
 * for the application of generating DTMF tones with the AVR family of 
 * Microcontrollers.  Code as-is was incompatible with Arduino and the 
 * ATmegal328 microcontroller.  Program uses 8 inputs for hex keypad input
 * and 1 PWM output.
 * 
 * INPUTS--   OUTPUT--
 * COL  ROW   Pin 9 (PB1)
 * PC0  PD4
 * PC1  PD5
 * PC2  PD6
 * PC3  PD7
 * 
 * Program Space:   922 bytes
 * Global Variables: 23 bytes
 ******************************************************************************/

#define  Xtal       16000000         // system clock frequency
#define  prescaler  1                // timer1 prescaler
#define  N_samples  128              // Number of samples in lookup table
#define  Fck        Xtal/prescaler   // Timer1 working frequency
#define  DLY        3                // port B setup delay in uS

//************************** SIN TABLE *************************************
// Samples table : one period sampled on 128 samples and
// quantized on 7 bit
//**************************************************************************
const unsigned char auc_SinParam [128] PROGMEM =  //stores array in program space which saves 128 bytes of SRAM
{
  64,67,70,73,76,79,82,85,88,91,94,96,99,102,104,106,109,111,113,115,117,118,120,121,123,124,125,126,126,127,127,127,127,127,127,127,126,
  126,125,124,123,121,120,118,117,115,113,111,109,106,104,102,99,96,94,91,88,85,82,79,76,73,70,67,64,60,57,54,51,48,45,42,39,36,33,31,28,
  25,23,21,18,16,14,12,10,9,7,6,4,3,2,1,1,0,0,0,0,0,0,0,1,1,2,3,4,6,7,9,10,12,14,16,18,21,23,25,28,31,33,36,39,42,45,48,51,54,57,60
};

//***************************  x_SW  ***************************************
//Table of x_SW (excess 8): x_SW = ROUND(8*N_samples*f*510/Fck)
//                 EXAMPLE: x_SW = ROUND(8*128*freq*510/16000000)
//**************************************************************************

//high frequency (column)
const unsigned char auc_frequencyH [4] = 
{
  /* 1633hz ---> x_SW */ 53,
  /* 1477hz ---> x_SW */ 48,
  /* 1336hz ---> x_SW */ 44,
  /* 1209hz ---> x_SW */ 39
};

//low frequency (row) 
const unsigned char auc_frequencyL [4] = 
{
  /* 941hz ---> x_SW */ 31,
  /* 852hz ---> x_SW */ 28,
  /* 770hz ---> x_SW */ 25,
  /* 697hz ---> x_SW */ 23
};

//**************************  global variables  ****************************
unsigned char x_SWa = 0x00;               // step width of high frequency
unsigned char x_SWb = 0x00;               // step width of low frequency
unsigned int  i_CurSinValA = 0;           // position freq. A in LUT (extended format)
unsigned int  i_CurSinValB = 0;           // position freq. B in LUT (extended format)
unsigned int  i_TmpSinValA;               // position freq. A in LUT (actual position)
unsigned int  i_TmpSinValB;               // position freq. B in LUT (actual position)

//**************************************************************************
// Timer overflow interrupt service routine
//**************************************************************************
ISR(TIMER1_OVF_vect)
{ 
  // move Pointer about step width aheaed
  i_CurSinValA += x_SWa;       
  i_CurSinValB += x_SWb;
  // normalize Temp-Pointer
  i_TmpSinValA  =  (char)(((i_CurSinValA+4) >> 3)&(0x007F)); 
  i_TmpSinValB  =  (char)(((i_CurSinValB+4) >> 3)&(0x007F));
  // calculate PWM value and assign to compare register: high frequency value + 3/4 low frequency value
  OCR1A = (pgm_read_byte(&(auc_SinParam[i_TmpSinValA])) + (pgm_read_byte(&(auc_SinParam[i_TmpSinValB]))-(pgm_read_byte(&(auc_SinParam[i_TmpSinValB]))>>2)));
}

//**************************************************************************
// Initialization
//**************************************************************************
void setup(void)
{
  TIMSK1 = (1 << TOIE1);                  // TC1 Overflow Interrupt Enable
  TCCR1A = (1 << COM1A1) + (1 << WGM10);  // non inverting / 8Bit PWM
  TCCR1B = (1 << CS10);                   // CLK/1 = no prescaling
  DDRB   = (1 << DDB1);                   // PB1 (OC1A) as output
  sei();                                  // Interrupts enabled
}

//**************************************************************************
// Time delay to ensure a correct setting of the pins of Port B 
//**************************************************************************
void Delay (void)
{
  delayMicroseconds(DLY);  //delay 3uS--any less will cause problems
}

//**************************************************************************
// MAIN
// Read from portB (eg: using evaluation board switch) which
// tone to generate, extract mixing high frequency
// (column) and low frequency (row), and then
// fix x_SWa and x_SWb
// row    -> PINB high nibble
// column -> PINB low nibble
//**************************************************************************

void loop()
{
  unsigned char uc_Input;
  unsigned char uc_Counter = 0;
  while(true){ 
    //Row Input - Port D in & pull-up, Port C out & low
    DDRD  &= 0x0F;                    //Port D pins 4-7 to input
    PORTD |= 0xF0;                    //Port D pins pull-up
    DDRC  |= 0x0F;                    //Port C pins 0-3 to output
    PORTC &= 0xF0;                    //Port C pins 0-3 low
    uc_Counter = 0;
    Delay();                          // wait for Port lines to be set up correctly
    uc_Input = PIND & 0xF0;           // read Port D pins 4-7
    do 
    {
      if(!(uc_Input & 0x80))          // check if MSB is low
      {
                                      // if yes get step width and end loop
        x_SWb = auc_frequencyL[uc_Counter];  
        uc_Counter = 4;
      }
      else
      {
        x_SWb = 0;                    // no frequency modulation needed
      }
      uc_Counter++;
      uc_Input = uc_Input << 1;       // shift Bits one left
    } while ((uc_Counter < 4));
 
    //Column Input - Port C in & pull-up, Port D out & low
    DDRC  &= 0xF0;                    //Port C pins 0-3 to input
    PORTC |= 0x0F;                    //Port C pins 0-3 pull-up
    DDRD  |= 0xF0;                    //Port D pins 4-7 to output
    PORTD &= 0x0F;                    //Port D pins 4-7 low
    uc_Counter = 0;
    Delay();                          // wait for Port lines to be set up correctly
    uc_Input = PINC & 0x0F;           // read Port C pins 0-3
    uc_Input = uc_Input << 4;     
    do 
    {
      if(!(uc_Input & 0x80))          // check if MSB is low
      {
                                      // if yes get delay and end loop
        x_SWa = auc_frequencyH[uc_Counter];
        uc_Counter = 4;
      }
      else 
      {
        x_SWa = 0;                 
      }
      uc_Counter++;
      uc_Input = uc_Input << 1;
    } while (uc_Counter < 4);
  } 
}
