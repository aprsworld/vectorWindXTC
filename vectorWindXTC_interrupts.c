

#int_timer2 // HIGH
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
}




#int_rda2
void serial_isr_wireless(void) {
	int8 c;

	c=fgetc(SERIAL_XTC);
}

#define GNSS_STATE_WAITING 0
#define GNSS_STATE_IN      1


#define GNSS_SENTENCE_GGA  0
#define GNSS_SENTENCE_HDT  1


#int_rda
void serial_isr_gnss(void) {
	static int1 gnss_state=0; 
	static int8 gnss_sentence_index=0;
	int8 c;
	int8 buff[6];

	if ( GNSS_STATE_WAITING == gnss_state ) {
		buff[0]=buff[1];            // '$'
		buff[1]=buff[2];            // (do not care)
		buff[2]=buff[3];            // (do not care)
		buff[3]=buff[4];            // 'G' or 'H'
		buff[4]=buff[5];            // 'G' or 'D'
		buff[5]=fgetc(SERIAL_GNSS); // 'A' or 'T'

		if ( '$'==buff[0] ) {
			if ( 'G'==buff[3] && 'G'==buff[4] && 'A'==buff[5] ) {
				/* got a $xxGGA sentence start */ 
				gnss_sentence_index=GNSS_SENTENCE_GGA;
				gnss_state=GNSS_STATE_IN;
				action.now_gga_start=1;
			} else if ( 'H'==buff[3] && 'D'==buff[4] && 'T'==buff[5] ) {
				/* got a $xxHDT sentence start */
				gnss_sentence_index=GNSS_SENTENCE_HDT;
				gnss_state=GNSS_STATE_IN;
			}

			if ( GNSS_STATE_IN==gnss_state ) {
				/* skip '$' since it isn't used in the checksum */
				current.gnss_sentence[gnss_sentence_index][0]=buff[1];
				current.gnss_sentence[gnss_sentence_index][1]=buff[2];
				current.gnss_sentence[gnss_sentence_index][2]=buff[3];
				current.gnss_sentence[gnss_sentence_index][3]=buff[4];
				current.gnss_sentence[gnss_sentence_index][4]=buff[5];

				current.gnss_sentence_pos[gnss_sentence_index]=5;

				current.gnss_sentence_age[gnss_sentence_index]=0;
			}
		} else {
			/* don't do anything because NMEA sentences must start with a '$' */
		}
	} else {
		/* GNSS state in */
		c=fgetc(SERIAL_GNSS);

		if ( '\r'==c || '\n'==c ) {
			/* got the end of the sentence. If there are any more trailing characters (ie '\r' or '\n' they will
			get skipped because GNSS_STATE_WAITING needs to start with a '$' */
			gnss_state = GNSS_STATE_WAITING;
			current.gnss_sentence[gnss_sentence_index][current.gnss_sentence_pos]='\0';

			if ( GNSS_SENTENCE_HDT == gnss_sentence_index ) {
				action.now_hdt_done=1;
			}

			return;
		}


		current.gnss_sentence[gnss_sentence_index][current.gnss_sentence_pos]=c;
		current.gnss_sentence_pos[gnss_sentence_index]++;
	}
}
