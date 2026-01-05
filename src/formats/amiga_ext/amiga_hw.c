/*
//
// Copyright (C) 2009-2016 Jean-Fran�ois DEL NERO
//
// This file is part of the HxCFloppyEmulator file selector.
//
// HxCFloppyEmulator file selector may be used and distributed without restriction
// provided that this copyright statement is not removed from the file and that any
// derivative work contains the original copyright notice and the associated
// disclaimer.
//
// HxCFloppyEmulator file selector is free software; you can redistribute it
// and/or modify  it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// HxCFloppyEmulator file selector is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with HxCFloppyEmulator file selector; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
*/

#include <devices/trackdisk.h>

#include <dos/dos.h>

#include <exec/types.h>

#include <libraries/commodities.h>

#include <proto/commodities.h>
#include <proto/exec.h>

#include <intuition/screens.h>
#include <intuition/preferences.h>
#include <stdio.h>
#include <string.h>

#include "amiga_hw.h"
#include "amiga_regs.h"

#include "crc.h"

//#define DBGMODE 1

extern volatile unsigned short io_floppy_timeout;

static unsigned char CIABPRB_DSKSEL;

static const unsigned char MFM_short_tab[]=
{
	0xAA,0xA9,0xA4,0xA5,0x92,0x91,0x94,0x95,
	0x4A,0x49,0x44,0x45,0x52,0x51,0x54,0x55
};

static const unsigned short MFM_tab[]=
{
	0xAAAA,0xAAA9,0xAAA4,0xAAA5,0xAA92,0xAA91,0xAA94,0xAA95,
	0xAA4A,0xAA49,0xAA44,0xAA45,0xAA52,0xAA51,0xAA54,0xAA55,
	0xA92A,0xA929,0xA924,0xA925,0xA912,0xA911,0xA914,0xA915,
	0xA94A,0xA949,0xA944,0xA945,0xA952,0xA951,0xA954,0xA955,
	0xA4AA,0xA4A9,0xA4A4,0xA4A5,0xA492,0xA491,0xA494,0xA495,
	0xA44A,0xA449,0xA444,0xA445,0xA452,0xA451,0xA454,0xA455,
	0xA52A,0xA529,0xA524,0xA525,0xA512,0xA511,0xA514,0xA515,
	0xA54A,0xA549,0xA544,0xA545,0xA552,0xA551,0xA554,0xA555,
	0x92AA,0x92A9,0x92A4,0x92A5,0x9292,0x9291,0x9294,0x9295,
	0x924A,0x9249,0x9244,0x9245,0x9252,0x9251,0x9254,0x9255,
	0x912A,0x9129,0x9124,0x9125,0x9112,0x9111,0x9114,0x9115,
	0x914A,0x9149,0x9144,0x9145,0x9152,0x9151,0x9154,0x9155,
	0x94AA,0x94A9,0x94A4,0x94A5,0x9492,0x9491,0x9494,0x9495,
	0x944A,0x9449,0x9444,0x9445,0x9452,0x9451,0x9454,0x9455,
	0x952A,0x9529,0x9524,0x9525,0x9512,0x9511,0x9514,0x9515,
	0x954A,0x9549,0x9544,0x9545,0x9552,0x9551,0x9554,0x9555,
	0x4AAA,0x4AA9,0x4AA4,0x4AA5,0x4A92,0x4A91,0x4A94,0x4A95,
	0x4A4A,0x4A49,0x4A44,0x4A45,0x4A52,0x4A51,0x4A54,0x4A55,
	0x492A,0x4929,0x4924,0x4925,0x4912,0x4911,0x4914,0x4915,
	0x494A,0x4949,0x4944,0x4945,0x4952,0x4951,0x4954,0x4955,
	0x44AA,0x44A9,0x44A4,0x44A5,0x4492,0x4491,0x4494,0x4495,
	0x444A,0x4449,0x4444,0x4445,0x4452,0x4451,0x4454,0x4455,
	0x452A,0x4529,0x4524,0x4525,0x4512,0x4511,0x4514,0x4515,
	0x454A,0x4549,0x4544,0x4545,0x4552,0x4551,0x4554,0x4555,
	0x52AA,0x52A9,0x52A4,0x52A5,0x5292,0x5291,0x5294,0x5295,
	0x524A,0x5249,0x5244,0x5245,0x5252,0x5251,0x5254,0x5255,
	0x512A,0x5129,0x5124,0x5125,0x5112,0x5111,0x5114,0x5115,
	0x514A,0x5149,0x5144,0x5145,0x5152,0x5151,0x5154,0x5155,
	0x54AA,0x54A9,0x54A4,0x54A5,0x5492,0x5491,0x5494,0x5495,
	0x544A,0x5449,0x5444,0x5445,0x5452,0x5451,0x5454,0x5455,
	0x552A,0x5529,0x5524,0x5525,0x5512,0x5511,0x5514,0x5515,
	0x554A,0x5549,0x5544,0x5545,0x5552,0x5551,0x5554,0x5555
};

