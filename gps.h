/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

typedef struct {
	
	uint8_t hour;   /* 0-23 */
	uint8_t minute; /* 0-60 */
	uint8_t second; /* 0-60 */
	
	int16_t  latitude_i; /* -180-180 */
	uint32_t latitude_f; /* 0-999999 */
	char     latitude_h; /* Hemisphere */
	
	int16_t  longitude_i; /* -180-180 */
	uint32_t longitude_f; /* 0-999999 */
	char     longitude_h; /* Hemisphere */
	
	int32_t altitude; /* 0-99999 */
	
	uint8_t fix;  /* 0-2  */
	uint8_t sats; /* 0-99 */
	
} gpsfix_t;

extern void gps_init();
extern char gps_parse(gpsfix_t *gps);

