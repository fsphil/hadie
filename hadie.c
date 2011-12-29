/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <avr/interrupt.h>
#include "rtty.h"
#include "gps.h"
#include "c328.h"
#include "rs8.h"
#include "ssdv.h"

/* Message buffer */
#define MSG_SIZE (100)
char msg[MSG_SIZE];

#define PREFIX "$$"

/* Image TX data */
uint8_t pkt[SSDV_PKT_SIZE], img[64];

/* State of the flight */
#define ALT_STEP (200)
static int32_t r_altitude = 0; /* Reference altitude */
static char ascent = 1; /* Direction of travel. 0 = Down, 1 = Up */

char tx_image(void)
{
	static char setup = 0;
	static uint8_t img_id = 0;
	static ssdv_t ssdv;
	int r;
	
	if(!setup)
	{
		/* Don't begin transmitting a new image if the payload is falling */
		if(ascent == 0) return(setup);
		
		if(c3_open(SR_320x240) != 0)
		{
			rtx_string_P(PSTR(PREFIX CALLSIGN ":Camera error\n"));
			return(setup);
		}
		
		setup = -1;
		
		ssdv_enc_init(&ssdv, img_id++);
		ssdv_enc_set_buffer(&ssdv, pkt);
	}
	
	while((r = ssdv_enc_get_packet(&ssdv)) == SSDV_FEED_ME)
	{
		size_t r = c3_read(img, 64);
		if(r == 0) break;
		ssdv_enc_feed(&ssdv, img, r);
	}
	
	if(r != SSDV_OK)
	{
		/* Something went wrong! */
		c3_close();
		setup = 0;
		rtx_string_P(PSTR(PREFIX CALLSIGN ":ssdv_enc_get_packet() failed\n"));
		return(setup);
	}
	
	if(ssdv.state == S_EOI || c3_eof())
	{
		/* The end of the image has been reached */
		c3_close();
		setup = 0;
	}
	
	/* Got the packet! Transmit it */
	rtx_data(pkt, SSDV_PKT_SIZE);
	
	return(setup);
}

uint16_t crccat(char *msg)
{
	uint16_t x;
	for(x = 0xFFFF; *msg; msg++)
		x = _crc_xmodem_update(x, *msg);
	snprintf(msg, 8, "*%04X\n", x);
	return(x);
}

char tx_telemetry(void)
{
	static unsigned int counter = 0;
	gpsfix_t gps;
	
	/* Read the GPS data */
	gps_parse(&gps);
	
	rtx_wait();
	snprintf(msg, MSG_SIZE,
		PREFIX CALLSIGN ",%u,%02i:%02i:%02i,%s%i.%06lu,%s%i.%06lu,%li,%i,%i,?",
		counter++,
		gps.hour, gps.minute, gps.second,
		(gps.latitude_h == 'S' ? "-" : ""),
		gps.latitude_i, gps.latitude_f,
		(gps.longitude_h == 'W' ? "-" : ""),
		gps.longitude_i, gps.longitude_f,
		gps.altitude, gps.fix, gps.sats);
	
	/* Append the checksum, skipping the $$ prefix */
	crccat(msg + 2);
	
	/* Begin transmitting */
	rtx_string(msg);
	
	/* Update the ascent / descent status */
	if(gps.fix > 0)
	{
		int32_t i = ascent;
		
		if(gps.altitude >= r_altitude + ALT_STEP)
		{
			/* Payload is rising */
			ascent = 1;
			r_altitude = gps.altitude;
		}
		else if(gps.altitude <= r_altitude - ALT_STEP)
		{
			/* Payload is falling */
			ascent = 0;
			r_altitude = gps.altitude;
		}
		
		/* If staring to fall above 4000 metres... */
		if(gps.altitude >= 4000 && i == 1 && ascent == 0)
			rtx_string_P(PSTR(PREFIX CALLSIGN ":Geronimo!!!!\n"));
	}
	
	return(0);
}

int main(void)
{
	char r;
	
	/* Initalise the various bits */
	rtx_init();
	gps_init();
	c3_init();
	
	/* Turn on the radio and let it settle before beginning */
	rtx_enable(1);
	_delay_ms(2000);
	
	/* Start interrupts and enter the main loop */
	sei();
	
	while(1)
	{
		if(tx_image() == -1)
		{
			/* The camera goes to sleep while transmitting telemetry,
			 * sync'ing here seems to prevent it. */
			c3_sync();
			rtx_string_P(PSTR("\n"));
			r = 1;
		}
		else
		{
			/* Put UBLOX5 GPS in proper nav mode */
			if(gps_ubx_init() != 0)
				rtx_string_P(PSTR(PREFIX CALLSIGN ":GPS mode set failed\n"));
			
			r = 10;
		}
		
		for(; r > 0; r--) tx_telemetry();
	}
	
	return(0);
}