static unsigned char * mfmtobinLUT_L = NULL;
static unsigned char * mfmtobinLUT_H = NULL;
#define MFMTOBINLUT_SIZE 256

#define MFMTOBIN(W) ( mfmtobinLUT_H[W>>8] | mfmtobinLUT_L[W&0xFF] )

static unsigned short * track_buffer = NULL;
#define TRACK_BUFFER_SIZE 24*1024
static unsigned short * track_buffer_wr = NULL;
#define TRACK_BUFFER_WR_SIZE 2*1024

static unsigned char validcache;

unsigned short sector_pos[16];

void waitus(int centus)
{
	int cnt;
	unsigned short time;

	WRITEREG_B(CIAB_CRA, (READREG_B(CIAB_CRA)&0xC0) | 0x08 );
	WRITEREG_B(CIAB_ICR, 0x7F );

	time = 0x48 * centus;
	WRITEREG_B(CIAB_TALO, time&0xFF );
	WRITEREG_B(CIAB_TAHI, time>>8 );

	WRITEREG_B(CIAB_CRA, READREG_B(CIAB_CRA) | 0x01 );

	do
	{
	}while(!(READREG_B(CIAB_ICR)&1));

}

void waitms(int ms)
{
	int cnt;

	WRITEREG_B(CIAB_CRA, (READREG_B(CIAB_CRA)&0xC0) | 0x08 );
	WRITEREG_B(CIAB_ICR, 0x7F );

	WRITEREG_B(CIAB_TALO, 0xCC );
	WRITEREG_B(CIAB_TAHI, 0x02 );

	WRITEREG_B(CIAB_CRA, READREG_B(CIAB_CRA) | 0x01 );
	for(cnt=0;cnt<ms;cnt++)
	{
		do
		{
		}while(!(READREG_B(CIAB_ICR)&1));

		WRITEREG_B(CIAB_CRA, READREG_B(CIAB_CRA) | 0x01 );
	}
}

void testblink()
{
	for(;;)
	{
		waitms(500);

		WRITEREG_B(CIAAPRA, READREG_B(CIAAPRA) ^  0x02 );
	}
}

int jumptotrack(unsigned char t)
{
	unsigned short i,j,k;

	Forbid();
	WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL ));

#ifdef DBGMODE
	printf("-- jumptotrack %d --\n",t);
#endif
	waitms(100);

#ifdef DBGMODE
	printf("-- jumptotrack %d - seek track 0... --\n",t);
#endif
	k = 0;
	while((READREG_B(CIAAPRA) & CIAAPRA_DSKTRACK0) && k<1024)
	{
		WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL  | CIABPRB_DSKSTEP));
		waitms(1);
		WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL ) );
		waitms(1);

		k++;
	}

	if(k < 1024)
	{
	#ifdef DBGMODE
		printf("-- jumptotrack %d - track 0 found --\n",t);
	#endif

		for(j=0;j<t;j++)
		{
			WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL | CIABPRB_DSKDIREC |CIABPRB_DSKSTEP) );
			waitms(1);
			WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL | CIABPRB_DSKDIREC ) );
			waitms(1);
		}

		WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL ) );
	#ifdef DBGMODE
		printf("-- jumptotrack %d - jump done    --\n",t);
	#endif

		Permit();

		return 0;
	}

	#ifdef DBGMODE
		printf("-- jumptotrack %d - track 0 not found!! --\n",t);
	#endif

	Permit();
	return 1;
};

