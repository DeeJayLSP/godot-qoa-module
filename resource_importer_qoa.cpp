#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO

#include "resource_importer_qoa.h"

#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "scene/resources/texture.h"

#ifdef TOOLS_ENABLED
#include "editor/import/audio_stream_import_settings.h"
#endif

String ResourceImporterQOA::get_importer_name() const {
	return "qoa";
}

String ResourceImporterQOA::get_visible_name() const {
	return "Quite OK Audio";
}

void ResourceImporterQOA::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("qoa");
	p_extensions->push_back("wav");
}

String ResourceImporterQOA::get_save_extension() const {
	return "qoastr";
}

String ResourceImporterQOA::get_resource_type() const {
	return "AudioStreamQOA";
}

bool ResourceImporterQOA::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterQOA::get_preset_count() const {
	return 0;
}

String ResourceImporterQOA::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterQOA::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "loop"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "loop_offset"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "bpm", PROPERTY_HINT_RANGE, "0,400,0.01,or_greater"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "beat_count", PROPERTY_HINT_RANGE, "0,512,or_greater"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "bar_beats", PROPERTY_HINT_RANGE, "2,32,or_greater"), 4));
}

#ifdef TOOLS_ENABLED
bool ResourceImporterQOA::has_advanced_options() const {
	return true;
}
void ResourceImporterQOA::show_advanced_options(const String &p_path) {
	Ref<AudioStreamQOA> qoa_stream = import_stream(p_path);
	if (qoa_stream.is_valid()) {
		AudioStreamImportSettings::get_singleton()->edit(p_path, "qoa", qoa_stream);
	}
}
#endif

Ref<AudioStreamQOA> ResourceImporterQOA::import_stream(const String &p_path) {
	return p_path.ends_with(".qoa") ? import_from_qoa(p_path) : import_from_wav(p_path);
}

Ref<AudioStreamQOA> ResourceImporterQOA::import_from_qoa(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(f.is_null(), Ref<AudioStreamQOA>());

	uint64_t len = f->get_length();

	Vector<uint8_t> data;
	data.resize(len);
	uint8_t *w = data.ptrw();

	f->get_buffer(w, len);

	Ref<AudioStreamQOA> qoa_stream;
	qoa_stream.instantiate();

	qoa_stream->set_data(data);
	ERR_FAIL_COND_V(!qoa_stream->get_data().size(), Ref<AudioStreamQOA>());

	return qoa_stream;
}

