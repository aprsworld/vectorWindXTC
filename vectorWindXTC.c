#include "vectorWindXTC.h"

#define SERIAL_PREFIX 'R'
#define SERIAL_NUMBER 1040




const int8 NMEA0183_TRIGGER[] = { 'G', 'P', 'H', 'D', 'T' };



typedef struct {
	/* most recent valid */
	int16 pulse_period;     /* period for #40HC */
	int16 pulse_min_period; /* period for #40HC */
	int16 pulse_count;

	int16 strobed_pulse_period;
	int16 strobed_pulse_min_period;
	int16 strobed_pulse_count;

	int16 input_voltage_adc;
	int16 vertical_anemometer_adc;
	int16 wind_vane_adc;
	int16 sequence;

	int8 live_age;

	int8 gnss_age;
	int8 gnss_buff[254];
	int8 gnss_length;
} struct_current;


typedef struct {
	short now_nmea_raw_received;
	short now_strobe_counters;
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


typedef struct {
	int8 buff[254];
	int8 pos;
} struct_nmea_raw;

/* global structures */
struct_current current;
struct_action action;
struct_time_keep timers;
struct_nmea_raw nmea_raw;


#include "vectorWindXTC_adc.c"
#include "vectorWindXTC_interrupts.c"
#include "vectorWindXTC_live.c"


void task_10millisecond(void) {
	if ( current.gnss_age < 255 ) {
		current.gnss_age++;
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
//	int8 i;
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
//	setup_timer_2(T2_DIV_BY_4,49,1); 

	/* one periodic interrupt @ 100uS. Generated from system 32 MHz clock */
	/* prescale=16, match=49, postscale=1. Match is 49 because when match occurs, one cycle is lost */
	setup_timer_2(T2_DIV_BY_16,49,1); 


	/* not sure why we need this */
	delay_ms(14);


	action.now_strobe_counters=0;
	action.now_gnss_trigger_start=0;
	action.now_gnss_trigger_done=0;
	action.now_10millisecond=0;

	current.pulse_period=0;
	current.pulse_min_period=65535;
	current.pulse_count=0;

	nmea_raw.buff[0]='\0';
	nmea_raw.pos=0;

	current.gnss_age=255;;
	current.gnss_buff[0]='\0';
	current.gnss_length=0;
	
	current.sequence=0;
}


void main(void) {
	int8 i;
//	int16 l,m;

	init();

	output_low(LED_ORANGE);
	output_low(LED_RED);
	output_low(LED_GREEN);


//	fprintf(SERIAL_XTC,"# vectorWindXTC (%s) on XTC\r\n",__DATE__);

	/* start 100uS timer */
	enable_interrupts(INT_TIMER2);
	/* enable serial ports */
	enable_interrupts(INT_RDA);
	enable_interrupts(GLOBAL);

	i=0;
	for ( ; ; ) {
		restart_wdt();


#if 1
		if ( current.live_age >= 120 ) {
			/* didn't get a triger sentence from GNSS for last 1.2 seconds. Send data anyhow */
			action.now_strobe_counters=1;    /* triggers strobe of data */
			action.now_gnss_trigger_done=1;  /* triggers send of data */
//			fprintf(SERIAL_XTC,"# timeout waiting for trigger\r\n");
		}
#endif

		/* recieving a GNSS trigger on our serial port causes counter values to be strobe. Once that
		is complete, we sample ADCs */	
		if ( action.now_gnss_trigger_start ) {
			action.now_gnss_trigger_start=0;

			sample_adc();

//			fprintf(SERIAL_XTC,"# got trigger sentence\r\n");
		}	

		/* as soon as interrupt finishes a $xxHDT we send our data */
		if ( action.now_gnss_trigger_done) { 
			action.now_gnss_trigger_done=0;

#if 0
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
			fprintf(SERIAL_XTC,"# current {gnss_age=%u gnss='%s'}\r\n",
				current.gnss_age,
				current.gnss_buff
			);
#endif

			live_send();

			current.live_age=0;
		}

		/* periodic tasks */
		if  ( action.now_10millisecond ) {
			action.now_10millisecond=0;
			task_10millisecond();
		}
	}
}