int test_drive(int drive)
{
	int t,j,c;
	Forbid();
	WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) ));

	waitms(100);

	// Jump to Track 0 ("Slow")
	t = 0;
	while((READREG_B(CIAAPRA) & CIAAPRA_DSKTRACK0) && (t<260))
	{
		WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3))  | CIABPRB_DSKSTEP));
		waitus(10);
		WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) ) );
		waitus(80);

		t++;
	}

	if(t<260)
	{
		c = 0;
		do
		{
			// Jump to Track 30 (Fast)
			for(j=0;j<40;j++)
			{
				WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) | CIABPRB_DSKDIREC |CIABPRB_DSKSTEP) );
				waitus(8);
				WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) | CIABPRB_DSKDIREC ) );
				waitus(8);
			}

			waitus(200);

			// And go back to Track 30 (Slow)
			t = 0;
			while((READREG_B(CIAAPRA) & CIAAPRA_DSKTRACK0) && (t<40))
			{
				WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3))  | CIABPRB_DSKSTEP));
				waitus(10);
				WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) ) );
				waitus(80);

				t++;
			}

			c++;
		}while( (t != 40) && c < 2 );

		if(t == 40)
		{
			WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) ) );

			Permit();

			return 1;
		}
	}

	WRITEREG_B(CIABPRB, ~(CIABPRB_DSKMOTOR | (CIABPRB_DSKSEL0<<(drive&3)) ) );

	Permit();

	return 0;
}

int waitindex()
{
	int timeout;

	timeout = 0;

	io_floppy_timeout = 0;

	do{
		asm("nop");
	}while( (!(READREG_B(CIAB_ICR)&0x10)) && ( io_floppy_timeout < 0x200 ) );

	do
	{
		asm("nop");
	}while( (READREG_B(CIAB_ICR)&0x10) && ( io_floppy_timeout < 0x200 ) );

	do{
		asm("nop");
	}while((!(READREG_B(CIAB_ICR)&0x10)) && ( io_floppy_timeout < 0x200 ) );

	if(!(io_floppy_timeout < 0x200 ))
	{
		timeout = 1;
	}

	return timeout;
}

int readtrack(unsigned short * track,unsigned short size,unsigned char waiti)
{
	WRITEREG_B(CIABPRB,~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL));
	WRITEREG_W( DMACON,0x8210);

	WRITEREG_W(INTREQ,0x0002);

	// set dsklen to 0x4000
	WRITEREG_W( DSKLEN ,0x4000);

	WRITEREG_L( DSKPTH ,track);

	WRITEREG_W( ADKCON, 0x7F00);
	WRITEREG_W( ADKCON, 0x9500); //9500
	WRITEREG_W( DMACON, 0x8210);
	WRITEREG_W( DSKSYNC,0x4489);
	WRITEREG_W( INTREQ, 0x0002);

	if(waiti)
	{
		if(waitindex())
		{
			printf("ERROR: READ - No Index Timeout ! (state %d)\n",(READREG_B(CIAB_ICR)&0x10)>>4);
			exit(1);
		}
	}

	//Put the value you want into the DSKLEN register
	WRITEREG_W( DSKLEN ,size | 0x8000);
	//Write this value again into the DSKLEN register. This actually starts the DMA.
	WRITEREG_W( DSKLEN ,size | 0x8000);

	while(!(READREG_W(INTREQR)&0x0002));
	WRITEREG_W( DSKLEN ,0x4000);
	WRITEREG_W(INTREQ,0x0002);

	validcache=1;

	return 1;

}

