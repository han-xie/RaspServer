/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: minimad.c,v 1.4 2004/01/23 09:41:32 rob Exp $
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/soundcard.h>
#include "../background.h"

//#include "mad.h"
#include "mp3_play.h"
#include "../Dev/sound.h"


#define DEBUG tty
#if (DEBUG==tty)
#define DP( s, arg... )  printf( "<log mp3>:\t" s , ##arg )
#define DE( s, arg... )  fprintf( stderr , "<err mp3>:\t" s , ##arg )
#else
#define DP( x... )
#define DE( x... )
#endif

static pthread_mutex_t       play_lock;
static pthread_t             th_player;

static int stop_cmd=0;
static int playing=0;

/*
 * This is perhaps the simplest example use of the MAD high-level API.
 * Standard input is mapped into memory via mmap(), then the high-level API
 * is invoked with three callbacks: input, output, and error. The output
 * callback converts MAD's high-resolution PCM samples to 16 bits, then
 * writes them to standard output in little-endian, stereo-interleaved
 * format.
 */

static int decode(unsigned char const *, unsigned long);

static int sound_fd=0;
static int file_fd;

static char* path_mp3;

int player_init()
{
		sound_fd = MFGetSoundFD();

		return 0;
}

int play(const char * path)
{
    if(playing)
    {
        DE("Player is running\n");
        return -1;
    }
    
    path_mp3=(char *)path;

	pthread_create(&th_player,NULL,mp3_loop,NULL);

    pthread_mutex_unlock(&play_lock);
    pthread_mutex_lock(&play_lock);
    
    return 0;
}


void *mp3_loop(void *arg)
{
    //int temp;
    struct stat stat;
    void *fdm;

    int file_fd;

	// pthread_mutex_lock(&play_lock);

    file_fd=open(path_mp3,O_RDWR);

    playing = 1;
	stop_cmd = 0;

    if(!path_mp3)
    {
        DE("Null file path pointer\n");
		goto next;
    }
    
    if (fstat(file_fd, &stat) == -1 ||
        stat.st_size == 0)
    {
        DE("ERROR file state: %s\n",path_mp3);
        goto next;
    }

    fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, file_fd, 0);
    if (fdm == MAP_FAILED)
    {
        DE("Failed map mp3 file:%s\n",path_mp3);
        goto next;
    }

    decode((unsigned char*)fdm, stat.st_size);

    if (munmap(fdm, stat.st_size) == -1)
    {
        DE("Failed unmap mp3 file:%s\n",path_mp3);
        goto next;
    }
	
  next:
	
	//restart sr,if necessary
 	if (MFSRisStarted() == true && stop_cmd == 0)
	{
		MFSRContinue();
	}

	// pthread_mutex_unlock(&play_lock);
	playing=0;
    stop_cmd=0;


    return 0;
}

/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
    unsigned char const *start;
    unsigned long length;
};

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input(void *data,struct mad_stream *stream)
{
/*
	printf("mad_flow input");
    struct buffer *buffer = (struct buffer *)data;

    if (!buffer->length)
        return MAD_FLOW_STOP;

    mad_stream_buffer(stream, buffer->start, buffer->length);

    buffer->length = 0;

    return MAD_FLOW_CONTINUE;
*/
return 1;
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline signed int scale(mad_fixed_t sample)
{
/*
    // round 
    sample += (1L << (MAD_F_FRACBITS - 16));

    // clip 
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    // quantize 
    return sample >> (MAD_F_FRACBITS + 1 - 16);
*/
return 0;
}

#define SOUND_BUF_LEN  4096
static char buf[SOUND_BUF_LEN];
static int c_count=0;
int write_dsp_cache(int c) {
    buf[c_count]=c;
    c_count=(++c_count)%SOUND_BUF_LEN;
    if(c_count==SOUND_BUF_LEN-1)
        write(sound_fd, (char *)buf, SOUND_BUF_LEN);

    return 0;
}

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */
/*static enum mad_flow output(void *data,
                     struct mad_header const *header,
                     struct mad_pcm *pcm)
{
    unsigned int nchannels, nsamples,samplerate,format=AFMT_S16_NE;
    mad_fixed_t const *left_ch, *right_ch;
    int c;

    if(stop_cmd)
        return MAD_FLOW_STOP;
    
    // pcm->samplerate contains the sampling frequency 

    nchannels = pcm->channels;
    nsamples  = pcm->length;
    samplerate= pcm->samplerate;
    left_ch   = pcm->samples[0];
    right_ch  = pcm->samples[1];

    ioctl(sound_fd, SNDCTL_DSP_SPEED, &samplerate);
    ioctl(sound_fd, SNDCTL_DSP_SETFMT, &format);
    ioctl(sound_fd, SNDCTL_DSP_CHANNELS, &nchannels);
    
    while (nsamples--) {
        signed int sample;

        // output sample(s) in 16-bit signed little-endian PCM 

        sample = scale(*left_ch++);;
        c=(sample >> 0) & 0xff;
        write_dsp_cache(c);
        c=(sample >> 8) & 0xff;
        write_dsp_cache(c);

        if (nchannels == 2) {
            sample = scale(*right_ch++);
            c=(sample >> 0) & 0xff;
            write_dsp_cache(c);
            c=(sample >> 8) & 0xff;
            write_dsp_cache(c);
        }
    }

    return MAD_FLOW_CONTINUE;
}
*/

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */
/*
static
enum mad_flow error(void *data,
                    struct mad_stream *stream,
                    struct mad_frame *frame)
{
    struct buffer *buffer = (struct buffer *)data;

    DE( "decoding error 0x%04x (%s) at byte offset %u\n",
            stream->error, mad_stream_errorstr(stream),
            stream->this_frame - buffer->start);

    // return MAD_FLOW_BREAK here to stop decoding (and propagate an error)

    return MAD_FLOW_CONTINUE;
}*/

/*
 * This is the function called by main() above to perform all the decoding.
 * It instantiates a decoder object and configures it with the input,
 * output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */
/*
static
int decode(unsigned char const *start, unsigned long length)
{
    struct buffer buffer;
    struct mad_decoder decoder;
    int result;

    // initialize our private message structure 

    buffer.start  = start;
    buffer.length = length;
	//////////////////////////////////////////////////////////////////////////
	//ioctl( sound_fd, SNDCTL_DSP_SET_WRITE_MODE, 0);
	//////////////////////////////////////////////////////////////////////////
	
    // configure input, output, and error functions 

    mad_decoder_init(&decoder, &buffer,
                     input, 0 , 0 , output,
                     error, 0 );

    // start decoding 

    result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

    // release the decoder 

    mad_decoder_finish(&decoder);
	//////////////////////////////////////////////////////////////////////////
	//ioctl( sound_fd, SNDCTL_DSP_SET_READ_MODE, 0);
	//////////////////////////////////////////////////////////////////////////
	

    return result;
}
*/
/*
  Stop the player.If successed,return 0,else return -1;
*/
int stop_play(int wait)
{
    //int i=0;

    if(!playing)
        return 0;
    
    stop_cmd=1;

    while(wait--)
    {
        if(!stop_cmd) //wait for mp3_loop end 
            return 0;
        usleep(1);
    }

    return -1;
}

void player_exit()
{
    pthread_mutex_destroy(&play_lock);

    close(file_fd);

    return ;
}

void put_res_player()
{
   MFCloseSound();
   //close(sound_fd);
   sound_fd=0;
}

int isMP3Playing()
{
	return playing;
}
