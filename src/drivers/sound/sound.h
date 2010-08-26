/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   src/drivers/sound/sound.h                                    *
 * Created:     2009-10-17 by Hampa Hug <hampa@hampa.ch>                     *
 * Copyright:   (C) 2009-2010 Hampa Hug <hampa@hampa.ch>                     *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/


#ifndef PCE_DRIVERS_SOUND_SOUND_H
#define PCE_DRIVERS_SOUND_SOUND_H 1


#include <stdio.h>
#include <stdint.h>


#define SND_CHN_MAX 16


/*!***************************************************************************
 * @short A second order IIR filter
 *****************************************************************************/
typedef struct {
	long a[3];
	long b[3];
	long x[3];
	long y[3];
} sound_iir2_t;


/*!***************************************************************************
 * @short The sound driver context
 *****************************************************************************/
typedef struct sound_drv_t {
	void          *ext;

	unsigned      channels;
	unsigned long sample_rate;
	int           sample_sign;

	unsigned long lowpass_freq;
	sound_iir2_t  lowpass_iir2[SND_CHN_MAX];

	unsigned long bbuf_max;
	unsigned char *bbuf;

	unsigned long sbuf_max;
	uint16_t      *sbuf;

	FILE          *wav_fp;
	unsigned long wav_cnt;
	char          wav_filter;

	void (*close) (struct sound_drv_t *sdrv);

	int (*write) (struct sound_drv_t *sdrv, const uint16_t *buf, unsigned cnt);

	int (*set_params) (struct sound_drv_t *sdrv,
		unsigned chn, unsigned long srate, int sign
	);
} sound_drv_t;


/*!***************************************************************************
 * @short Initialize an IIR2 filter
 *****************************************************************************/
void snd_iir2_init (sound_iir2_t *iir);

/*!***************************************************************************
 * @short Reset an IIR2 filter
 *****************************************************************************/
void snd_iir2_reset (sound_iir2_t *iir);

/*!***************************************************************************
 * @short Initialize a low-pass IIR filter
 * @param freq   The cut-off frequency
 * @param srate  The sample rate
 *****************************************************************************/
void snd_iir2_set_lowpass (sound_iir2_t *iir,
	unsigned long freq, unsigned long srate
);

/*!***************************************************************************
 * @short Filter samples with an IIR2 filter
 * @param dst   The destination buffer
 * @param src   The source buffer
 * @param cnt   The sample count
 * @param ofs   The sample offset in both src and dst
 * @param sign  The sample signedness in both src and dst
 *****************************************************************************/
void snd_iir2_filter (sound_iir2_t *iir,
	uint16_t *dst, const uint16_t *src, unsigned cnt, unsigned ofs, int sign
);


/*!***************************************************************************
 * @short Get a temporary byte buffer
 * @param cnt  The requested buffer size in bytes
 *
 * The buffer returned is sdrv->bbuf. Subsequent calls to this function
 * will invalidate the buffers returned by earlier calls.
 *****************************************************************************/
unsigned char *snd_get_bbuf (sound_drv_t *sdrv, unsigned long cnt);

/*!***************************************************************************
 * @short Get a temporary sample buffer
 * @param cnt  The requested buffer size in samples
 *
 * The buffer returned is sdrv->sbuf. Subsequent calls to this function
 * will invalidate the buffers returned by earlier calls.
 *****************************************************************************/
uint16_t *snd_get_sbuf (sound_drv_t *sdrv, unsigned long cnt);

/*!***************************************************************************
 * @short Store samples in a byte buffer
 * @param dst   The destination byte buffer of size (2 * cnt)
 * @param src   The source sample buffer
 * @param cnt   The number of samples in src
 * @param sign  If true the sign of samples is inverted
 * @param be    If true samples are stored in big endian format.
 *****************************************************************************/
void snd_set_buf (unsigned char *dst, const uint16_t *src, unsigned long cnt,
	int sign, int be
);


void snd_init (sound_drv_t *sdrv, void *ext);

void snd_close (sound_drv_t *sdrv);

/*!***************************************************************************
 * @short Filter samples
 * @param buf  The source samples
 * @param cnt  The number of samples in buf
 *
 * The samples in buf are filtered by sdrv->lowpass_iir2. The buffer returned
 * is sdrv->sbuf.
 *****************************************************************************/
const uint16_t *snd_filter (sound_drv_t *sdrv, const uint16_t *buf, unsigned cnt);

/*!***************************************************************************
 * @short Write samples
 * @param buf  The sample buffer
 * @param cnt  The sample count per channel
 *
 * The total number of samples in buf is (channels * cnt).
 *****************************************************************************/
int snd_write (sound_drv_t *sdrv, const uint16_t *buf, unsigned cnt);

/*!***************************************************************************
 * @short Set the sound parameters
 * @param chn    The number of channels
 * @param srate  The sample rate in Herz
 * @param sign   If true, samples are signed otherwise unsigned
 *
 * This function must be called after snd_open() and before the first call
 * to snd_write().
 *****************************************************************************/
int snd_set_params (sound_drv_t *sdrv, unsigned chn, unsigned long srate, int sign);


sound_drv_t *snd_open (const char *name);

sound_drv_t *snd_null_open (const char *name);


#endif