int writetrack(unsigned short * track,unsigned short size,unsigned char waiti)
{

//	while(!(READREG_W(INTREQR)&0x0002));

	WRITEREG_B(CIABPRB,~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL));
	WRITEREG_W( DMACON,0x8210);

	WRITEREG_W(INTREQ,0x0002);

	// set dsklen to 0x4000
	WRITEREG_W( DSKLEN ,0x4000);

	WRITEREG_L( DSKPTH ,track);

	WRITEREG_W( ADKCON, 0x7F00);
	WRITEREG_W( ADKCON, 0xB100); //9500
	WRITEREG_W( DMACON, 0x8210);
	WRITEREG_W( DSKSYNC,0x4489);
	WRITEREG_W( INTREQ, 0x0002);

	if(waiti)
	{
		io_floppy_timeout = 0;
		while( READREG_B(CIAB_ICR)&0x10 && ( io_floppy_timeout < 0x200 ) );
		while( !(READREG_B(CIAB_ICR)&0x10) && ( io_floppy_timeout < 0x200 ) );
		if(!( io_floppy_timeout < 0x200 ))
		{
			printf("ERROR: WRITE - No Index Timeout ! (state %d)\n",(READREG_B(CIAB_ICR)&0x10)>>4);
			exit(1);
		}
	}

	//Put the value you want into the DSKLEN register
	WRITEREG_W( DSKLEN ,size | 0x8000 | 0x4000 );
	//Write this value again into the DSKLEN register. This actually starts the DMA.
	WRITEREG_W( DSKLEN ,size | 0x8000 | 0x4000 );

	while(!(READREG_W(INTREQR)&0x0002));
	WRITEREG_W( DSKLEN ,0x4000);
	WRITEREG_W(INTREQ,0x0002);

	validcache=0;

	return 1;
}

// Fast Bin to MFM converter
int BuildCylinder(unsigned char * mfm_buffer,int mfm_size,unsigned char * track_data,int track_size,unsigned short lastbit,unsigned short * retlastbit)
{
	int i,l;
	unsigned char byte;
	unsigned short mfm_code;

	if(track_size*2>mfm_size)
	{
		track_size=mfm_size/2;
	}

	// MFM Encoding
	i=0;
	for(l=0;l<track_size;l++)
	{
		byte =track_data[l];

		mfm_code = MFM_tab[byte] & lastbit;

		mfm_buffer[i++]=mfm_code>>8;
		mfm_buffer[i++]=mfm_code&0xFF;

		lastbit=~(MFM_tab[byte]<<15);
	}

	if(retlastbit)
		*retlastbit = lastbit;

	return track_size;
}

unsigned char writesector(unsigned char sectornum,unsigned char * data)
{
	unsigned short i,j,len,retry,retry2,lastbit;
	unsigned char sectorfound;
	unsigned char c;
	unsigned char CRC16_High,CRC16_Low,byte;

	Forbid();

	retry2=2;

	i=0;
	validcache=0;

	CRC16_Init(&CRC16_High, &CRC16_Low);
	for(j=0;j<3;j++)
	{
		CRC16_Update(&CRC16_High,&CRC16_Low,0xA1);
	}

	CRC16_Update(&CRC16_High,&CRC16_Low,0xFB);

	for(j=0;j<512;j++)
	{
		CRC16_Update(&CRC16_High,&CRC16_Low,data[j]);
	}

	for(j=0;j<12;j++)
		track_buffer_wr[i++]=0xAAAA;

	track_buffer_wr[i++]=0x4489;
	track_buffer_wr[i++]=0x4489;
	track_buffer_wr[i++]=0x4489;
	lastbit = 0x7FFF;
	byte = 0xFB;
	BuildCylinder((unsigned char*)&track_buffer_wr[i++],1*2,&byte,1,lastbit,&lastbit);
	BuildCylinder((unsigned char*)&track_buffer_wr[i],512*2,data,512,lastbit,&lastbit);
	i=i+512;
	BuildCylinder((unsigned char*)&track_buffer_wr[i++],1*2,&CRC16_High,1,lastbit,&lastbit);
	BuildCylinder((unsigned char*)&track_buffer_wr[i++],1*2,&CRC16_Low,1,lastbit,&lastbit);
	byte = 0x4E;
	for(j=0;j<4;j++)
	{
		BuildCylinder((unsigned char*)&track_buffer_wr[i++],1*2,&byte,1,lastbit,&lastbit);
	}

	len=i;

	sectorfound=0;
	retry=30;

	if(sectornum)
	{
		do
		{

			do
			{

				i=0;

				retry--;

				if(!readtrack(track_buffer,16,0))
				{
					Permit();
					return 0;
				}

				while(track_buffer[i]==0x4489 && (i<16))
				{
					i++;
				}

				if(MFMTOBIN(track_buffer[i])==0xFE && (i<(16-3)))
				{

					CRC16_Init(&CRC16_High, &CRC16_Low);

					for(j=0;j<3;j++)
						CRC16_Update(&CRC16_High,&CRC16_Low,0xA1);

					for(j=0;j<(1+4+2);j++)
					{
						c = MFMTOBIN(track_buffer[i+j]);
						CRC16_Update(&CRC16_High, &CRC16_Low,c);
					}

					if(!CRC16_High && !CRC16_Low)
					{
						i++;
						if(MFMTOBIN(track_buffer[i])==0xFF) //track
						{
							i++;
							if(MFMTOBIN(track_buffer[i])==0x00) //side
							{
								i++;
								if(MFMTOBIN(track_buffer[i])==sectornum) //sector
								{
									sectorfound=1;
									if(!writetrack(track_buffer_wr,len,0))
									{
										Permit();
										return 0;
									}
								}
							}
						}
					}
				}
			}while(!sectorfound  && retry);

			if(!sectorfound)
			{
				if(jumptotrack(255))
				{
					printf("ERROR: writesector -> failure while seeking the track 00!\n");
				}
				retry=30;
			}
			retry2--;

		}while(!sectorfound && retry2);

	}
	else
	{
		sectorfound=1;

        if(!writetrack(track_buffer_wr,len,1))
		{
			Permit();
			return 0;
		}

	}

	Permit();
	return sectorfound;
}

