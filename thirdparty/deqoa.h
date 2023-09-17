/*******************************************************************************
 * Copyright (c) 2023 Dominic Szablewski (@phoboslab), original author
 * Copyright (c) 2023 Ramon Santamaria (@raysan5), added reading from memory
 * Copyright (c) 2023 Douglas Leão (@DeeJayLSP), minimal reviewer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

// DeQOA - A minimal QOA decoder from memory
//
// This header contains basic functions to decode QOA files from memory.
//
// This is a compact version of the original qoa files qoa.h and qoaplay.c,
// made by phoboslab and later modified by raysan5 to support reading from memory.
// DeeJayLSP compacted it for decoding purposes only and stereo enforcing.
//
// Note that this header was originally made as an implementation of QOA for the
// Godot Engine, therefore may not have all the features you need.
//
// The following functions are exposed:
// 	deqoa_open_memory()
// 	deqoa_close()
// 	deqoa_seek_frame()
// 	deqoa_decode()
//
// The DEQOA_ENFORCE_STEREO define should be used when the target buffer is stereo-enforced.

#ifndef DEQOA_H
#define DEQOA_H

#ifdef __cplusplus
extern "C" {
#endif

#define QOA_MIN_FILESIZE 16

#define QOA_SLICE_LEN 20
#define QOA_SLICES_PER_FRAME 256
#define QOA_FRAME_LEN (QOA_SLICES_PER_FRAME * QOA_SLICE_LEN)
#define QOA_LMS_LEN 4

#define QOA_FRAME_SIZE(channels, slices) \
	(8 + QOA_LMS_LEN * 4 * channels + 8 * slices * channels)

typedef struct {
	int history[QOA_LMS_LEN];
	int weights[QOA_LMS_LEN];
} qoa_lms_t;

typedef struct {
	unsigned int channels;
	unsigned int samplerate;
	unsigned int samples;
	qoa_lms_t lms[8];
} qoa_desc;

typedef struct {
	qoa_desc info;

	unsigned char *file_data;
	unsigned int file_data_size;
	unsigned int file_data_offset;

	unsigned int first_frame_pos;
	unsigned int sample_position;

	unsigned char *buffer;
	unsigned int buffer_len;

	short *sample_data;
	unsigned int sample_data_len;
	unsigned int sample_data_pos;

} deqoa;

deqoa *deqoa_open_memory(const unsigned char *data, int data_size);
void deqoa_close(deqoa *qdc);

void deqoa_seek_frame(deqoa *qdc, int frame);
unsigned int deqoa_decode_stereo(deqoa *qdc, float *sample_data, int num_samples);

#ifdef __cplusplus
}
#endif
#endif //DEQOA_H

#ifdef DEQOA_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

#define QOA_F32_DIVISOR 32768.0f

static const int qoa_dequant_tab[16][8] = {
	{ 1, -1, 3, -3, 5, -5, 7, -7 },
	{ 5, -5, 18, -18, 32, -32, 49, -49 },
	{ 16, -16, 53, -53, 95, -95, 147, -147 },
	{ 34, -34, 113, -113, 203, -203, 315, -315 },
	{ 63, -63, 210, -210, 378, -378, 588, -588 },
	{ 104, -104, 345, -345, 621, -621, 966, -966 },
	{ 158, -158, 528, -528, 950, -950, 1477, -1477 },
	{ 228, -228, 760, -760, 1368, -1368, 2128, -2128 },
	{ 316, -316, 1053, -1053, 1895, -1895, 2947, -2947 },
	{ 422, -422, 1405, -1405, 2529, -2529, 3934, -3934 },
	{ 548, -548, 1828, -1828, 3290, -3290, 5117, -5117 },
	{ 696, -696, 2320, -2320, 4176, -4176, 6496, -6496 },
	{ 868, -868, 2893, -2893, 5207, -5207, 8099, -8099 },
	{ 1064, -1064, 3548, -3548, 6386, -6386, 9933, -9933 },
	{ 1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005 },
	{ 1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336 },
};

static int qoa_lms_predict(qoa_lms_t *lms) {
	int prediction = 0;
	for (int i = 0; i < QOA_LMS_LEN; i++) {
		prediction += lms->weights[i] * lms->history[i];
	}
	return prediction >> 13;
}

static void qoa_lms_update(qoa_lms_t *lms, int sample, int residual) {
	int delta = residual >> 4;
	for (int i = 0; i < QOA_LMS_LEN; i++) {
		lms->weights[i] += lms->history[i] < 0 ? -delta : delta;
	}

	for (int i = 0; i < QOA_LMS_LEN - 1; i++) {
		lms->history[i] = lms->history[i + 1];
	}
	lms->history[QOA_LMS_LEN - 1] = sample;
}

static inline int qoa_clamp(int v, int min, int max) {
	if (v < min)
		return min;
	if (v > max)
		return max;
	return v;
}

static inline int qoa_clamp_s16(int v) {
	if ((unsigned int)(v + 32768) > 65535) {
		if (v < -32768)
			return -32768;
		if (v > 32767)
			return 32767;
	}
	return v;
}

static inline unsigned long long qoa_read_u64(const unsigned char *bytes, unsigned int *p) {
	bytes += *p;
	*p += 8;
	return ((unsigned long long)(bytes[0]) << 56) | ((unsigned long long)(bytes[1]) << 48) |
			((unsigned long long)(bytes[2]) << 40) | ((unsigned long long)(bytes[3]) << 32) |
			((unsigned long long)(bytes[4]) << 24) | ((unsigned long long)(bytes[5]) << 16) |
			((unsigned long long)(bytes[6]) << 8) | ((unsigned long long)(bytes[7]) << 0);
}

static inline unsigned int qoa_max_frame_size(qoa_desc *qd) {
	return QOA_FRAME_SIZE(qd->channels, QOA_SLICES_PER_FRAME);
}

static inline unsigned int qoa_decode_header(const unsigned char *bytes, int size, qoa_desc *qd) {
	unsigned int p = 0;
	if (size < QOA_MIN_FILESIZE)
		return 0;

	unsigned long long file_header = qoa_read_u64(bytes, &p);

	if ((file_header >> 32) != 0x716f6166)
		return 0;

	qd->samples = file_header & 0xffffffff;
	if (!qd->samples)
		return 0;

	unsigned long long frame_header = qoa_read_u64(bytes, &p);
	qd->channels = (frame_header >> 56) & 0x0000ff;
	qd->samplerate = (frame_header >> 32) & 0xffffff;

	if (qd->channels == 0 || qd->samples == 0 || qd->samplerate == 0)
		return 0;

	return 8;
}

static inline unsigned int qoa_decode_frame(const unsigned char *bytes, unsigned int size, qoa_desc *qd, short *sample_data, unsigned int *frame_len) {
	unsigned int p = 0;
	*frame_len = 0;

	if (size < 8 + QOA_LMS_LEN * 4 * qd->channels)
		return 0;

	unsigned long long frame_header = qoa_read_u64(bytes, &p);
	unsigned int channels = (frame_header >> 56) & 0x0000ff;
	unsigned int samplerate = (frame_header >> 32) & 0xffffff;
	unsigned int samples = (frame_header >> 16) & 0x00ffff;
	unsigned int frame_size = (frame_header) & 0x00ffff;

	unsigned int max_total_samples = (frame_size - 8 - QOA_LMS_LEN * 4 * channels) / 8 * QOA_SLICE_LEN;

	if (
			channels != qd->channels ||
			samplerate != qd->samplerate ||
			frame_size > size ||
			samples * channels > max_total_samples)
		return 0;

	for (unsigned int c = 0; c < channels; c++) {
		unsigned long long history = qoa_read_u64(bytes, &p);
		unsigned long long weights = qoa_read_u64(bytes, &p);
		for (unsigned int i = 0; i < QOA_LMS_LEN; i++) {
			qd->lms[c].history[i] = ((signed short)(history >> 48));
			history <<= 16;
			qd->lms[c].weights[i] = ((signed short)(weights >> 48));
			weights <<= 16;
		}
	}

	for (unsigned int sample_i = 0; sample_i < samples; sample_i += QOA_SLICE_LEN) {
		for (unsigned int c = 0; c < channels; c++) {
			unsigned long long slice = qoa_read_u64(bytes, &p);

			int scalefactor = (slice >> 60) & 0xf;
			int slice_start = sample_i * channels + c;
			int slice_end = qoa_clamp(sample_i + QOA_SLICE_LEN, 0, samples) * channels + c;

			for (int slice_i = slice_start; slice_i < slice_end; slice_i += channels) {
				int predicted = qoa_lms_predict(&qd->lms[c]);
				int quantized = (slice >> 57) & 0x7;
				int dequantized = qoa_dequant_tab[scalefactor][quantized];
				int reconstructed = qoa_clamp_s16(predicted + dequantized);

				sample_data[slice_i] = reconstructed;
				slice <<= 3;

				qoa_lms_update(&qd->lms[c], reconstructed, dequantized);
			}
		}
	}

	*frame_len = samples;
	return p;
}

static inline unsigned int deqoa_decode_frame(deqoa *qdc) {
	qdc->buffer_len = qoa_max_frame_size(&qdc->info);
	memcpy(qdc->buffer, qdc->file_data + qdc->file_data_offset, qdc->buffer_len);
	qdc->file_data_offset += qdc->buffer_len;

	unsigned int frame_len;
	qoa_decode_frame(qdc->buffer, qdc->buffer_len, &qdc->info, qdc->sample_data, &frame_len);
	qdc->sample_data_pos = 0;
	qdc->sample_data_len = frame_len;

	return frame_len;
}

// Opens a QOA file from memory. Returns NULL if the file is invalid.
deqoa *deqoa_open_memory(const unsigned char *data, int data_size) {
	unsigned char header[QOA_MIN_FILESIZE];
	memcpy(header, data, QOA_MIN_FILESIZE);

	qoa_desc qd;
	unsigned int first_frame_pos = qoa_decode_header(header, QOA_MIN_FILESIZE, &qd);
	if (!first_frame_pos)
		return NULL;

	unsigned int buffer_size = qoa_max_frame_size(&qd);
	unsigned int sample_data_size = qd.channels * QOA_FRAME_LEN * sizeof(short) * 2;
	deqoa *qdc = (deqoa *)malloc(sizeof(deqoa) + buffer_size + sample_data_size);
	memset(qdc, 0, sizeof(deqoa));

	qdc->file_data = (unsigned char *)malloc(data_size);
	memcpy(qdc->file_data, data, data_size);
	qdc->file_data_size = data_size;
	qdc->file_data_offset = 0;
	qdc->first_frame_pos = first_frame_pos;

	qdc->buffer = ((unsigned char *)qdc) + sizeof(deqoa);
	qdc->sample_data = (short *)(((unsigned char *)qdc) + sizeof(deqoa) + buffer_size);

	qdc->info.channels = qd.channels;
	qdc->info.samplerate = qd.samplerate;
	qdc->info.samples = qd.samples;

	return qdc;
}

// Frees memory used by a previously opened QOA file.
void deqoa_close(deqoa *qdc) {
	if ((qdc->file_data) && (qdc->file_data_size > 0)) {
		free(qdc->file_data);
		qdc->file_data_size = 0;
	}
	free(qdc);
}

// Seek to a frame in the QOA file.
void deqoa_seek_frame(deqoa *qdc, int frame) {
	frame = qoa_clamp(frame, 0, (int)(qdc->info.samples / QOA_FRAME_LEN));

	qdc->sample_position = frame * QOA_FRAME_LEN;
	qdc->sample_data_len = 0;
	qdc->sample_data_pos = 0;

	unsigned int offset = qdc->first_frame_pos + frame * qoa_max_frame_size(&qdc->info);

	qdc->file_data_offset = offset;
}

// Main decode function.
unsigned int deqoa_decode(deqoa *qdc, float *sample_data, int num_samples) {
	int src_index = qdc->sample_data_pos * qdc->info.channels;
	int dst_index = 0;

	for (int i = 0; i < num_samples; i++) {
		if (qdc->sample_data_len - qdc->sample_data_pos == 0) {
			if (!deqoa_decode_frame(qdc)) {
				deqoa_seek_frame(qdc, 0);
				deqoa_decode_frame(qdc);
			}
			src_index = 0;
		}

/*
A workaround made with Godot in mind, since its AudioFrame buffer enforces stereo.
This will send the same sample to L and R if Mono, and discard any channels other
than left and right otherwise.
*/
#ifdef DEQOA_ENFORCE_STEREO
		if (qdc->info.channels == 1)
			sample_data[dst_index++] = qdc->sample_data[src_index] / QOA_F32_DIVISOR;
		else
			sample_data[dst_index++] = qdc->sample_data[src_index++] / QOA_F32_DIVISOR;
		sample_data[dst_index++] = qdc->sample_data[src_index++] / QOA_F32_DIVISOR;

		if (qdc->info.channels > 2)
			src_index += qdc->info.channels - 2;
#else
		for (int c = 0; c < qdc->info.channels; c++) {
			sample_data[dst_index++] = qdc->sample_data[src_index++] / QOA_F32_DIVISOR;
		}
#endif
		qdc->sample_data_pos++;
		qdc->sample_position++;
	}

	return num_samples;
}

#endif //DEQOA_IMPLEMENTATION
