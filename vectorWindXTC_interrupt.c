#int_timer2
void isr_100us(void) {
	if ( 0xffff != timers.pulse_period ) 
		timers.pulse_period++;
}

#int_ext
/* high resolution pulse timer / counter triggered on falling edge */
void isr_ext(void) {
	static short state=0;
	
	current.pulse_count++;

	if ( 1 == state ) {
		/* currently counting, time to finish */
		current.pulse_period=timers.pulse_period;
		if ( current.pulse_period < current.pulse_min_period ) {
			current.pulse_min_period=current.pulse_period;
		}
		state=0;
	}

	if ( 0 == state ) {
		/* not counting, time to start */
		timers.pulse_period=0;
		state=1;
	}
}

#int_timer3
void isr_10ms(void) {
	static int16 uptimeticks=0;
	static int16 adcTicks=0;
	static int16 ticks=0;
//	static int8 ledTicks=0;
	static int16 telem_count;

	/* preset so we trigger again in 10 milliseconds */
	set_timer3(45536);

	/* seconds since last query */
	if ( current.interval_milliseconds < 65535 ) {
		current.interval_milliseconds++;
	}


	/* seconds */
	ticks++;
	if ( ticks >= 100 ) {
		ticks=0;

		telem_count++;
		if ( telem_count >= config.seconds_telem ) {
			telem_count=0;
			timers.now_telem=1;
		}

		if ( timers.count_load_on_seconds && timers.load_on_seconds < 65535 ) {
			timers.load_on_seconds++;
		}
		if ( timers.count_load_on_delay_seconds && timers.load_on_delay_seconds < 65535 ) {
			timers.load_on_delay_seconds++;
		}
		if ( timers.count_load_off_delay_seconds && timers.load_off_delay_seconds < 65535 ) {
			timers.load_off_delay_seconds++;
		}

		/* dawn dusk mode timers */
		if ( timers.evening_countdown_seconds != 65535 && timers.evening_countdown_seconds > 0 )
			timers.evening_countdown_seconds--;
		if ( timers.morning_countdown_to_seconds != 65535 && timers.morning_countdown_to_seconds > 0 ) 
			timers.morning_countdown_to_seconds--;
		if ( timers.morning_countdown_seconds != 65535 && timers.morning_countdown_seconds > 0 ) 
			timers.morning_countdown_seconds--;
	}


	/* uptime counter */
	uptimeTicks++;
	if ( 6000 == uptimeTicks ) {
		uptimeTicks=0;
		if ( current.uptime_minutes < 65535 ) 
			current.uptime_minutes++;
	}

	/* ADC sample counter */
	if ( timers.now_adc_reset_count ) {
		timers.now_adc_reset_count=0;
		adcTicks=0;
	}

	adcTicks++;
	if ( adcTicks == config.adc_sample_ticks ) {
		adcTicks=0;
		timers.now_adc_sample=1;
	}

	/* LEDs */
	if ( 0==timers.led_on_green ) {
		output_low(LED_GREEN);
	} else {
		output_high(LED_GREEN);
		timers.led_on_green--;
	}

	if ( 0==timers.led_on_red ) {
		output_low(LED_RED);
	} else {
		output_high(LED_RED);
		timers.led_on_red--;
	}


	if ( timers.xbee_wakeup < 255 )
		timers.xbee_wakeup++;

	if ( timers.xbee_shutdown > 0 ) {
		timers.xbee_shutdown--;
	} else {
		output_high(XBEE_SLEEP);
	}
}