unsigned char readsector(unsigned char sectornum,unsigned char * data,unsigned char invalidate_cache)
{
	unsigned short i,j,t;
	unsigned char sectorfound,tc;
	unsigned char c,retry,badcrc,retry2;
	unsigned char CRC16_High,CRC16_Low;

	Forbid();
	retry2=2;
	retry=5;

	do
	{

		do
		{
			sectorfound=0;
			i=0;
			badcrc=0;
			if(!validcache || invalidate_cache)
			{
				if(!readtrack(track_buffer,10*1024,0))
				{
					Permit();
					return 0;
				}

				i=1;
				for(j=0;j<9;j++)
				{
					sector_pos[j]=0xFFFF;
				}

				for(j=0;j<9;j++)
				{
					while(track_buffer[i]!=0x4489 && i) i=(i+1)&0x3FFF;
					if(!i) j=9;
					while(track_buffer[i]==0x4489 && i) i=(i+1)&0x3FFF;
					if(!i) j=9;
					if(MFMTOBIN(track_buffer[i])==0xFE)
					{
						sector_pos[MFMTOBIN(track_buffer[i+3])&0xF]=i;
						i=(i+512+2)&0x3FFF;
					}
					else
					{
						i++;
					}
				}
			}

			do
			{
				i=sector_pos[sectornum&0xF];
				if(i<16*1024)
				{
					if(MFMTOBIN(track_buffer[i])==0xFE)
					{
						CRC16_Init(&CRC16_High, &CRC16_Low);
						for(j=0;j<3;j++)CRC16_Update(&CRC16_High,&CRC16_Low,0xA1);

						for(j=0;j<(1+4+2);j++)
						{
							c=MFMTOBIN(track_buffer[i+j]);
							CRC16_Update(&CRC16_High, &CRC16_Low,c);
						}

						if(!CRC16_High && !CRC16_Low)
						{
							i++;
							if(MFMTOBIN(track_buffer[i])==0xFF) //track
							{
								i++;
								if(MFMTOBIN(track_buffer[i])==0x00) //side
								{
									i++;
									if(MFMTOBIN(track_buffer[i])==sectornum) //sector
									{
										i=i+41;

										CRC16_Init(&CRC16_High, &CRC16_Low);
										for(j=0;j<3;j++)
											CRC16_Update(&CRC16_High,&CRC16_Low,0xA1);

										CRC16_Update(&CRC16_High,&CRC16_Low,MFMTOBIN(track_buffer[i]));
										i++;

										for(j=0;j<512;j++)
										{

											tc = MFMTOBIN(track_buffer[i]);

											//CRC16_Update(&CRC16_High, &CRC16_Low,tc);
											i++;
											data[j]=tc;
										}

										for(j=0;j<2;j++)
										{
											c = MFMTOBIN(track_buffer[i++]);
											//CRC16_Update(&CRC16_High, &CRC16_Low,c);
										}

										if(1)//!CRC16_High && !CRC16_Low)
										{
											sectorfound=1;
										}
										else
										{
											badcrc=1;
										}

									}
									else
									{
										i=i+512+2;
									}
								}
								else
								{
									i=i+512+2;
								}
							}
							else
							{
								i=i+512+2;
							}
						}
						else
						{
							i++;
							badcrc=1;
						}
					}
					else
					{
						i++;
					}
				}
				else
				{
					badcrc=1;
				}

			}while( !sectorfound && (i<(16*1024)) && !badcrc);

			retry--;
			if(!sectorfound && retry)
			{
				validcache=0;
			}

		}while(!sectorfound && retry);


		if(!sectorfound)
		{
			if(jumptotrack(255))
			{
				printf("ERROR: readsector -> failure while seeking the track 00!\n");
			}

			retry2--;
			retry=5;
		}


	}while(!sectorfound && retry2);

	if(!sectorfound)
	{
		validcache=0;
	}

	Permit();

	return sectorfound;
}

