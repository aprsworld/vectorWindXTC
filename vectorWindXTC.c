#include "vectorWindXTC.h"

#define SERIAL_PREFIX 'R'
#define SERIAL_NUMBER 1036


#define NMEA0183_SENTENCE_GPGGA 0
#define NMEA0183_SENTENCE_GPGLL 1
#define NMEA0183_SENTENCE_GPGNS 2
#define NMEA0183_SENTENCE_GPGRS 3
#define NMEA0183_SENTENCE_GPGSA 4
#define NMEA0183_SENTENCE_GPGST 5
#define NMEA0183_SENTENCE_GPGSV 6
#define NMEA0183_SENTENCE_GPRMC 7
#define NMEA0183_SENTENCE_GPRRE 8
#define NMEA0183_SENTENCE_GPVTG 9
#define NMEA0183_SENTENCE_GPZDA 10
#define NMEA0183_SENTENCE_GPROT 11
#define NMEA0183_SENTENCE_GPHEV 12
#define NMEA0183_SENTENCE_GPHDT 13
#define NMEA0183_SENTENCE_GPHDM 14
#define NMEA0183_SENTENCE_GBGSV 15
#define NMEA0183_SENTENCE_GLGSV 16
#define NMEA0183_SENTENCE_GNGNS 17
#define NMEA0183_SENTENCE_GNGSA 18

/* how many sentences we have statically allocate memory for */
#define NMEA0183_N_SENTENCE   2   
/* maximum GNSS setence length to store, less the first '$' */
#define NMEA0183_LEN_SENTENCE 110 

typedef struct {
	int8 id;                          /* defined above */
	int8 age;                         /* 0.010 second increments */
	int8 prefix[6];                   /* example: "GPRMC", null terminated */
	int8 data[NMEA0183_LEN_SENTENCE]; /* data from GNSS after '$' and before '\r' or '\n', null terminated */
} struct_nmea0183_sentence;



typedef struct {
	/* most recent valid */
	int16 pulse_period;     /* period for #40HC */
	int16 pulse_min_period; /* period for #40HC */
	int16 pulse_count;

	int16 strobed_pulse_period;
	int16 strobed_pulse_min_period;
	int16 strobed_pulse_count;

	struct_nmea0183_sentence nmea_sentence[NMEA0183_N_SENTENCE];


	int16 input_voltage_adc;
	int16 vertical_anemometer_adc;
	int16 wind_vane_adc;
	int16 uptime;

	int8 live_age;
} struct_current;


