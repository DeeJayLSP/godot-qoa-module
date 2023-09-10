#include "audio_stream_qoa.h"

#include "core/io/file_access.h"

#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO

#include <qoa.h>

int AudioStreamPlaybackQOA::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	if (!active) {
		return 0;
	}

	int todo = p_frames;
	// TODO: find a way to decode QOA.
	// The rest of this function doesn't decode anything.
	// The following is just an attempt to mimic other formats' decoding, but prevents the engine from freezing on close.
	
	int total_samples = qoad->samples * qoad->channels;
	short *sample_data = (short *)memalloc(total_samples * sizeof(short));
	unsigned int frame_len;

	int frames_mixed_this_step = p_frames;

	int beat_length_frames = -1;
	bool beat_loop = qoa_stream->has_loop() && qoa_stream->get_bpm() > 0 && qoa_stream->get_beat_count() > 0;
	if (beat_loop) {
		beat_length_frames = qoa_stream->get_beat_count() * qoa_stream->sample_rate * 60 / qoa_stream->get_bpm();
	}

	while (todo && active) {
		unsigned char *buffer = (unsigned char *)p_buffer;
		short *sample_ptr = sample_data + (p_frames - todo) * qoad->channels;

		// Find a way to correctly provide parameters to qoa_decode_frame. Always returning 0 because frame doesn't match description.
		unsigned int frame_size = qoa_decode_frame(buffer, qoa_max_frame_size(qoad), qoad, sample_ptr, &frame_len);

		if (frame_size) {
			p_buffer[p_frames - todo] = AudioFrame(buffer[0], buffer[frame_size - 1]);
			if (loop_fade_remaining < FADE_SIZE) {
				p_buffer[p_frames - todo] += loop_fade[loop_fade_remaining] * (float(FADE_SIZE - loop_fade_remaining) / float(FADE_SIZE));
				loop_fade_remaining++;
			}
			todo -= frame_size;
			++frames_mixed;

			if (beat_loop && (int)frames_mixed >= beat_length_frames) {
				for (int i = 0; i < FADE_SIZE; i++) {
					frame_size = qoa_decode_frame(buffer, qoa_max_frame_size(qoad), qoad, sample_ptr, &frame_len);
					loop_fade[i] = AudioFrame(buffer[0], buffer[frame_size - 1]);
					if (!frame_size) {
						break;
					}
				}
				loop_fade_remaining = 0;
				seek(qoa_stream->loop_offset);
				loops++;
			}
		}

		else {
			//EOF
			if (qoa_stream->loop) {
				seek(qoa_stream->loop_offset);
				loops++;
			} else {
				frames_mixed_this_step = p_frames - todo;
				//fill remainder with silence
				for (int i = p_frames - todo; i < p_frames; i++) {
					p_buffer[i] = AudioFrame(0, 0);
				}
				active = false;
				todo = 0;
			}
		}
	}

	return frames_mixed_this_step;
}

float AudioStreamPlaybackQOA::get_stream_sampling_rate() {
	return qoa_stream->sample_rate;
}

void AudioStreamPlaybackQOA::start(double p_from_pos) {
	active = true;
	seek(p_from_pos);
	loops = 0;
	begin_resample();
}

void AudioStreamPlaybackQOA::stop() {
	active = false;
}

bool AudioStreamPlaybackQOA::is_playing() const {
	return active;
}

int AudioStreamPlaybackQOA::get_loop_count() const {
	return loops;
}

double AudioStreamPlaybackQOA::get_playback_position() const {
	return double(frames_mixed) / qoa_stream->sample_rate;
}

void AudioStreamPlaybackQOA::seek(double p_time) {
	if (!active) {
		return;
	}

	if (p_time >= qoa_stream->get_length()) {
		p_time = 0;
	}

	frames_mixed = uint32_t(qoa_stream->sample_rate * p_time);
	// TODO: implement seek, that is, if qoa even supports it
}

void AudioStreamPlaybackQOA::tag_used_streams() {
	qoa_stream->tag_used(get_playback_position());
}

AudioStreamPlaybackQOA::~AudioStreamPlaybackQOA() {
	if (qoad) {
		// MAYBE_TODO: free qoad
		// A clue from qoaplay.c is that it frees a struct containing the qoa_desc. So maybe memfree is enough.
		memfree(qoad);
	}
}