Ref<AudioStreamQOA> ResourceImporterQOA::import_from_wav(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(file.is_null(), Ref<AudioStreamQOA>());

	// WAV import code borrowed from ResourceImporterWAV

	char riff[5];
	riff[4] = 0;
	file->get_buffer((uint8_t *)&riff, 4); //RIFF

	if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') {
		ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), vformat("Not a WAV file. File should start with 'RIFF', but found '%s', in file of size %d bytes", riff, file->get_length()));
	}

	file->get_32();

	char wave[5];
	wave[4] = 0;
	file->get_buffer((uint8_t *)&wave, 4); //WAVE

	if (wave[0] != 'W' || wave[1] != 'A' || wave[2] != 'V' || wave[3] != 'E') {
		ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), vformat("Not a WAV file. Header should contain 'WAVE', but found '%s', in file of size %d bytes", wave, file->get_length()));
	}

	int format_bits = 0;
	int format_channels = 0;
	bool format_found = false;
	bool data_found = false;
	int compression_code = 0;
	int format_freq = 0;
	int frames = 0;

	Vector<float> data;

	while (!file->eof_reached()) {
		/* chunk */
		char chunkID[4];
		file->get_buffer((uint8_t *)&chunkID, 4); //RIFF

		/* chunk size */
		uint32_t chunksize = file->get_32();
		uint32_t file_pos = file->get_position(); //save file pos, so we can skip to next chunk safely

		if (file->eof_reached()) {
			//ERR_PRINT("EOF REACH");
			break;
		}

		if (chunkID[0] == 'f' && chunkID[1] == 'm' && chunkID[2] == 't' && chunkID[3] == ' ' && !format_found) {
			/* IS FORMAT CHUNK */

			//Issue: #7755 : Not a bug - usage of other formats (format codes) are unsupported in current importer version.
			//Consider revision for engine version 3.0
			compression_code = file->get_16();
			if (compression_code != 1 && compression_code != 3) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Format not supported for WAVE file (not PCM). Save WAVE files as uncompressed PCM or IEEE float instead.");
			}

			format_channels = file->get_16();
			if (format_channels != 1 && format_channels != 2) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Format not supported for WAVE file (not stereo or mono).");
			}

			format_freq = file->get_32(); //sampling rate

			file->get_32(); // average bits/second (unused)
			file->get_16(); // block align (unused)
			format_bits = file->get_16(); // bits per sample

			if (format_bits % 8 || format_bits == 0) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Invalid amount of bits in the sample (should be one of 8, 16, 24 or 32).");
			}

			if (compression_code == 3 && format_bits % 32) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Invalid amount of bits in the IEEE float sample (should be 32 or 64).");
			}

			/* Don't need anything else, continue */
			format_found = true;
		}

		if (chunkID[0] == 'd' && chunkID[1] == 'a' && chunkID[2] == 't' && chunkID[3] == 'a' && !data_found) {
			/* IS DATA CHUNK */
			data_found = true;

			if (!format_found) {
				ERR_PRINT("'data' chunk before 'format' chunk found.");
				break;
			}

			frames = chunksize;

			if (format_channels == 0) {
				ERR_FAIL_COND_V(format_channels == 0, Ref<AudioStreamQOA>());
			}
			frames /= format_channels;
			frames /= (format_bits >> 3);

			data.resize(frames * format_channels);

			if (compression_code == 1) {
				if (format_bits == 8) {
					for (int i = 0; i < frames * format_channels; i++) {
						// 8 bit samples are UNSIGNED

						data.write[i] = int8_t(file->get_8() - 128) / 128.f;
					}
				} else if (format_bits == 16) {
					for (int i = 0; i < frames * format_channels; i++) {
						//16 bit SIGNED

						data.write[i] = int16_t(file->get_16()) / 32768.f;
					}
				} else {
					for (int i = 0; i < frames * format_channels; i++) {
						//16+ bits samples are SIGNED
						// if sample is > 16 bits, just read extra bytes

						uint32_t s = 0;
						for (int b = 0; b < (format_bits >> 3); b++) {
							s |= ((uint32_t)file->get_8()) << (b * 8);
						}
						s <<= (32 - format_bits);

						data.write[i] = (int32_t(s) >> 16) / 32768.f;
					}
				}
			} else if (compression_code == 3) {
				if (format_bits == 32) {
					for (int i = 0; i < frames * format_channels; i++) {
						//32 bit IEEE Float

						data.write[i] = file->get_float();
					}
				} else if (format_bits == 64) {
					for (int i = 0; i < frames * format_channels; i++) {
						//64 bit IEEE Float

						data.write[i] = file->get_double();
					}
				}
			}

			if (file->eof_reached()) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Premature end of file.");
			}
		}

		file->seek(file_pos + chunksize + (chunksize & 1));
	}

	Vector<uint8_t> wav_data;
	// Since the converter requires 16 bit samples, we have to enforce it
	wav_data.resize(data.size() * 2);
	{
		uint8_t *w = wav_data.ptrw();

		int ds = data.size();
		for (int i = 0; i < ds; i++) {
			int16_t v = CLAMP(data[i] * 32768, -32768, 32767);
			encode_uint16(v, &w[i * 2]);
		}
	}

	// End of borrowed code, we now have everything needed

	qoa_desc desc;

	desc.samplerate = format_freq;
	desc.samples = frames;
	desc.channels = format_channels;

	uint32_t dst_size;
	void *encoded = qoa_encode((short *)wav_data.ptrw(), &desc, &dst_size);

	Ref<AudioStreamQOA> qoa_stream;
	qoa_stream.instantiate();

	Vector<uint8_t> dst_data;
	dst_data.resize(dst_size);
	memcpy(dst_data.ptrw(), encoded, dst_size);

	qoa_stream->set_data(dst_data);

	return qoa_stream;
}

Error ResourceImporterQOA::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	bool loop = p_options["loop"];
	float loop_offset = p_options["loop_offset"];
	double bpm = p_options["bpm"];
	float beat_count = p_options["beat_count"];
	float bar_beats = p_options["bar_beats"];

	Ref<AudioStreamQOA> qoa_stream = import_stream(p_source_file);
	if (qoa_stream.is_null()) {
		return ERR_CANT_OPEN;
	}
	qoa_stream->set_loop(loop);
	qoa_stream->set_loop_offset(loop_offset);
	qoa_stream->set_bpm(bpm);
	qoa_stream->set_beat_count(beat_count);
	qoa_stream->set_bar_beats(bar_beats);

	return ResourceSaver::save(qoa_stream, p_save_path + ".qoastr");
}

ResourceImporterQOA::ResourceImporterQOA() {
}
