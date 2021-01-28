

#int_timer2  HIGH
/* this timer only runs once booted */
void isr_100us(void) {
	static int8 tick=0;

	/* anemometer polling state variables */
	/* anemometer 0 / PIN_B0 */
	short ext0_count;
	short ext0_now;
	static short ext0_last=0;
	static short ext0_state=0;


	/* every 100 cycles we tell main() loop to do 10 milisecond activities */
	tick++;
	if ( 100 == tick ) {
		tick=0;
		action.now_10millisecond=1;
	}
	

	/* count time between falling edges */
	if ( ext0_count && 0xffff != timers.pulse_period )
		timers.pulse_period++;

	/* anemometer 0 / PIN_B0 trigger on falling edge */
	ext0_now=input(PIN_B0);
	if ( 0 == ext0_now && 1 == ext0_last ) {
		current.pulse_count++;
		if ( 1 == ext0_state ) {
			/* currently counting, time to finish */
			ext0_count=0;
			current.pulse_period=timers.pulse_period;
			if ( current.pulse_period < current.pulse_min_period ) {
				current.pulse_min_period=current.pulse_period;
			}
			ext0_state=0;
		}
		if ( 0 == ext0_state ) {
			/* not counting, time to start */
			timers.pulse_period=0;
			ext0_count=1;
			ext0_state=1;
		}
	}
	ext0_last = ext0_now;

	if ( action.now_strobe_counters ) {
		action.now_strobe_counters=0;

		/* save (strobe) our data */
		current.strobed_pulse_period=current.pulse_period;
		current.strobed_pulse_min_period=current.pulse_min_period;
		current.strobed_pulse_count=current.pulse_count;

		/* reset our data */
		current.pulse_period=0;
		current.pulse_min_period=65535;
		current.pulse_count=0;

		/* counters are copied, now we can start the rest of the measurements */
		action.now_gnss_trigger_start=1;
	}
}




#int_rda2
void serial_isr_wireless(void) {
	int8 c;

	c=fgetc(SERIAL_XTC);
}

#define GNSS_STATE_WAITING 0
#define GNSS_STATE_IN      1



#if 0
void serial_isr_gnss(void) {
	static int8 receive_state=GNSS_STATE_WAITING;
	int8 c;

	c=fgetc(SERIAL_GNSS);


	if ( 1==action.now_nmea_raw_received ) {
		/* still haven't processed our last message */
		return;
	}

	if ( GNSS_STATE_WAITING==receive_state && '$'==c ) {
		receive_state=GNSS_STATE_IN;
		nmea_raw.pos=0;
		return;
	}

	if ( GNSS_STATE_IN==receive_state ) {
		if ( '\r'==c || '\n'==c ) {
			receive_state=GNSS_STATE_WAITING;
			action.now_nmea_raw_received=1;

			output_toggle(LED_RED);

			return;
		} else if ( nmea_raw.pos < (sizeof(nmea_raw.buff) - 1) ) {
			nmea_raw.buff[nmea_raw.pos]=c;
			nmea_raw.pos++;
			nmea_raw.buff[nmea_raw.pos]='\0';
		} 
	}
}
#endif

