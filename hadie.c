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
uint16_t pkt_len;
uint16_t image_len;

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

char setup_camera(void)
{
	if(c3_sync() != 0)
	{
		rtx_string_P(PSTR(PREFIX CALLSIGN ":Camera sync failed...\n"));
		return(-1);
	}
	
	/* Setup the camera */
	if(c3_setup(CT_JPEG, 0, SR_320x240) != 0)
	{
		rtx_string_P(PSTR(PREFIX CALLSIGN ":Camera setup failed...\n"));
		return(-1);
	}
	
	/* Set the package size */
	if(c3_set_package_size(PKT_SIZE_PAYLOAD + 6) != 0)
	{
		rtx_string_P(PSTR(PREFIX CALLSIGN ":Package size set failed!\n"));
		return(-1);
	}
	
	/* Take the image */
	if(c3_snapshot(ST_JPEG, 0) != 0)
	{
		rtx_string_P(PSTR(PREFIX CALLSIGN ":Snapshot failed!\n"));
		return(-1);
	}
	
	/* Get the image size and begin the transfer */
	if(c3_get_picture(PT_SNAPSHOT, &image_len) != 0)
	{
		rtx_string_P(PSTR(PREFIX CALLSIGN ":Get picture failed\n"));
		return(-1);
	}
	
	/* Camera is ready to transfer the image data */
	
	return(0);
}

char tx_image(void)
{
	static char setup = 0;
	
	static uint16_t pkg_id;
	static uint8_t *pkg;
	static uint16_t pkg_len;
	
	static uint8_t img_id = 0;
	static uint16_t img_tx;
	static uint8_t pkt_id;
	
	if(!setup)
	{
		if(setup_camera() != 0) return(setup);
		setup = -1;
		
		pkt_id = 0;
		pkg_len = 0;
		pkg_id = 0;
		img_tx = 0;
		img_id++;
	}
	
	/* Initialise the packet -- make sure previous packet has finished TX'ing! */
	init_packet(pkt, img_id, pkt_id++, 0xFF, 320, 240);
	pkt_len = 0;
	
	while(pkt_len < PKT_SIZE_PAYLOAD)
	{
		if(pkg_len == 0)
		{
			char msg[100];
			char i;
			if((i = c3_get_package(pkg_id++, &pkg, &pkg_len)) != 0)
			{
				snprintf(msg, 100, PREFIX CALLSIGN ",Get package %i failed (%i)\n", pkg_id - 1, i);
				rtx_string(msg);
				rtx_wait();
				
				setup = 0;
				return(setup);
			}
			
			/* Skip the package header */
			pkg += 4;
			pkg_len -= 6;
		}
		
		if(pkg_len > 0)
		{
			uint16_t l = PKT_SIZE_PAYLOAD - pkt_len;
			if(pkg_len < l) l = pkg_len;
			
			/* TODO: Copy with the JPEG filter */
			memcpy(pkt + PKT_SIZE_HEADER + pkt_len, pkg, l);
			
			pkg += l;
			pkg_len -= l;
			pkt_len += l;
			img_tx += l;
		}
		
		/* Have we reached the end of the image? */
		if(img_tx >= image_len)
		{
			c3_finish_picture();
			setup = 0;
			break;
		}
	}
	
	encode_rs_8(&pkt[1], &pkt[PKT_SIZE_HEADER + PKT_SIZE_PAYLOAD], 0);
	rtx_string_P(PSTR("UUU")); /* U = 0x55 */
	rtx_data(pkt, PKT_SIZE);
	//rtx_wait();
	
	//c3_ping();
	
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
		PREFIX CALLSIGN ",%u,%02i:%02i:%02i,%i.%06lu,%i.%06lu,%li,%i:%i",
		counter++,
		gps.hour, gps.minute, gps.second,
		gps.latitude_i, gps.latitude_f,
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

