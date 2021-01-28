#include <18F24J11.h>
#device ADC=10
#device *=16
#device HIGH_INTS=TRUE
//#fuses WDT32768, NOIESO, INTRC_IO, NOFCMEN, NODEBUG, NOSTVREN, T1DIG
#fuses NOWDT, NOIESO, HSPLL, NOFCMEN, NODEBUG, NOSTVREN, T1DIG


#include <stdlib.h>
#use delay(crystal=8MHz, clock=32MHz, restart_wdt)

/* Vector GNSS */
#use rs232(UART1,stream=SERIAL_GNSS,baud=19200,xmit=PIN_C6,rcv=PIN_C7,ERRORS)	
// for PIC18F24J11
//#byte TXSTA=0xfad
//#bit  TRMT=TXSTA.1
#byte ANCON0=GETENV("SFR:ancon0")
#byte ANCON1=GETENV("SFR:ancon1")

/* UART2 - XTC */
#pin_select U2TX=PIN_C0
#pin_select U2RX=PIN_C1
#use rs232(UART2,stream=SERIAL_XTC, baud=57600,ERRORS)	


#use standard_io(A)
#use standard_io(B)
#use standard_io(C)

#define LED_ORANGE          PIN_B5
#define LED_RED             PIN_B4
#define LED_GREEN           PIN_B3
#define ANEMOMETER_FILTERED PIN_B0

#define LED_VERTICAL_ANEMOMETER LED_GREEN
#define LED_WIRELESS            LED_RED
#define LED_ANEMOMETER          LED_ORANGE



#define GNSS_PORT_A_TX      PIN_C7
#define GNSS_PORT_A_RX      PIN_C6
#define FROM_MODEM_DATA     PIN_C1
#define TO_MODEM_DATA       PIN_C0

/* analog inputs */
#define AN_IN_VOLTS        PIN_A0
#define AN0_FILTERED       PIN_B0
#define AN1_FILTERED       PIN_B1
#define WV0_FILTERED       PIN_A1
#define WV1_FILTERED       PIN_A2
#define ADC_AN_IN_VOLTS    0
#define ADC_WV0_FILTERED   1
#define ADC_WV1_FILTERED   2
