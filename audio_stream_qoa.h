#ifndef AUDIO_STREAM_QOA_H
#define AUDIO_STREAM_QOA_H

#include "core/io/resource_loader.h"
#include "servers/audio/audio_stream.h"

#include "./thirdparty/qoa.h"

class AudioStreamQOA;

class AudioStreamPlaybackQOA : public AudioStreamPlaybackResampled {
	GDCLASS(AudioStreamPlaybackQOA, AudioStreamPlaybackResampled);

	qoa_desc *qoad = nullptr;
	uint32_t data_offset = 0;
	uint32_t frames_mixed = 0;
	uint32_t frame_data_len = 0;
	int16_t *decoded = nullptr;
	uint32_t decoded_len = 0;
	uint32_t decoded_offset = 0;

	bool active = false;
	int sign = 1;
	int loops = 0;

	friend class AudioStreamQOA;

	Ref<AudioStreamQOA> qoa_stream;

protected:
	virtual int _mix_internal(AudioFrame *p_buffer, int p_frames) override;
	virtual float get_stream_sampling_rate() override;

public:
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override; //times it looped

	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;

	virtual void tag_used_streams() override;

	AudioStreamPlaybackQOA() {}
	~AudioStreamPlaybackQOA();
};

class AudioStreamQOA : public AudioStream {
	GDCLASS(AudioStreamQOA, AudioStream);
	OBJ_SAVE_TYPE(AudioStream) //children are all saved as AudioStream, so they can be exchanged
	RES_BASE_EXTENSION("qoastr");

public:
	// Keep the ResourceImporterQOA `edit/loop_mode` enum hint in sync with these options.
	enum LoopMode {
		LOOP_DISABLED,
		LOOP_FORWARD,
		LOOP_PINGPONG,
		LOOP_BACKWARD,
	};


private:
	friend class AudioStreamPlaybackQOA;

	PackedByteArray data;
	uint32_t data_len = 0;

	LoopMode loop_mode = LOOP_DISABLED;
	int channels = 1;
	float length = 0.0;
	int loop_begin = 0;
	int loop_end = -1;
	int mix_rate = 44100;
	void clear_data();

protected:
	static void _bind_methods();

public:
	void set_loop_mode(LoopMode p_loop_mode);
	LoopMode get_loop_mode() const;

	void set_loop_begin(int p_frame);
	int get_loop_begin() const;

	void set_loop_end(int p_frame);
	int get_loop_end() const;

	void set_mix_rate(int p_hz);
	int get_mix_rate() const;

	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual String get_stream_name() const override;

	void set_data(const Vector<uint8_t> &p_data);
	Vector<uint8_t> get_data() const;

	virtual double get_length() const override;

	virtual bool is_monophonic() const override;

	AudioStreamQOA();
	virtual ~AudioStreamQOA();
};

VARIANT_ENUM_CAST(AudioStreamQOA::LoopMode)

#endif // AUDIO_STREAM_QOA_H
