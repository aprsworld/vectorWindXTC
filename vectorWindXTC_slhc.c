#include "slhc.h"


typedef struct {
	int8 modbus_address;
	int8 modbus_mode;

	int8 serial_prefix;
	int16 serial_number;

	int16 adc_sample_ticks;   
	int8  adc_offset[10];
	int16 seconds_telem;

	int8  load_control_mode;
	int16 load_insolation_on;
	int16 load_insolation_off;
	int16 load_volts_disconnect;
	int16 load_volts_reconnect;
	int16 load_transition_delay;
	int16 load_evening_seconds;
	int16 load_morning_seconds;
} struct_config;



typedef struct {
	/* circular buffer for ADC readings */
	int16 adc_buffer[10][16];
	int8  adc_buffer_index;

	int16 modbus_our_packets;
	int16 modbus_other_packets;
	int16 modbus_last_error;

	int16 sequence_number;
	int16 uptime_minutes;
	int16 interval_milliseconds;

	int8 factory_unlocked;

	int16 pulse_count;
	int16 pulse_period;
	int16 pulse_min_period;

	int8  load_disconnect;

	int8  state_evening_morning;
} struct_current;

typedef struct {
	int8 led_on_green;
	int8 led_on_red;

	int1 now_adc_sample;
	int1 now_adc_reset_count;
	int1 now_count;
	int1 now_telem;
	int1 count_load_on_seconds;
	int1 count_load_on_delay_seconds;
	int1 count_load_off_delay_seconds;

	int8 xbee_wakeup;
	int8 xbee_shutdown;

	int16 pulse_period;


	int16 load_on_seconds;
	int16 load_on_delay_seconds;
	int16 load_off_delay_seconds;

	int16 last_night_seconds[4];

	int16 evening_countdown_seconds;
	int16 morning_countdown_to_seconds;
	int16 morning_countdown_seconds;
} struct_time_keep;


/* global structures */
struct_config config;
struct_current current;
struct_time_keep timers;

/* function prototypes */
int8 read_jumpers(void);
void xbee_off(void);
int16 nightLength(void);

#include "interrupt.c"
#include "param.c"
#include "ltc4151.c"
#include "adc.c"

#include "modbus_slave_slhc.c"
#include "modbus_handler_slhc.c"
#include "live.c"

void xbee_off(void) {
	timers.xbee_wakeup=255;
	timers.xbee_shutdown=5;
}

void init() {
	setup_oscillator(OSC_8MHZ | OSC_INTRC);
	setup_adc(ADC_CLOCK_INTERNAL);
	setup_adc_ports(sAN0 | sAN1 | sAN2 | sAN3 | VSS_VDD);
//	setup_adc_ports(NO_ANALOGS);
	setup_wdt(WDT_ON);

	/* 
	Manually set ANCON0 to 0xff and ANCON1 to 0x1f for all digital
	Otherwise set high bit of ANCON1 for VbGen enable, then remaining bits are AN12 ... AN8
	ANCON1 AN7 ... AN0
	set bit to make input digital
	*/
	/* AN7 AN6 AN5 AN4 AN3 AN2 AN1 AN0 */
	ANCON0 = 0b11110000;
	/* VbGen x x 12 11 10 9 8 */
	ANCON1 = 0b01111111;


	/* data structure initialization */
	timers.led_on_green=0;
	timers.led_on_red=0;
	timers.now_adc_sample=0;
	timers.now_count=0;
	timers.now_telem=0;
	timers.xbee_wakeup=255;
	timers.xbee_shutdown=250;

	timers.count_load_on_seconds=0;
	timers.load_on_seconds=0;
	timers.count_load_on_delay_seconds=0;
	timers.load_on_delay_seconds=0;
	timers.count_load_off_delay_seconds=0;
	timers.load_off_delay_seconds=0;
	timers.last_night_seconds[0]=0;
	timers.last_night_seconds[1]=0;
	timers.last_night_seconds[2]=0;
	timers.last_night_seconds[3]=0;
	timers.evening_countdown_seconds=65535;
	timers.morning_countdown_to_seconds=65535;
	timers.morning_countdown_seconds=65535;

	current.pulse_period=0;
	current.pulse_min_period=65535;
	current.pulse_count=0;
	current.modbus_our_packets=0;
	current.modbus_other_packets=0;
	current.modbus_last_error=0;
	current.sequence_number=0;
	current.uptime_minutes=0;
	current.interval_milliseconds=0;
	current.adc_buffer_index=0;
	current.factory_unlocked=0;
	current.load_disconnect=0;
	current.state_evening_morning=0;

	output_float(I2C_SDA);
	output_float(I2C_SCL);

	/* interrupts */
	/* timer0 - Modbus slave timeout timer */
	/* configured in modbus_slave_sdc.c */

	/* timer1 - unused */

	/* timer2 - 10 kHz / 100uS periodic */
	setup_timer_2(T2_DIV_BY_1,200,1); 
	set_timer2(0);
	enable_interrupts(INT_TIMER2);

	/* timer3 - general housekeeping Prescaler=1:1; TMR1 Preset=45536; Freq=100.00Hz; Period=10.00 ms */
	setup_timer_3(T3_INTERNAL | T3_DIV_BY_1);
	set_timer3(45536);
	enable_interrupts(INT_TIMER3);

	/* timer4 - unused */

	/* external interrupt - anemometer */
	ext_int_edge(0,H_TO_L);
	enable_interrupts(INT_EXT);


	port_b_pullups(TRUE);
	delay_ms(14);
}

