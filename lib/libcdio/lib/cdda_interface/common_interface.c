/*
  $Id: common_interface.c,v 1.12 2005/01/27 03:10:06 rocky Exp $

  Copyright (C) 2004, 2005 Rocky Bernstein <rocky@panix.com>
  Copyright (C) 1998, 2002 Monty monty@xiph.org
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/******************************************************************
 *
 * CDROM communication common to all interface methods is done here 
 * (mostly ioctl stuff, but not ioctls specific to the 'cooked'
 * interface) 
 *
 ******************************************************************/

#include <math.h>
#include "common_interface.h"
#include "utils.h"
#include "smallft.h"

#ifdef _XBOX
#include "xtl.h"
#else
#include <windows.h>
#endif


/*! Determine Endian-ness of the CD-drive based on reading data from
  it. Some drives return audio data Big Endian while some (most)
  return data Little Endian. Drives known to return data bigendian are
  SCSI drives from Kodak, Ricoh, HP, Philips, Plasmon, Grundig
  CDR100IPW, and Mitsumi CD-R. ATAPI and MMC drives are little endian.

  rocky: As someone who didn't write the code, I have to say this is
  nothing less than brilliant. An FFT is done both ways and the the
  transform is looked at to see which has data in the FFT (or audible)
  portion. (Or so that's how I understand it.)

  @return 1 if big-endian, 0 if little-endian, -1 if we couldn't
  figure things out or some error.
 */
int 
data_bigendianp(cdrom_drive_t *d)
{
  float lsb_votes=0;
  float msb_votes=0;
  int i,checked;
  int endiancache=d->bigendianp;
  float *a=calloc(1024,sizeof(float));
  float *b=calloc(1024,sizeof(float));
  long readsectors=5;
  int16_t *buff=malloc(readsectors*CDIO_CD_FRAMESIZE_RAW);
  memset(buff, 0, readsectors*CDIO_CD_FRAMESIZE_RAW);

  /* look at the starts of the audio tracks */
  /* if real silence, tool in until some static is found */

  /* Force no swap for now */
  d->bigendianp=-1;
  
  cdmessage(d,"\nAttempting to determine drive endianness from data...");
  d->enable_cdda(d,1);
  for(i=0,checked=0;i<d->tracks;i++){
    float lsb_energy=0;
    float msb_energy=0;
    if(cdda_track_audiop(d,i+1)==1){
      long firstsector=cdda_track_firstsector(d,i+1);
      long lastsector=cdda_track_lastsector(d,i+1);
      int zeroflag=-1;
      long beginsec=0;
      
      /* find a block with nonzero data */
      
      while(firstsector+readsectors<=lastsector){
	int j;
	
	if(d->read_audio(d,buff,firstsector,readsectors)>0){
	  
	  /* Avoid scanning through jitter at the edges */
	  for(beginsec=0;beginsec<readsectors;beginsec++){
	    int offset=beginsec*CDIO_CD_FRAMESIZE_RAW/2;
	    /* Search *half* */
	    for(j=460;j<128+460;j++)
	      if(buff[offset+j]!=0){
		zeroflag=0;
		break;
	      }
	    if(!zeroflag)break;
	  }
	  if(!zeroflag)break;
	  firstsector+=readsectors;
	}else{
	  d->enable_cdda(d,0);
	  free(a);
	  free(b);
	  free(buff);
	  return(-1);
	}
      }

      beginsec*=CDIO_CD_FRAMESIZE_RAW/2;
      
      /* un-interleave for an FFT */
      if(!zeroflag){
	int j;
	
	for(j=0;j<128;j++)
	  a[j] = le16_to_cpu(buff[j*2+beginsec+460]);
	for(j=0;j<128;j++)
	  b[j] = le16_to_cpu(buff[j*2+beginsec+461]);

	fft_forward(128,a,NULL,NULL);
	fft_forward(128,b,NULL,NULL);

	for(j=0;j<128;j++)
	  lsb_energy+=fabs(a[j])+fabs(b[j]);
	
	for(j=0;j<128;j++)
	  a[j] = be16_to_cpu(buff[j*2+beginsec+460]);

	for(j=0;j<128;j++)
	  b[j] = be16_to_cpu(buff[j*2+beginsec+461]);

	fft_forward(128,a,NULL,NULL);
	fft_forward(128,b,NULL,NULL);

	for(j=0;j<128;j++)
	  msb_energy+=fabs(a[j])+fabs(b[j]);
      }
    }
    if(lsb_energy<msb_energy){
      lsb_votes+=msb_energy/lsb_energy;
      checked++;
    }else
      if(lsb_energy>msb_energy){
	msb_votes+=lsb_energy/msb_energy;
	checked++;
      }

    if(checked==5 && (lsb_votes==0 || msb_votes==0))break;
    cdmessage(d,".");
  }

  free(buff);
  free(a);
  free(b);
  d->bigendianp=endiancache;
  d->enable_cdda(d,0);

  /* How did we vote?  Be potentially noisy */
  if (lsb_votes>msb_votes) {
    char buffer[256];
    cdmessage(d,"\n\tData appears to be coming back Little Endian.\n");
    sprintf(buffer,"\tcertainty: %d%%\n",(int)
	    (100.*lsb_votes/(lsb_votes+msb_votes)+.5));
    cdmessage(d,buffer);
    return(0);
  } else {
    if(msb_votes>lsb_votes){
      char buffer[256];
      cdmessage(d,"\n\tData appears to be coming back Big Endian.\n");
      sprintf(buffer,"\tcertainty: %d%%\n",(int)
	      (100.*msb_votes/(lsb_votes+msb_votes)+.5));
      cdmessage(d,buffer);
      return(1);
    }

    cdmessage(d,"\n\tCannot determine CDROM drive endianness.\n");
    return(bigendianp());
    return(-1);
  }
}

