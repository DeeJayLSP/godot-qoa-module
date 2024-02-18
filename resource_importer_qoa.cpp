#include "resource_importer_qoa.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "scene/resources/texture.h"

#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO

#include "thirdparty/qoa.h"

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
	Ref<AudioStreamQOA> qoa_stream;
	if (p_path.ends_with(".qoa")) {
		qoa_stream = import_qoa(p_path);
	} else if (p_path.ends_with(".wav")) {
		qoa_stream = import_wav(p_path);
	}
	if (qoa_stream.is_valid()) {
		AudioStreamImportSettings::get_singleton()->edit(p_path, "qoa", qoa_stream);
	}
}
#endif

Ref<AudioStreamQOA> ResourceImporterQOA::import_qoa(const String &p_path) {
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

Ref<AudioStreamQOA> ResourceImporterQOA::import_wav(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(file.is_null(), Ref<AudioStreamQOA>());

	char riff[5];
	riff[4] = 0;
	file->get_buffer((uint8_t *)&riff, 4); //RIFF

	ERR_FAIL_COND_V(riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F', Ref<AudioStreamQOA>());

	file->get_32();

	char wave[5];
	wave[4] = 0;
	file->get_buffer((uint8_t *)&wave, 4); //WAVE

	ERR_FAIL_COND_V(wave[0] != 'W' || wave[1] != 'A' || wave[2] != 'V' || wave[3] != 'E', Ref<AudioStreamQOA>());

	uint32_t data_size = 0;
	bool format_found = false;
	uint32_t compression_code = 0;
	uint32_t format_channels = 0;
	uint32_t format_freq = 0;
	uint32_t format_bits = 0;

	while (1) {
		char chunkID[4];
		file->get_buffer((uint8_t *)&chunkID, 4);

		uint32_t chunksize = file->get_32();

		if (chunkID[0] == 'f' && chunkID[1] == 'm' && chunkID[2] == 't' && chunkID[3] == ' ') {
			ERR_FAIL_COND_V_MSG(chunksize != 16 && chunksize != 18, Ref<AudioStreamQOA>(), "Invalid 'fmt ' chunk.");

			//TODO: Add conversion from all other supported WAV formats to 16-bit PCM

			compression_code = file->get_16();
			if (compression_code != 1) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Format not supported by the QOA importer (not PCM).");
			}

			format_channels = file->get_16();
			if (format_channels != 1 && format_channels != 2) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Format not supported for WAVE file (not mono or stereo).");
			}

			format_freq = file->get_32();

			file->get_32();
			file->get_16();

			format_bits = file->get_16();

			if (format_bits != 16) {
				ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), "Invalid amount of bits for the QOA importer (only 16 is supported).");
			}

			if (chunksize == 18) {
				file->get_16(); // ignore extra parameters
			}

			format_found = true;
		}

		else if (chunkID[0] == 'd' && chunkID[1] == 'a' && chunkID[2] == 't' && chunkID[3] == 'a') {
			if (!format_found) {
				ERR_PRINT("'data' chunk before 'fmt ' chunk found.");
				break;
			}

			data_size = chunksize;
			break;
		}

		else {
			file->seek(file->get_position() + chunksize);
		}
	}

	ERR_FAIL_COND_V_MSG(data_size == 0, Ref<AudioStreamQOA>(), "No 'data' chunk found.");

	u_char *wav_data = (u_char *)memalloc(data_size);
	ERR_FAIL_COND_V_MSG(wav_data == nullptr, Ref<AudioStreamQOA>(), "Could not allocate memory for wav data.");
	file->get_buffer(wav_data, data_size);

	qoa_desc desc;

	desc.samplerate = format_freq;
	desc.samples = data_size / (format_channels * (format_bits / 8));
	desc.channels = format_channels;

	uint32_t size;
	void *encoded = qoa_encode((short *)wav_data, &desc, &size);

	Ref<AudioStreamQOA> qoa_stream;
	qoa_stream.instantiate();

	Vector<uint8_t> data;
	data.resize(size);
	memcpy(data.ptrw(), encoded, size);

	qoa_stream->set_data(data);

	return qoa_stream;
}

Error ResourceImporterQOA::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	bool loop = p_options["loop"];
	float loop_offset = p_options["loop_offset"];
	double bpm = p_options["bpm"];
	float beat_count = p_options["beat_count"];
	float bar_beats = p_options["bar_beats"];

	Ref<AudioStreamQOA> qoa_stream;
	if (p_source_file.ends_with(".qoa")) {
		qoa_stream = import_qoa(p_source_file);
	} else if (p_source_file.ends_with(".wav")) {
		qoa_stream = import_wav(p_source_file);
	}
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