typedef struct {
	short now_gnss_trigger_start;
	short now_gnss_trigger_done;
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



#include "vectorWindXTC_adc.c"
#include "vectorWindXTC_interrupts.c"

int8 xtoi(int8 h) {
	if ( h>='0' && h<='9' ) {
		return h-'0';
	}

	h=toupper(h);
	if ( h>='A' && h<='F' ) {
		return (10 + (h-'A'));
	}

	return 0;
}

int8 nmea0183_is_valid(char *p) {
	/* check if a string that starts after the '$' and includes the '*' checksum is valid */
	int16 lcrc, rcrc;
	int8 i;

	lcrc=rcrc=0;

	for ( i=0 ; i<NMEA0183_LEN_SENTENCE<2 ; i++ ) {
		if ( '\0' == p[i] ) {
			/* reached the end of the string without getting a '*' */
			return 0;
		}

		if ( '*' == p[i] ) {
			rcrc = 16*xtoi(p[i+1]) + xtoi(p[i+2]);

			if ( rcrc == lcrc ) {
				return 1;
			}
		} else {
			lcrc = lcrc ^ p[i];
		}
	}
	
	/* should only get here if we never encounter a '*' */
	return 0;
}

void task_10millisecond(void) {
	int8 i;

	/* age NMEA0183 sentences */
	for ( i=0 ; i<NMEA0183_N_SENTENCE ; i++ ) {
		if ( current.nmea_sentence[i].age < 255 ) {
			current.nmea_sentence[i].age++;
		}
	}

	/* age live data timeout */
	if ( current.live_age < 255 ) {
		current.live_age++;
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
	int8 i;
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

	action.now_gnss_trigger_start=0;
	action.now_gnss_trigger_done=0;
	action.now_10millisecond=0;

	current.pulse_period=0;
	current.pulse_min_period=65535;
	current.pulse_count=0;

	for ( i=0 ; i<NMEA0183_N_SENTENCE ; i++ ) {
		current.nmea_sentence[i].age=255;
		current.nmea_sentence[i].prefix[0]='\0';
		current.nmea_sentence[i].data[0]='\0';
	}

	/* configure the sentence we want to capture */
	/* 0 index sentence is used as the trigger */
	current.nmea_sentence[0].id=NMEA0183_SENTENCE_GPHDT;
	strcpy(current.nmea_sentence[0].prefix,"GPHDT");

	current.nmea_sentence[1].id=NMEA0183_SENTENCE_GPRMC;
	strcpy(current.nmea_sentence[1].prefix,"GPRMC");

	
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

	/* start 100uS timer */
	enable_interrupts(INT_TIMER2);
	/* enable serial ports */
	enable_interrupts(INT_RDA);
	enable_interrupts(GLOBAL);

	i=0;
	for ( ; ; ) {
		restart_wdt();
		m++;

#if 1
		if ( current.live_age >= 120 ) {
			/* didn't get a triger sentence from GNSS for last 1.2 seconds. Send data anyhow */
			action.now_gnss_trigger_start=1; /* triggers strobe of data */
			action.now_gnss_trigger_done=1;  /* triggers send of data */
			fprintf(SERIAL_XTC,"# timeout waiting for trigger\r\n");
		}
#endif

		/* as soon as interrupt flags trigger sentence start, we save our data */	
		if ( action.now_gnss_trigger_start ) {
			action.now_gnss_trigger_start=0;

			/* turn off interrupts so data isn't changed during copy*/
			disable_interrupts(GLOBAL);

			/* save (strobe) our data */
			current.strobed_pulse_period=current.pulse_period;
			current.strobed_pulse_min_period=current.pulse_min_period;
			current.strobed_pulse_count=current.pulse_count;

			/* reset our data */
			current.pulse_period=0;
			current.pulse_min_period=65535;
			current.pulse_count=0;

			/* turn back on interrupts */
			enable_interrupts(GLOBAL);

			sample_adc();

			fprintf(SERIAL_XTC,"# got trigger sentence\r\n");
		}	

		/* as soon as interrupt finishes a $xxHDT we send our data */
		if ( action.now_gnss_trigger_done) { 
			action.now_gnss_trigger_done=0;

			fprintf(SERIAL_XTC,"# finished receiving trigger sentence or timeout\r\n");
		
			fprintf(SERIAL_XTC,"# current.live_age=%u\r\n",current.live_age);

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

			for ( i=0 ; i < NMEA0183_N_SENTENCE ; i++ ) {
				fprintf(SERIAL_XTC,"# nmea_sentence[%u] id=%u valid=%u age=%u prefix='%s' '%s'\r\n",
					i,
					current.nmea_sentence[i].id,
					nmea0183_is_valid(current.nmea_sentence[i].data),
					current.nmea_sentence[i].age,
					current.nmea_sentence[i].prefix,
					current.nmea_sentence[i].data
				);
			}

			current.live_age=0;

		}

		/* periodic tasks */
		if  ( action.now_10millisecond ) {
			action.now_10millisecond=0;
			task_10millisecond();
		}



#if 0
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
#endif



	
//		delay_ms(100);

//		fprintf(SERIAL_XTC,"# PIN_B0=%u l=%lu m=%lu\r\n",input(PIN_B0),l,m);


	}
}
