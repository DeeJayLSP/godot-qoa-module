#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO

#include "audio_stream_qoa.h"

#include "core/io/file_access.h"

int AudioStreamPlaybackQOA::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	if (!active) {
		return 0;
	}

	int todo = p_frames;
	int start_buffer = 0;
	int frames_mixed_this_step = p_frames;

	int beat_length_frames = -1;
	bool beat_loop = qoa_stream->has_loop() && qoa_stream->get_bpm() > 0 && qoa_stream->get_beat_count() > 0;
	if (beat_loop) {
		beat_length_frames = qoa_stream->get_beat_count() * qoa_stream->sample_rate * 60 / qoa_stream->get_bpm();
	}

	while (todo && active) {
		float *buffer = (float *)p_buffer;
		if (start_buffer > 0) {
			buffer = (buffer + start_buffer * 2);
		}
		unsigned int num_samples = qoaplay_decode(qoad, buffer, todo);

		for (unsigned int num_samples_i = 0; num_samples_i < num_samples; num_samples_i++) {
			if (loop_fade_remaining < FADE_SIZE) {
				p_buffer[p_frames - todo] += loop_fade[loop_fade_remaining] * (float(FADE_SIZE - loop_fade_remaining) / float(FADE_SIZE));
				loop_fade_remaining++;
			}

			--todo;
			++frames_mixed;

			if (beat_loop && (int)frames_mixed >= beat_length_frames) {
				for (int i = 0; i < FADE_SIZE; i++) {
					p_buffer[i] = AudioFrame(buffer[i * qoa_stream->channels], buffer[i * qoa_stream->channels + qoa_stream->channels - 1]);
				}
				loop_fade_remaining = 0;
				seek(qoa_stream->loop_offset);
				loops++;
			}
		}

		if (frames_mixed >= qoad->info.samples - 64) {
			//EOF
			if (qoa_stream->loop) {
				seek(qoa_stream->loop_offset);
				loops++;
				start_buffer = p_frames - todo;
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
	qoaplay_seek_frame(qoad, frames_mixed / QOA_FRAME_LEN);
}

void AudioStreamPlaybackQOA::tag_used_streams() {
	qoa_stream->tag_used(get_playback_position());
}

AudioStreamPlaybackQOA::~AudioStreamPlaybackQOA() {
	if (qoad) {
		qoaplay_close(qoad);
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
	qoas->qoad = qoaplay_open(data.ptr(), data.size());

	qoas->frames_mixed = 0;
	qoas->active = false;
	qoas->loops = 0;

	if (!qoas->qoad) {
		ERR_FAIL_COND_V(!qoas->qoad, Ref<AudioStreamPlaybackQOA>());
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

	qoaplay_desc *qoad = qoaplay_open(src_datar, src_data_len);
	ERR_FAIL_COND_MSG(qoad == nullptr, "Failed to decode QOA file. Make sure it is a valid QOA audio file.");

	channels = qoad->info.channels;
	sample_rate = qoad->info.samplerate;
	length = float(qoad->info.samples) / (sample_rate);

	qoaplay_close(qoad);
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
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_offset"), "set_loop_offset", "get_loop_offset");
}

AudioStreamQOA::AudioStreamQOA() {
}

AudioStreamQOA::~AudioStreamQOA() {
	clear_data();
}
