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

/* Message buffer */
#define MSG_SIZE (100)
char msg[MSG_SIZE];

#define PREFIX "$$$$"

/* Image TX data */
#define PKT_SIZE         (0x100)
#define PKT_SIZE_HEADER  (0x0A)
#define PKT_SIZE_RSCODES (0x20)
#define PKT_SIZE_PAYLOAD (PKT_SIZE - PKT_SIZE_HEADER - PKT_SIZE_RSCODES)
uint8_t pkt[PKT_SIZE];
uint8_t image_pkts;

void init_packet(uint8_t *packet, uint8_t imageid, uint8_t pktid, uint8_t pkts, uint16_t width, uint16_t height)
{
	packet[0] = 0x55;        /* Sync            */
	packet[1] = 0x66;        /* Type            */
	packet[2] = imageid;     /* Image ID        */
	packet[3] = pktid;       /* Packet ID       */
	packet[4] = pkts;        /* Packets         */
	packet[5] = width >> 3;  /* Width MCU       */
	packet[6] = height >> 3; /* Height MCU      */
	packet[7] = 0xFF;        /* Next MCU offset */
	packet[8] = 0x00;        /* MCU ID MSB      */
	packet[9] = 0x00;        /* MCU ID LSB      */
	memset(&packet[PKT_SIZE_HEADER], 0, PKT_SIZE_PAYLOAD);
}

char tx_image(void)
{
	static char setup = 0;
	static uint8_t img_id = 0;
	static uint8_t pkt_id;
	
	if(!setup)
	{
		uint16_t image_len;
		
		if(c3_open(SR_320x240) != 0)
		{
			rtx_string_P(PSTR(PREFIX CALLSIGN ":Camera error\n"));
			return(setup);
		}
		
		setup = -1;
		pkt_id = 0;
		img_id++;
		
		/* Calculate the number of packets needed for this image */
		image_len   = c3_filesize();
		image_pkts  = image_len / PKT_SIZE_PAYLOAD;
		image_pkts += (image_len % PKT_SIZE_PAYLOAD > 0 ? 1 : 0);
	}
	
	/* Initialise the packet */
	init_packet(pkt, img_id, pkt_id++, image_pkts, 320, 240);
	
	if(c3_read(&pkt[PKT_SIZE_HEADER], PKT_SIZE_PAYLOAD) == 0)
	{
		rtx_string_P(PSTR(PREFIX CALLSIGN ":Error reading from camera\n"));
		c3_close();
		setup = 0;
		return(setup);
	}
	
	if(c3_eof())
	{
		c3_close();
		setup = 0;
	}
	
	encode_rs_8(&pkt[1], &pkt[PKT_SIZE_HEADER + PKT_SIZE_PAYLOAD], 0);
	rtx_string_P(PSTR("UUU")); /* U = 0x55 */
	rtx_data(pkt, PKT_SIZE);
	
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
		PREFIX CALLSIGN ",%u,%02i:%02i:%02i,%s%i.%06lu,%s%i.%06lu,%li,%i:%i",
		counter++,
		gps.hour, gps.minute, gps.second,
		(gps.latitude_h == 'S' ? "-" : ""),
		gps.latitude_i, gps.latitude_f,
		(gps.longitude_h == 'W' ? "-" : ""),
		gps.longitude_i, gps.longitude_f,
		gps.altitude, gps.fix, gps.sats);
	
	/* Append the checksum, skipping the first four $'s */
	crccat(msg + 4);
	
	/* Begin transmitting */
	rtx_string(msg);
	
	return(0);
}

int main(void)
{
	char r;
	
	/* Initalise the various bits */
	rtx_init();
	gps_init();
	c3_init();
	
	/* Let the radio settle before beginning */
	_delay_ms(2000);
	
	/* Start interrupts and enter the main loop */
	sei();
	
	while(1)
	{
		r = 10;
		
		if(tx_image() == -1)
		{
			/* The camera goes to sleep while transmitting telemetry,
			 * sync'ing here seems to prevent it. */
			c3_sync();
			r = 1;
		}
		
		rtx_string_P(PSTR("\n"));
		for(; r > 0; r--) tx_telemetry();
	}
	
	return(0);
}

