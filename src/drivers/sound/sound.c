/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   src/drivers/sound/sound.c                                    *
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


#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <drivers/options.h>
#include <drivers/sound/sound.h>
#include <drivers/sound/sound-wav.h>


#define SND_IIR_MUL 8192


struct snd_drv_list {
	const char *prefix;
	sound_drv_t *(*open) (const char *name);
};

static
struct snd_drv_list drvtab[] = {
	{ "null", snd_null_open },
	{ "wav", snd_null_open },
	{ NULL, NULL }
};


void snd_iir2_init (sound_iir2_t *iir)
{
	unsigned i;

	for (i = 0; i < 3; i++) {
		iir->a[i] = 0;
		iir->b[i] = 0;
		iir->x[i] = 0;
		iir->y[i] = 0;
	}

	iir->a[0] = SND_IIR_MUL;
}

void snd_iir2_reset (sound_iir2_t *iir)
{
	iir->x[0] = 0;
	iir->x[1] = 0;
	iir->x[2] = 0;

	iir->y[0] = 0;
	iir->y[1] = 0;
	iir->y[2] = 0;
}

void snd_iir2_set_lowpass (sound_iir2_t *iir, unsigned long freq, unsigned long srate)
{
	double om, b0;

	snd_iir2_init (iir);

	if ((freq == 0) || (srate == 0)) {
		return;
	}

	if ((2 * freq) >= srate) {
		freq = (srate / 2) - 1;
	}

	om = 1.0 / tan ((3.14159265358979312 * freq) / srate);
	b0 = om * (om + sqrt(2.0)) + 1.0;

	iir->a[0] = (long) (SND_IIR_MUL * 1.0 / b0);
	iir->a[1] = (long) (SND_IIR_MUL * 2.0 / b0);
	iir->a[2] = (long) (SND_IIR_MUL * 1.0 / b0);

	iir->b[0] = (long) (SND_IIR_MUL * 1.0);
	iir->b[1] = (long) (SND_IIR_MUL * 2.0 * (1.0 - om * om) / b0);
	iir->b[2] = (long) (SND_IIR_MUL * (om * (om - sqrt(2.0)) + 1.0) / b0);
}

void snd_iir2_filter (sound_iir2_t *iir, uint16_t *dst, const uint16_t *src,
	unsigned cnt, unsigned ofs, int sign)
{
	long     v;
	uint16_t sig;

	sig = sign ? 0x8000 : 0;

	while (cnt > 0) {
		v = *src ^ sig;
		v -= 32768;

		iir->x[2] = iir->x[1];
		iir->x[1] = iir->x[0];
		iir->x[0] = v;

		iir->y[2] = iir->y[1];
		iir->y[1] = iir->y[0];

		iir->y[0] = iir->a[0] * iir->x[0];
		iir->y[0] += iir->a[1] * iir->x[1] + iir->a[2] * iir->x[2];
		iir->y[0] -= iir->b[1] * iir->y[1] + iir->b[2] * iir->y[2];

		iir->y[0] = iir->y[0] / SND_IIR_MUL;

		v = iir->y[0];
		v += 32768;

		if (v < 0) {
			v = 0;
		}
		else if (v > 65535) {
			v = 0xffff;
		}
		else {
			v &= 0xffff;
		}

		*dst = ((uint16_t) v) ^ sig;

		src += ofs;
		dst += ofs;
		cnt -= 1;
	}
}

/*
 * Initialize the low-pass filter in sdrv->lowpass_iir2 with a cut-off
 * frequency of sdrv->lowpass_freq.
 */
static
void snd_fix_lowpass (sound_drv_t *sdrv)
{
	unsigned i;

	for (i = 0; i < sdrv->channels; i++) {
		snd_iir2_set_lowpass (
			&sdrv->lowpass_iir2[i],
			sdrv->lowpass_freq, sdrv->sample_rate
		);

		snd_iir2_reset (&sdrv->lowpass_iir2[i]);
	}
}

unsigned char *snd_get_bbuf (sound_drv_t *sdrv, unsigned long cnt)
{
	unsigned char *tmp;

	if (cnt <= sdrv->bbuf_max) {
		return (sdrv->bbuf);
	}

	tmp = realloc (sdrv->bbuf, cnt);

	if (tmp == NULL) {
		return (NULL);
	}

	sdrv->bbuf = tmp;
	sdrv->bbuf_max = cnt;

	return (sdrv->bbuf);
}

uint16_t *snd_get_sbuf (sound_drv_t *sdrv, unsigned long cnt)
{
	uint16_t *tmp;

	if (cnt <= sdrv->sbuf_max) {
		return (sdrv->sbuf);
	}

	tmp = realloc (sdrv->sbuf, cnt * sizeof (uint16_t));

	if (tmp == NULL) {
		return (NULL);
	}

	sdrv->sbuf = tmp;
	sdrv->sbuf_max = cnt;

	return (sdrv->sbuf);
}