#int_rda
void serial_isr_gnss(void) {
	static int1 state=GNSS_STATE_WAITING; 
	static int8 buff[6];
	int8 c;

	c=fgetc(SERIAL_GNSS);

	/* still haven't processed our last message */
	if ( 1==action.now_gnss_trigger_done ) {
		return;
	}


	/* don't insert if our buffer is full */
	if ( nmea_raw.pos == (sizeof(nmea_raw.buff)- 2) ) {
		return;
	}

	/* skip adding carriage returns or line feeds */
	if ( '\r' != c && '\n' != c ) {
		/* add to our buffer */
		nmea_raw.buff[nmea_raw.pos]=c;
		nmea_raw.pos++;
		nmea_raw.buff[nmea_raw.pos]='\0';
	}

	/* capturing data but looking for our trigger so we can set flag */
	if ( GNSS_STATE_WAITING == state ) {
		/* FIFO */
		buff[0]=buff[1];            // '$'
		buff[1]=buff[2];            // 'G'
		buff[2]=buff[3];            // 'P'
		buff[3]=buff[4];            // 'R' or 'H'
		buff[4]=buff[5];            // 'M' or 'D'
		buff[5]=c; 					// 'C' or 'T'
	
		if ( '$' == buff[0] && NMEA0183_TRIGGER[0] == buff[1] && 
	 	                       NMEA0183_TRIGGER[1] == buff[2] &&
		                       NMEA0183_TRIGGER[2] == buff[3] &&
		                       NMEA0183_TRIGGER[3] == buff[4] &&
	 	                       NMEA0183_TRIGGER[4] == buff[5] ) {
			/* matched our 5 character trigger sequence */
			action.now_strobe_counters=1;
			nmea_raw.triggered_age=0;
		}
	} else {
		/* in our trigger sentence. Look for '\r' or '\n' that ends it */
		if ( '\r' == c || '\n' == c ) {
			action.now_gnss_trigger_done=1;
		}
	}

}

#if 0
void serial_isr_gnss(void) {
	static int1 gnss_state=0; 
	static int8 index=0;
	static int8 pos;
	static int8 buff[6];
	int8 c;


	if ( GNSS_STATE_WAITING == gnss_state ) {
		/* FIFO */
		buff[0]=buff[1];            // '$'
		buff[1]=buff[2];            // 'G'
		buff[2]=buff[3];            // 'P'
		buff[3]=buff[4];            // 'R' or 'H'
		buff[4]=buff[5];            // 'M' or 'D'
		buff[5]=fgetc(SERIAL_GNSS); // 'C' or 'T'


		if ( '$' != buff[0] ) {
			return;
		}

		/* search through current.nmea_sentence[].prefixes for a match */
		for ( index=0 ; index<NMEA0183_N_SENTENCE ; index++ ) {
			if (  current.nmea_sentence[index].prefix[0] == buff[1] && current.nmea_sentence[index].prefix[1] == buff[2] &&	current.nmea_sentence[index].prefix[2] == buff[3] && current.nmea_sentence[index].prefix[3] == buff[4] && current.nmea_sentence[index].prefix[4] == buff[5] ) {
					/* got a prefix match! */
					current.nmea_sentence[index].age=0;
			
					output_toggle(LED_RED);

					/* skip '$' since it isn't used in the checksum */
					current.nmea_sentence[index].data[0]=buff[1];
					current.nmea_sentence[index].data[1]=buff[2];
					current.nmea_sentence[index].data[2]=buff[3];
					current.nmea_sentence[index].data[3]=buff[4];
					current.nmea_sentence[index].data[4]=buff[5];
					current.nmea_sentence[index].data[5]='\0';
					pos=5;

					if ( 0 == index ) {
						/* first element is our trigger */
						//action.now_gnss_trigger_start=1;
						action.now_strobe_counters=1;
					}
				
					gnss_state=GNSS_STATE_IN;

					/* quit searching */
					break;
			}			
		}
	} else {
		/* GNSS state in */
		c=fgetc(SERIAL_GNSS);

		if ( '\r'==c || '\n'==c ) {
			/* got the end of the sentence. If there are any more trailing characters (ie '\r' or '\n' they will
			get skipped because GNSS_STATE_WAITING needs to start with a '$' */
			gnss_state = GNSS_STATE_WAITING;

			if ( 0 == index ) {
				action.now_gnss_trigger_done=1;
			}

			return;
		}

		if ( pos < (NMEA0183_LEN_SENTENCE-2) ) {
			/* add our received character if we have room. If not, sentence was already null
			terminated so we are good to go with truncated sentence */
			current.nmea_sentence[index].data[pos]=c;
			pos++;
			/* always null terminate */
			current.nmea_sentence[index].data[pos]='\0';
		} else {
			gnss_state = GNSS_STATE_WAITING;
		}

	}
}
#endif