void alloc_error()
{
	printf("ERROR: Memory Allocation Error -> No more free mem ?\n");
}

void free_mem_amiga_fdc(void)
{
    if(mfmtobinLUT_L!=NULL) FreeMem(mfmtobinLUT_L,MFMTOBINLUT_SIZE);
    if(mfmtobinLUT_H!=NULL) FreeMem(mfmtobinLUT_H,MFMTOBINLUT_SIZE);
    if(track_buffer!=NULL) FreeMem(track_buffer,TRACK_BUFFER_SIZE);
    if(track_buffer_wr!=NULL) FreeMem(track_buffer_wr,TRACK_BUFFER_WR_SIZE);
}

int init_amiga_fdc(unsigned char drive)
{
	unsigned short i;

	if(drive==0)
		CIABPRB_DSKSEL=CIABPRB_DSKSEL0;
	else
		CIABPRB_DSKSEL=CIABPRB_DSKSEL1;

	validcache=0;

	mfmtobinLUT_L=(unsigned char*)AllocMem(MFMTOBINLUT_SIZE,MEMF_CHIP);
	mfmtobinLUT_H=(unsigned char*)AllocMem(MFMTOBINLUT_SIZE,MEMF_CHIP);
	if(mfmtobinLUT_L && mfmtobinLUT_H)
	{
		for(i=0;i<256;i++)
		{
			mfmtobinLUT_L[i] =   ((i&0x40)>>3) | ((i&0x10)>>2) | ((i&0x04)>>1) | (i&0x01);
			mfmtobinLUT_H[i] =   mfmtobinLUT_L[i] << 4;
		}
	}
	else
	{
		alloc_error();
        free_mem_amiga_fdc();
        return 0;
	}

	track_buffer=(unsigned short*)AllocMem(TRACK_BUFFER_SIZE,MEMF_CHIP);
	if(track_buffer)
	{
		memset(track_buffer,0,TRACK_BUFFER_SIZE);
	}
	else
	{
		alloc_error();
        free_mem_amiga_fdc();
        return 0;
	}

	track_buffer_wr=(unsigned short*)AllocMem(TRACK_BUFFER_WR_SIZE,MEMF_CHIP);
	if(track_buffer_wr)
	{
		memset(track_buffer_wr,0,TRACK_BUFFER_WR_SIZE);
	}
	else
	{
		alloc_error();
        free_mem_amiga_fdc();
        return 0;
	}

	Forbid();

	WRITEREG_B(CIABPRB,~(CIABPRB_DSKMOTOR | CIABPRB_DSKSEL));
	WRITEREG_W( DMACON,0x8210);

	if(jumptotrack(255))
	{
		printf("ERROR: init_amiga_fdc -> failure while seeking the track to 255!\n");
        free_mem_amiga_fdc();
        return 0;
	}
	Delay(12);
	WRITEREG_W(INTREQ,0x0002);

	Permit();

    return 1;
}

void shutdown_amiga_fdc(void)
{
    if(jumptotrack(0))
	{
		printf("ERROR: shutdown_amiga_fdc -> failure while seeking the track to 0!\n");
	}

    free_mem_amiga_fdc();
}
