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
#include "c328.h"
#include "rs8.h"

/* Message buffer */
#define MSG_SIZE (100)
char msg[MSG_SIZE];

/* Image TX data */
#define PKT_SIZE         (0x100)
#define PKT_SIZE_HEADER  (0x06)
#define PKT_SIZE_RSCODES (0x20)
#define PKT_SIZE_PAYLOAD (PKT_SIZE - PKT_SIZE_HEADER - PKT_SIZE_RSCODES)
uint8_t pkt[PKT_SIZE];
uint16_t pkt_len;
uint16_t image_len;

void init_packet(uint8_t *packet, uint8_t imageid, uint8_t pktid, uint16_t filesize)
{
	packet[0] = 0x55;                 /* Preamble      */
	packet[1] = 0x66;                 /* Marker        */
	packet[2] = imageid;              /* Image ID      */
	packet[3] = pktid;                /* Packet ID     */
	packet[4] = filesize & 0XFF;      /* Filesize LSB  */
	packet[5] = filesize >> 8;        /* Filesize MSB  */
	memset(&packet[PKT_SIZE_HEADER], 0, PKT_SIZE_PAYLOAD);
}

char setup_camera(void)
{
	if(c3_sync() != 0)
	{
		rtx_string_P(PSTR("$$" CALLSIGN ":Camera sync failed...\n"));
		return(-1);
	}
	
	/* Setup the camera */
	if(c3_setup(CT_JPEG, 0, SR_320x240) != 0)
	{
		rtx_string_P(PSTR("$$" CALLSIGN ":Camera setup failed...\n"));
		return(-1);
	}
	
	/* Set the package size */
	if(c3_set_package_size(PKT_SIZE_PAYLOAD + 6) != 0)
	{
		rtx_string_P(PSTR("$$" CALLSIGN ":Package size set failed!\n"));
		return(-1);
	}
	
	/* Take the image */
	if(c3_snapshot(ST_JPEG, 0) != 0)
	{
		rtx_string_P(PSTR("$$" CALLSIGN ":Snapshot failed!\n"));
		return(-1);
	}
	
	/* Get the image size and begin the transfer */
	if(c3_get_picture(PT_SNAPSHOT, &image_len) != 0)
	{
		rtx_string_P(PSTR("$$" CALLSIGN ":Get picture failed\n"));
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
	static uint8_t img_tx;
	static uint8_t pkt_id;
	
	if(!setup)
	{
		if(setup_camera() != 0) return(-1);
		setup = -1;
		
		pkt_id = 0;
		pkg_len = 0;
		pkg_id = 0;
		img_tx = 0;
		img_id++;
	}
	
	/* Initialise the packet -- make sure previous packet has finished TX'ing! */
	init_packet(pkt, img_id, pkt_id++, image_len);
	pkt_len = 0;
	
	while(pkt_len < PKT_SIZE_PAYLOAD)
	{
		if(pkg_len == 0)
		{
			char msg[100];
			if(c3_get_package(pkg_id++, &pkg, &pkg_len) != 0)
			{
				snprintf(msg, 100, "$$" CALLSIGN ",Get package %i failed\n", pkg_id - 1);
				rtx_string(msg);
				rtx_wait();
				
				setup = 0;
				return(-1);
			}
			
			/* Skip the package header */
			pkg += 4;
			pkg_len -= 6;
		}
		
		if(pkg_len > 0)
		{
			uint16_t l = PKT_SIZE_PAYLOAD - pkt_len;
			if(pkg_len < l) l = pkg_len;
			
			memcpy(pkt + PKT_SIZE_HEADER + pkt_len, pkg, l);
			
			pkg += l;
			pkg_len -= l;
			pkt_len += l;
			img_tx += l;
		}
		
		/* Have we reached the end of the image? */
		if(img_tx == image_len)
		{
			c3_finish_picture();
			setup = 0;
			break;
		}
	}
	
	encode_rs_8(&pkt[1], &pkt[PKT_SIZE_HEADER + PKT_SIZE_PAYLOAD], 0);
	rtx_data(pkt, PKT_SIZE);
	
	return(0);
}

int main(void)
{
	/* Initalise the various bits */
	rtx_init();
	c3_init();
	
	/* Let the radio settle before beginning */
	_delay_ms(2000);
	
	/* Start interrupts and enter the main loop */
	sei();
	
	while(1)
	{
		tx_image();
		rtx_wait();
		//rtx_string_P(PSTR("$$" CALLSIGN ",TEST\n"));
	}
	
	return(0);
}