int8 read_jumpers() {
	int8 v=0;
	if ( ! input(J4_1) ) v+=1;
	if ( ! input(J4_2) ) v+=2;
	if ( ! input(J5_1) ) v+=4;
	if ( ! input(J5_2) ) v+=8;

	return v;
}

void nightAdd(int16 val) { 
	static int8 pos=0;

	timers.last_night_seconds[pos]=val;
	pos++;

	if ( pos >= 4 )
		pos=0;
}

int16 nightLength(void) {
	int8 i,n;
	int16 l;
	int32 sum;


	sum=0;
	n=0;
	for ( i=0 ; i<4 ; i++ ) {
		if ( timers.last_night_seconds[i] > 0 && timers.last_night_seconds[i] < 65535 ) {
			sum += timers.last_night_seconds[i];
			n++;
		}
	}

	l = sum / n;
	return l;
}

void loadOn(void) {
	if ( 1 != current.load_disconnect ) {
		output_high(LIGHT_CTL);
	}
}

void loadOff(void) {
	output_low(LIGHT_CTL);
}

void main(void) {
	int8 i;
	int16 l,m;

	init();
	read_param_file();

	if ( config.modbus_address > 127 ) {
		write_default_param_file();
	}

	/* start out Modbus slave */
	setup_uart(TRUE);
	/* modbus_init turns on global interrupts */
	modbus_init();
	/* modbus initializes @ 9600 */

	/* if default serial number is set, we attempt to set baud
	   rate from 9600 to 57600, channel to D, and pin hibernate */
	if ( config.serial_prefix == SERIAL_PREFIX_DEFAULT && 
         config.serial_number == SERIAL_NUMBER_DEFAULT ) {

		timers.led_on_red=3000;
		set_uart_speed(9600,WORLD_SERIAL);
		delay_ms(1200);
		fprintf(WORLD_SERIAL,"+++");
		delay_ms(1200);

		fprintf(WORLD_SERIAL,"ATBD=6\r");
		delay_ms(100);
		fprintf(WORLD_SERIAL,"ATCH=D\r");
		delay_ms(100);
		fprintf(WORLD_SERIAL,"ATSM=1\r");
		delay_ms(100);

		/* exit command mode */
		fprintf(WORLD_SERIAL,"ATWR\r");
		delay_ms(100);
		fprintf(WORLD_SERIAL,"ATCN\r");
		delay_ms(100);

		timers.led_on_red=0;
		
		set_uart_speed(57600,WORLD_SERIAL);
	}


	fprintf(MODBUS_SERIAL,"# slhc (%c%lu) start up (modbus stream) (modbus address=%u) %s\r\n",
		config.serial_prefix,
		config.serial_number,
		config.modbus_address,
		__DATE__
	);

	fprintf(WORLD_SERIAL,"# slhc (%c%lu) start up (worldData stream) (modbus address=%u) (telem_seconds=%lu) %s\r\n",
		config.serial_prefix,
		config.serial_number,
		config.modbus_address,
		config.seconds_telem,
		__DATE__
	);	

	/* prime the ADC filter, otherwise first 16 samples or so can be garabage */
	for ( i=0 ; i<20 ; ) {
		if ( timers.now_adc_sample ) {
			timers.now_adc_sample=0;
			adc_update();
			i++;
		}
	}
	timers.led_on_red=timers.led_on_green=0;

	/* fast red, yellow, green */
	timers.led_on_red  =200;
	delay_ms(160);
	timers.led_on_green=400;
	delay_ms(160);
	timers.led_on_red=0;
	delay_ms(160);
	timers.led_on_green=0;

	i=0;
	for ( ; ; ) {
		restart_wdt();

		if ( timers.now_adc_sample ) {
			timers.now_adc_sample=0;
			adc_update();
		}

		if ( timers.now_telem ) {
			live_send_all();
		}


		/* low voltage disconnect for load */
		l=adc_get_avg(ADC_CH_SOLAR_VOLTS)>>2; /* convert to tenths of a volt */
		if ( 1==current.load_disconnect && l>=config.load_volts_reconnect ) {
			current.load_disconnect=0;
		} else if ( l<config.load_volts_disconnect ) {
			current.load_disconnect=1;
		}

		if ( read_jumpers() & 0b1 ) {
			/* install jumper JP4.1 to force light on */
			output_high(LIGHT_CTL);
		} else if ( read_jumpers() & 0b100 ) {
			/* install jumper JP5.1 to force light off */
			output_low(LIGHT_CTL);
		} else if ( LOAD_MODE_ON == config.load_control_mode ) {
			/* load on if JP4.1 is jumpered or mode is set to LOAD_MODE_ON via Modbus */
			output_high(LIGHT_CTL);
		} else if ( LOAD_MODE_OFF == config.load_control_mode ) {
			output_low(LIGHT_CTL);
		} else {
			/* either LOAD_MODE_DAWN_DUSK or LOAD_MODE_EVENING_MORNING */
			l=adc_get_avg(ADC_CH_SOLAR_INSOL);

			if ( read_jumpers() & 0b10 ) {
				/* dawn to dusk when J4.2 installed */
				if ( l < config.load_insolation_on ) {
					timers.count_load_off_delay_seconds=0;
					timers.load_off_delay_seconds=0;
	
					if ( timers.load_on_delay_seconds >= config.load_transition_delay ) {
						timers.count_load_on_seconds=1;

						loadOn();
					} else {
						timers.count_load_on_delay_seconds=1;
					}
				}
	
				if (  l > config.load_insolation_off ) {
					timers.count_load_on_delay_seconds=0;
					timers.load_on_delay_seconds=0;
	
					if ( timers.load_off_delay_seconds >= config.load_transition_delay ) {
						/* turning off for the morning. Save the length of the night */
			
						timers.count_load_on_seconds=0;
						/* this gets repeatedly called, so only save non-zero night times */
						if ( timers.load_on_seconds > 0 ) 
							nightAdd(timers.load_on_seconds);
	
						timers.load_on_seconds=0;
						loadOff();
					} else {
						timers.count_load_off_delay_seconds=1;
					}
				}
	
				if ( 1==current.load_disconnect ) {
					loadOff();
				}
			} else {
				/* morning and evening since J4.2 not installed */
				if ( l < config.load_insolation_on ) {
					timers.count_load_off_delay_seconds=0;
					timers.load_off_delay_seconds=0;
	
					if ( timers.load_on_delay_seconds >= config.load_transition_delay ) {
						timers.count_load_on_seconds=1;
	
						if ( 65535 == timers.evening_countdown_seconds ) {
							current.state_evening_morning=1;
							/* just got dark, start our evening countdown and seconds to morning countdown */
							timers.evening_countdown_seconds=config.load_evening_seconds;
							m=nightLength();
							if ( m > config.load_morning_seconds ) {
								timers.morning_countdown_to_seconds = m - config.load_morning_seconds;
							} else {
								timers.morning_countdown_to_seconds=65535; /* no morning time */
							}
						} else if ( timers.evening_countdown_seconds != 65535 && timers.evening_countdown_seconds > 0 ) {
							current.state_evening_morning=2;
							/* in evening on time */
							loadOn();					
						} else if ( timers.morning_countdown_seconds != 65535 && timers.morning_countdown_seconds > 0 ) {
							current.state_evening_morning=3;
							/* in morning on time */
							loadOn();					
						} else if ( 0 == timers.morning_countdown_to_seconds ) {
							current.state_evening_morning=4;
							/* time to transitition to morning, act on next pass */
							timers.morning_countdown_to_seconds=65535; /* done with countdown */
							timers.morning_countdown_seconds=config.load_morning_seconds; /* seconds to stay on */
						} else if ( 0 == timers.evening_countdown_seconds ) {
							current.state_evening_morning=5;
							/* done with evening on time */
							loadOff();
						} else if ( 0 == timers.morning_countdown_seconds ) {
							current.state_evening_morning=6;
							/* done with morning on time */
							timers.morning_countdown_seconds=65535;
							loadOff();
						}

					} else {
						current.state_evening_morning=7;
						timers.count_load_on_delay_seconds=1;
					}
				}
	
				if (  l > config.load_insolation_off ) {
					timers.count_load_on_delay_seconds=0;
					timers.load_on_delay_seconds=0;
	
					if ( timers.load_off_delay_seconds >= config.load_transition_delay ) {
						/* turning off for the morning. Save the length of the night */
			
						timers.count_load_on_seconds=0;
						/* this gets repeatedly called, so only save non-zero night times */
						if ( timers.load_on_seconds > 0 ) 
							nightAdd(timers.load_on_seconds);
	
						timers.load_on_seconds=0;

						/* dawn dusk timers */
						timers.evening_countdown_seconds=65535;
						timers.morning_countdown_to_seconds=65535;
						timers.morning_countdown_seconds=65535;

						loadOff();
					} else {
						timers.count_load_off_delay_seconds=1;
					}
				}
	
				if ( 1==current.load_disconnect ) {
					loadOff();
				}


			}
		}

		modbus_process();
		
	}
}
