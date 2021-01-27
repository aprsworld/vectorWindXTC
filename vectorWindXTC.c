#include "vectorWindXTC.h"

#define SERIAL_PREFIX 'A'
#define SERIAL_NUMBER 2233


#define N_GNSS_SENTENCE   2  /* how many GNSS sentences we support */
#define LEN_GNSS_SENTENCE 80 /* maximum GNSS setence length */


typedef struct {
	/* most recent valid */
	int16 pulse_period;     /* period for #40HC */
	int16 pulse_min_period; /* period for #40HC */
	int16 pulse_count;

	int16 strobed_pulse_period;
	int16 strobed_pulse_min_period;
	int16 strobed_pulse_count;


	int8 gnss_sentence[N_GNSS_SENTENCE][LEN_GNSS_SENTENCE];
	int8 gnss_sentence_pos[N_GNSS_SENTENCE];
	int8 gnss_sentence_age[N_GNSS_SENTENCE]; /* 10 mS */



	int16 input_voltage_adc;
	int16 vertical_anemometer_adc;
	int16 wind_vane_adc;
	int16 uptime;
} struct_current;


typedef struct {
	short now_gga_start;
	short now_hdt_done;
	short now_10millisecond;

	/* flag to see if we are timing */
	short pulse_0_count;
} struct_action;



typedef struct {
	int16 pulse_period;
	int16 pulse_count; /* used in frequency counting interrupt to count pulses in second */
} struct_time_keep;




/* global structures */
struct_current current;
struct_action action;
struct_time_keep timers;
//struct_gps_data gps;



#include "vectorWindXTC_adc.c"
#include "vectorWindXTC_interrupts.c"


void task_10millisecond(void) {
	int8 i;

	for ( i=0 ; i<N_GNSS_SENTENCE ; i++ ) {
		if ( current.gnss_sentence_age[i] < 255 ) {
			current.gnss_sentence_age[i]++;
		}
	}

	if ( current.strobed_pulse_count > 0 ) {
		output_high(LED_ANEMOMETER);
	} else {
		output_low(LED_ANEMOMETER);
	}

	if ( current.vertical_anemometer_adc > 782 || current.vertical_anemometer_adc < 770 ) {
		output_high(LED_VERTICAL_ANEMOMETER);
	} else {
		output_low(LED_VERTICAL_ANEMOMETER);
	}
}


void init() {
//	setup_oscillator(OSC_8MHZ | OSC_INTRC);
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

	/* one periodic interrupt @ 100uS. Generated from system 8 MHz clock */
	/* prescale=4, match=49, postscale=1. Match is 49 because when match occurs, one cycle is lost */
	setup_timer_2(T2_DIV_BY_4,49,1); 

//	port_b_pullups(TRUE);
	delay_ms(14);

	action.now_gga_start=0;
	action.now_hdt_done=0;
	action.now_10millisecond=0;

	current.pulse_period=0;
	current.pulse_min_period=65535;
	current.pulse_count=0;
	
}


void main(void) {
	int8 i;
	int16 l,m;

	init();

	l=m=0;

	output_low(LED_ORANGE);
	output_low(LED_RED);
	output_low(LED_GREEN);


	fprintf(SERIAL_XTC,"# vectorWindXTC (%s) on XTC\r\n",__DATE__);

#if 1
	/* start 100uS timer */
	enable_interrupts(INT_TIMER2);
	/* enable serial ports */
	enable_interrupts(INT_RDA);
//	enable_interrupts(INT_RDA2);
	
	enable_interrupts(GLOBAL);
#endif

	i=0;
	for ( ; ; ) {
		restart_wdt();
		m++;

#if 1
		if ( current.gnss_sentence_age[GNSS_SENTENCE_GGA] >= 120 ) {
			/* didn't get a $xxGGA sentence from GNSS for last 1.2 seconds. Send data anyhow */
			current.gnss_sentence_age[GNSS_SENTENCE_GGA]=0;
			action.now_gga_start=1;
		}
#endif

		/* as soon as interrupt flags a $xxGGA we save our data */	
		if ( action.now_gga_start ) {
			action.now_gga_start=0;

			disable_interrupts(GLOBAL);
			/* save (strobe) our data */
			current.strobed_pulse_period=current.pulse_period;
			current.strobed_pulse_min_period=current.pulse_min_period;
			current.strobed_pulse_count=current.pulse_count;

			/* reset our data */
			current.pulse_period=0;
			current.pulse_min_period=65535;
			current.pulse_count=0;

			enable_interrupts(GLOBAL);

			sample_adc();

			fprintf(SERIAL_XTC,"# got $xxGGA\r\n");
		}	


#if 1
		if ( current.gnss_sentence_age[GNSS_SENTENCE_HDT] >= 125 ) {
			/* didn't get a $xxGGA sentence from GNSS for last 1.25 seconds. Send data anyhow */
			current.gnss_sentence_age[GNSS_SENTENCE_HDT]=0;
			action.now_hdt_done=1;
		}
#endif

		/* as soon as interrupt finishes a $xxHDT we send our data */
		if ( action.now_hdt_done) { 
			action.now_hdt_done=0;

			fprintf(SERIAL_XTC,"# finished $xxHDT\r\n");

			fprintf(SERIAL_XTC,"# {count=%lu, period=%lu, min_period=%lu}\r\n",
				current.strobed_pulse_count,
				current.strobed_pulse_period,
				current.strobed_pulse_min_period
			);
			fprintf(SERIAL_XTC,"# {input=%lu, vertical=%lu, wind_vane=%lu}\r\n",
				current.input_voltage_adc,
				current.vertical_anemometer_adc,
				current.wind_vane_adc
			);
		}

		/* periodic tasks */
		if  ( action.now_10millisecond ) {
			action.now_10millisecond=0;
			task_10millisecond();
		}




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

#if 0
		if ( kbhit(SERIAL_GNSS) ) {
			i=fgetc(SERIAL_GNSS);
			l++;

			switch ( i ) {
				case '$': output_high(LED_ORANGE); break;
				case '*': output_low(LED_ORANGE); break;
			}
		}
#endif



	
//		delay_ms(100);

//		fprintf(SERIAL_XTC,"# PIN_B0=%u l=%lu m=%lu\r\n",input(PIN_B0),l,m);


	}
}