void snd_set_buf (unsigned char *dst, const uint16_t *src, unsigned long cnt,
	int sign, int be)
{
	unsigned long i;
	uint16_t      val, sig;

	sig = sign ? 0x8000 : 0x0000;

	if (be) {
		for (i = 0; i < cnt; i++) {
			val = *src ^ sig;

			dst[0] = (val >> 8) & 0xff;
			dst[1] = val & 0xff;

			dst += 2;
			src += 1;
		}
	}
	else {
		for (i = 0; i < cnt; i++) {
			val = *src ^ sig;

			dst[0] = val & 0xff;
			dst[1] = (val >> 8) & 0xff;

			dst += 2;
			src += 1;
		}
	}
}

void snd_init (sound_drv_t *sdrv, void *ext)
{
	sdrv->ext = ext;

	sdrv->channels = 0;
	sdrv->sample_rate = 0;
	sdrv->sample_sign = 0;

	sdrv->lowpass_freq = 0;

	sdrv->bbuf_max = 0;
	sdrv->bbuf = NULL;

	sdrv->sbuf_max = 0;
	sdrv->sbuf = NULL;

	sdrv->wav_fp = NULL;
	sdrv->wav_cnt = 0;
	sdrv->wav_filter = 1;

	sdrv->close = NULL;

	sdrv->write = NULL;

	sdrv->set_params = NULL;
}

void snd_close (sound_drv_t *sdrv)
{
	if (sdrv == NULL) {
		return;
	}

	snd_wav_close (sdrv);

	if (sdrv->close != NULL) {
		sdrv->close (sdrv);
	}

	if (sdrv->sbuf != NULL) {
		free (sdrv->sbuf);
	}

	if (sdrv->bbuf != NULL) {
		free (sdrv->bbuf);
	}
}

const uint16_t *snd_filter (sound_drv_t *sdrv, const uint16_t *buf, unsigned cnt)
{
	unsigned      i;
	unsigned long scnt;
	uint16_t      *sbuf;

	if (sdrv->lowpass_freq == 0) {
		return (buf);
	}

	scnt = (unsigned long) sdrv->channels * (unsigned long) cnt;

	sbuf = snd_get_sbuf (sdrv, scnt);

	if (sbuf == NULL) {
		return (NULL);
	}

	for (i = 0; i < sdrv->channels; i++) {
		snd_iir2_filter (
			&sdrv->lowpass_iir2[i], sbuf + i, buf + i,
			cnt, sdrv->channels, sdrv->sample_sign
		);
	}

	return (sbuf);
}

int snd_write (sound_drv_t *sdrv, const uint16_t *buf, unsigned cnt)
{
	int            r;
	const uint16_t *sbuf;

	if ((sdrv == NULL) || (sdrv->write == NULL)) {
		return (1);
	}

	sbuf = snd_filter (sdrv, buf, cnt);

	r = sdrv->write (sdrv, sbuf, cnt);

	snd_wav_write (sdrv, sdrv->wav_filter ? sbuf : buf, cnt);

	return (r);
}

/*
 * Check if new parameters differ from current parameters.
 * Returns 0 if they differ.
 */
static
int snd_check_params (sound_drv_t *sdrv, unsigned chn, unsigned long srate, int sign)
{
	if (sdrv->channels != chn) {
		return (0);
	}

	if (sdrv->sample_rate != srate) {
		return (0);
	}

	if (sdrv->sample_sign != sign) {
		return (0);
	}

	return (1);
}

int snd_set_params (sound_drv_t *sdrv, unsigned chn, unsigned long srate, int sign)
{
	if (sdrv == NULL) {
		return (1);
	}

	if (sdrv->set_params == NULL) {
		return (1);
	}

	sign = (sign != 0);

	if (chn > SND_CHN_MAX) {
		return (1);
	}
	if (snd_check_params (sdrv, chn, srate, sign)) {
		return (0);
	}

	if (sdrv->set_params (sdrv, chn, srate, sign)) {
		return (1);
	}

	sdrv->channels = chn;
	sdrv->sample_rate = srate;
	sdrv->sample_sign = sign;

	if (snd_wav_set_params (sdrv, chn, srate, sign)) {
		return (1);
	}

	snd_fix_lowpass (sdrv);

	return (0);
}

static
sound_drv_t *snd_open_sdrv (sound_drv_t *sdrv, const char *name)
{
	if (sdrv == NULL) {
		return (NULL);
	}

	if (snd_wav_init (sdrv, name)) {
		snd_close (sdrv);
		return (NULL);
	}

	sdrv->wav_filter = drv_get_option_bool (name, "wavfilter", 1);

	sdrv->lowpass_freq = drv_get_option_uint (name, "lowpass", 0);

	snd_fix_lowpass (sdrv);

	return (sdrv);
}

sound_drv_t *snd_open (const char *name)
{
	unsigned   i;
	const char *s, *d;

	i = 0;

	while (drvtab[i].prefix != NULL) {
		s = name;
		d = drvtab[i].prefix;

		while ((*d != 0) && (*d == *s)) {
			d += 1;
			s += 1;
		}

		if ((*d == 0) && ((*s == ':') || (*s == 0))) {
			return (snd_open_sdrv (drvtab[i].open (name), name));
		}

		i += 1;
	}

	return (NULL);
}