Ref<AudioStreamPlayback> AudioStreamQOA::instantiate_playback() {
	Ref<AudioStreamPlaybackQOA> qoas;

	ERR_FAIL_COND_V_MSG(data.is_empty(), qoas,
			"This AudioStreamQOA does not have an audio file assigned "
			"to it. AudioStreamQOA should not be created from the "
			"inspector or with `.new()`. Instead, load an audio file.");

	qoas.instantiate();
	qoas->qoa_stream = Ref<AudioStreamQOA>(this);
	qoas->qoad = (qoa_desc *)memalloc(sizeof(qoa_desc));

	unsigned int first_frame_pos = qoa_decode_header(data.ptr(), data.size(), qoas->qoad);

	qoas->frames_mixed = 0;
	qoas->active = false;
	qoas->loops = 0;

	if (!first_frame_pos) {
		ERR_FAIL_COND_V(!first_frame_pos, Ref<AudioStreamPlaybackQOA>());
	}

	return qoas;
}

String AudioStreamQOA::get_stream_name() const {
	return "";
}

void AudioStreamQOA::clear_data() {
	data.clear();
}

void AudioStreamQOA::set_data(const Vector<uint8_t> &p_data) {
	int src_data_len = p_data.size();
	const uint8_t *src_datar = p_data.ptr();

	qoa_desc qoad;
	unsigned int err = qoa_decode_header(src_datar, src_data_len, &qoad);
	ERR_FAIL_COND_MSG(err != 8, "Failed to decode QOA file. Make sure it is a valid QOA audio file.");

	// Since channels, sample_rate and length are correctly read by Godot, this is assumed to be correct.

	channels = qoad.channels;
	sample_rate = qoad.samplerate;
	length = float(qoad.samples) / (sample_rate);

	// MAYBE_TODO: find a way to close the qoad without crashing the engine (if necessary)
	// A clue from qoaplay.c is that it frees the struct that contains the qoa_desc.

	clear_data();

	data.resize(src_data_len);
	memcpy(data.ptrw(), src_datar, src_data_len);
	data_len = src_data_len;
}

Vector<uint8_t> AudioStreamQOA::get_data() const {
	return data;
}

void AudioStreamQOA::set_loop(bool p_enable) {
	loop = p_enable;
}

bool AudioStreamQOA::has_loop() const {
	return loop;
}

void AudioStreamQOA::set_loop_offset(double p_seconds) {
	loop_offset = p_seconds;
}

double AudioStreamQOA::get_loop_offset() const {
	return loop_offset;
}

double AudioStreamQOA::get_length() const {
	return length;
}

bool AudioStreamQOA::is_monophonic() const {
	return false;
}

void AudioStreamQOA::set_bpm(double p_bpm) {
	ERR_FAIL_COND(p_bpm < 0);
	bpm = p_bpm;
	emit_changed();
}

double AudioStreamQOA::get_bpm() const {
	return bpm;
}

void AudioStreamQOA::set_beat_count(int p_beat_count) {
	ERR_FAIL_COND(p_beat_count < 0);
	beat_count = p_beat_count;
	emit_changed();
}

int AudioStreamQOA::get_beat_count() const {
	return beat_count;
}

void AudioStreamQOA::set_bar_beats(int p_bar_beats) {
	ERR_FAIL_COND(p_bar_beats < 2);
	bar_beats = p_bar_beats;
	emit_changed();
}

int AudioStreamQOA::get_bar_beats() const {
	return bar_beats;
}

void AudioStreamQOA::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &AudioStreamQOA::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &AudioStreamQOA::get_data);

	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &AudioStreamQOA::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamQOA::has_loop);

	ClassDB::bind_method(D_METHOD("set_loop_offset", "seconds"), &AudioStreamQOA::set_loop_offset);
	ClassDB::bind_method(D_METHOD("get_loop_offset"), &AudioStreamQOA::get_loop_offset);

	ClassDB::bind_method(D_METHOD("set_bpm", "bpm"), &AudioStreamQOA::set_bpm);
	ClassDB::bind_method(D_METHOD("get_bpm"), &AudioStreamQOA::get_bpm);

	ClassDB::bind_method(D_METHOD("set_beat_count", "beat_count"), &AudioStreamQOA::set_beat_count);
	ClassDB::bind_method(D_METHOD("get_beat_count"), &AudioStreamQOA::get_beat_count);

	ClassDB::bind_method(D_METHOD("set_bar_beats", "bar_beats"), &AudioStreamQOA::set_bar_beats);
	ClassDB::bind_method(D_METHOD("get_bar_beats"), &AudioStreamQOA::get_bar_beats);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bpm", PROPERTY_HINT_RANGE, "0,400,0.01,or_greater"), "set_bpm", "get_bpm");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "beat_count", PROPERTY_HINT_RANGE, "0,512,1,or_greater"), "set_beat_count", "get_beat_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bar_beats", PROPERTY_HINT_RANGE, "2,32,1,or_greater"), "set_bar_beats", "get_bar_beats");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_offset"), "set_loop_offset", "get_loop_offset");
}

AudioStreamQOA::AudioStreamQOA() {
}

AudioStreamQOA::~AudioStreamQOA() {
	clear_data();
}