/************************************************************************/

/*! Here we fix up a couple of things that will never happen.  yeah,
   right.  

   rocky OMITTED FOR NOW:
   The multisession stuff is from Hannu's code; it assumes it knows
   the leadout/leadin size.

   @return -1 if we can't get multisession info, 0 if there is one
   session only or the multi-session LBA is less than or 100 (don't
   ask me why -- I don't know), and 1 if the multi-session lba is
   greater than 100.
*/
int 
FixupTOC(cdrom_drive_t *d, track_t i_tracks)
{
  int j;
  
  /* First off, make sure the 'starting sector' is >=0 */
  
  for( j=0; j<i_tracks; j++){
    if (d->disc_toc[j].dwStartSector<0 ) {
      cdmessage(d,"\n\tTOC entry claims a negative start offset: massaging"
		".\n");
      d->disc_toc[j].dwStartSector=0;
    }
    if( j<i_tracks-1 && d->disc_toc[j].dwStartSector>
       d->disc_toc[j+1].dwStartSector ) {
      cdmessage(d,"\n\tTOC entry claims an overly large start offset: massaging"
		".\n");
      d->disc_toc[j].dwStartSector=0;
    }

  }
  /* Make sure the listed 'starting sectors' are actually increasing.
     Flag things that are blatant/stupid/wrong */
  {
    lsn_t last=d->disc_toc[0].dwStartSector;
    for ( j=1; j<i_tracks; j++){
      if ( d->disc_toc[j].dwStartSector<last ) {
	cdmessage(d,"\n\tTOC entries claim non-increasing offsets: massaging"
		  ".\n");
	 d->disc_toc[j].dwStartSector=last;
	
      }
      last=d->disc_toc[j].dwStartSector;
    }
  }

#if LOOKED_OVER
  /* For a scsi device, the ioctl must go to the specialized SCSI
     CDROM device, not the generic device. */

  if (d->ioctl_fd != -1) {
    struct cdrom_multisession ms_str;
    int result;

    ms_str.addr_format = CDROM_LBA;
    result = ioctl(d->ioctl_fd, CDROMMULTISESSION, &ms_str);
    if (result == -1) return -1;

    if (ms_str.addr.lba > 100) {

      /* This is an odd little piece of code --Monty */

      /* believe the multisession offset :-) */
      /* adjust end of last audio track to be in the first session */
      for (j = i_tracks-1; j >= 0; j--) {
	if (j > 0 && !IS_AUDIO(d,j) && IS_AUDIO(d,j-1)) {
	  if ((d->disc_toc[j].dwStartSector > ms_str.addr.lba - 11400) &&
	      (ms_str.addr.lba - 11400 > d->disc_toc[j-1].dwStartSector))
	    d->disc_toc[j].dwStartSector = ms_str.addr.lba - 11400;
	  break;
	}
      }
      return 1;
    }
  }
#endif

  return 0;
}


