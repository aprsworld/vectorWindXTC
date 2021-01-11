#include "vectorWindXTC.h"

#define SERIAL_PREFIX 'A'
#define SERIAL_NUMBER 2233


void init() {
	setup_oscillator(OSC_8MHZ | OSC_INTRC);
	setup_adc(ADC_CLOCK_INTERNAL);
	setup_adc_ports(sAN0 | sAN1 | sAN2 | VSS_VREF );
//	setup_adc_ports(NO_ANALOGS);
//	setup_wdt(WDT_ON);

	/* 
	Manually set ANCON0 to 0xff and ANCON1 to 0x1f for all digital
	Otherwise set high bit of ANCON1 for VbGen enable, then remaining bits are AN12 ... AN8
	ANCON1 AN7 ... AN0
	set bit to make input digital
	*/
	/* AN7 AN6 AN5 AN4 AN3 AN2 AN1 AN0 */
	ANCON0 = 0b11111000;
	/* VbGen x x 12 11 10 9 8 */
	ANCON1 = 0b01111111;



//	port_b_pullups(TRUE);
	delay_ms(14);
}


void main(void) {
	int8 i;
	int16 l,m;

	init();

	l=m=0;

	output_low(LED_ORANGE);
	output_low(LED_RED);
	output_low(LED_GREEN);

#if 0
	fprintf(WORLD_SERIAL,"# slhc (%c%lu) start up (worldData stream) (modbus address=%u) (telem_seconds=%lu) %s\r\n",
		config.serial_prefix,
		config.serial_number,
		config.modbus_address,
		config.seconds_telem,
		__DATE__
	);	
#endif

	fprintf(SERIAL_XTC,"# vectorWindXTC (%s) on XTC\r\n",__DATE__);


	i=0;
	for ( ; ; ) {
		restart_wdt();
		m++;

		if ( kbhit(SERIAL_XTC) ) {
			i=fgetc(SERIAL_XTC);
			l++;

			switch ( i ) {
				case 'o': output_low(LED_ORANGE); break;
				case 'O': output_high(LED_ORANGE); break;

				case 'r': output_low(LED_RED); break;
				case 'R': output_high(LED_RED); break;

				case 'g': output_low(LED_GREEN); break;
				case 'G': output_high(LED_GREEN); break;
			}
		}

		if ( kbhit(SERIAL_GNSS) ) {
			i=fgetc(SERIAL_GNSS);
			l++;

			switch ( i ) {
				case '$': output_high(LED_ORANGE); break;
				case '*': output_low(LED_ORANGE); break;
			}
		}



//		delay_ms(100);

//		fprintf(SERIAL_XTC,"# PIN_B0=%u l=%lu m=%lu\r\n",input(PIN_B0),l,m);


	}
}
