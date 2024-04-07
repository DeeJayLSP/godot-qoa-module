#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO

#include "resource_importer_qoa.h"

#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"

const float TRIM_DB_LIMIT = -50;
const int TRIM_FADE_OUT_FRAMES = 500;

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
	if (check_qoa(p_path)) {
		// These options only apply for WAV files.
		if (p_option == "force/mono" || p_option == "force/max_rate" || p_option == "force/max_rate_hz" || p_option == "edit/trim" || p_option == "edit/normalize") {
			return false;
		}
	}

	if (p_option == "force/max_rate_hz" && !bool(p_options["force/max_rate"])) {
		return false;
	}

	// Don't show begin/end loop points if loop mode is auto-detected or disabled.
	if ((int)p_options["edit/loop_mode"] < 2 && (p_option == "edit/loop_begin" || p_option == "edit/loop_end")) {
		return false;
	}

	return true;
}

int ResourceImporterQOA::get_preset_count() const {
	return 0;
}

String ResourceImporterQOA::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterQOA::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/mono"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/max_rate", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "force/max_rate_hz", PROPERTY_HINT_RANGE, "11025,192000,1,exp"), 44100));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "edit/trim"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "edit/normalize"), false));
	// Keep the `edit/loop_mode` enum in sync with AudioStreamQOA::LoopMode (note: +1 offset due to "Detect From WAV").
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_mode", PROPERTY_HINT_ENUM, "Detect From WAV,Disabled,Forward,Ping-Pong,Backward", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_begin"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_end"), -1));
}

Ref<AudioStreamQOA> ResourceImporterQOA::import_qoa(const String &p_path, const HashMap<StringName, Variant> &p_options) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(file.is_null(), Ref<AudioStreamQOA>());

	int import_loop_mode = p_options["edit/loop_mode"];
	AudioStreamQOA::LoopMode loop_mode = AudioStreamQOA::LOOP_DISABLED;
	int loop_begin = import_loop_mode > 1 ? (int)p_options["edit/loop_begin"] : 0;
	int loop_end = import_loop_mode > 1 ? (int)p_options["edit/loop_end"] : 0;
	int frames = 0;

	Vector<uint8_t> qoa_data;
	uint32_t len;

	if (check_qoa(p_path)) {
		if (import_loop_mode > 1) {
			loop_mode = (AudioStreamQOA::LoopMode)(import_loop_mode - 1);
		}
		file->seek(0);
		len = file->get_length();
		qoa_data.resize(len);
		uint8_t *w = qoa_data.ptrw();

		file->get_buffer(w, len);

		qoa_desc qd;
		qd.samples = 0;
		qoa_decode_header(w, len, &qd);
		frames = qd.samples;

	} else { // WAV
		char riff[5];
		riff[4] = 0;
		file->get_buffer((uint8_t *)&riff, 4); //RIFF

		if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') {
			ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), vformat("Not a WAV file. File should start with 'RIFF', but found '%s', in file of size %d bytes.", riff, file->get_length()));
		}

		file->get_32();

		char wave[5];
		wave[4] = 0;
		file->get_buffer((uint8_t *)&wave, 4); //WAVE

		if (wave[0] != 'W' || wave[1] != 'A' || wave[2] != 'V' || wave[3] != 'E') {
			ERR_FAIL_V_MSG(Ref<AudioStreamQOA>(), vformat("Not a WAV file. File should start with 'WAVE', but found '%s', in file of size %d bytes.", wave, file->get_length()));
		}

		// Let users override potential loop points from the WAV.
		// We parse the WAV loop points only with "Detect From WAV" (0).

		int format_bits = 0;
		int format_channels = 0;

		uint16_t compression_code = 1;
		bool format_found = false;
		bool data_found = false;
		int format_freq = 0;

		Vector<float> data;

		while (!file->eof_reached()) {
			/* chunk */
			char chunk_id[4];
			file->get_buffer((uint8_t *)&chunk_id, 4); //RIFF

			/* chunk size */
			uint32_t chunksize = file->get_32();
			uint32_t file_pos = file->get_position(); //save file pos, so we can skip to next chunk safely

			if (file->eof_reached()) {
				//ERR_PRINT("EOF REACH");
				break;
			}

			if (chunk_id[0] == 'f' && chunk_id[1] == 'm' && chunk_id[2] == 't' && chunk_id[3] == ' ' && !format_found) {
				/* IS FORMAT CHUNK */

				//Issue: #7755 : Not a bug - usage of other formats (format codes) are unsupported in current importer version.
				//Consider revision for engine version 3.0
				compression_code = file->get_16();
				ERR_FAIL_COND_V_MSG(compression_code != 1 && compression_code != 3, Ref<AudioStreamQOA>(), "Format not supported for WAVE file (not PCM). Save WAVE files as uncompressed PCM or IEEE float instead.");

				format_channels = file->get_16();
				ERR_FAIL_COND_V_MSG(format_channels != 1 && format_channels != 2, Ref<AudioStreamQOA>(), "Format not supported for WAVE file (not stereo or mono).");

				format_freq = file->get_32(); //sampling rate

				file->get_32(); // average bits/second (unused)
				file->get_16(); // block align (unused)
				format_bits = file->get_16(); // bits per sample
				ERR_FAIL_COND_V_MSG(format_bits % 8 || format_bits == 0, Ref<AudioStreamQOA>(), "Invalid amount of bits in the sample (should be one of 8, 16, 24 or 32).");
				ERR_FAIL_COND_V_MSG(compression_code == 3 && format_bits % 32, Ref<AudioStreamQOA>(), "Invalid amount of bits in the IEEE float sample (should be 32 or 64).");

				/* Don't need anything else, continue */
				format_found = true;
			}

			if (chunk_id[0] == 'd' && chunk_id[1] == 'a' && chunk_id[2] == 't' && chunk_id[3] == 'a' && !data_found) {
				/* IS DATA CHUNK */
				data_found = true;

				ERR_BREAK_MSG(!format_found, "'data' chunk before 'format' chunk found.");

				frames = chunksize;

				ERR_FAIL_COND_V(format_channels == 0, Ref<AudioStreamQOA>());

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
				ERR_FAIL_COND_V_MSG(file->eof_reached(), Ref<AudioStreamQOA>(), "Premature end of file.");
			}

			if (import_loop_mode == 0 && chunk_id[0] == 's' && chunk_id[1] == 'm' && chunk_id[2] == 'p' && chunk_id[3] == 'l') {
				// Loop point info!

				/**
				 *	Consider exploring next document:
				 *		http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/RIFFNEW.pdf
				 *	Especially on page:
				 *		16 - 17
				 *	Timestamp:
				 *		22:38 06.07.2017 GMT
				 **/

				for (int i = 0; i < 10; i++) {
					file->get_32(); // i wish to know why should i do this... no doc!
				}

				// only read 0x00 (loop forward), 0x01 (loop ping-pong) and 0x02 (loop backward)
				// Skip anything else because it's not supported, reserved for future uses or sampler specific
				// from https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#smpl (loop type values table)
				int loop_type = file->get_32();
				if (loop_type == 0x00 || loop_type == 0x01 || loop_type == 0x02) {
					if (loop_type == 0x00) {
						loop_mode = AudioStreamQOA::LOOP_FORWARD;
					} else if (loop_type == 0x01) {
						loop_mode = AudioStreamQOA::LOOP_PINGPONG;
					} else if (loop_type == 0x02) {
						loop_mode = AudioStreamQOA::LOOP_BACKWARD;
					}
					loop_begin = file->get_32();
					loop_end = file->get_32();
				}
			}

			file->seek(file_pos + chunksize + (chunksize & 1));
		}

		bool limit_rate = p_options["force/max_rate"];
		int limit_rate_hz = p_options["force/max_rate_hz"];
		if (limit_rate && format_freq > limit_rate_hz && format_freq > 0 && frames > 0) {
			// resample!
			int new_data_frames = (int)(frames * (float)limit_rate_hz / format_freq);

			Vector<float> new_data;
			new_data.resize(new_data_frames * format_channels);
			for (int c = 0; c < format_channels; c++) {
				float frac = .0f;
				int ipos = 0;

				for (int i = 0; i < new_data_frames; i++) {
					// Cubic interpolation should be enough.

					float y0 = data[MAX(0, ipos - 1) * format_channels + c];
					float y1 = data[ipos * format_channels + c];
					float y2 = data[MIN(frames - 1, ipos + 1) * format_channels + c];
					float y3 = data[MIN(frames - 1, ipos + 2) * format_channels + c];

					new_data.write[i * format_channels + c] = Math::cubic_interpolate(y1, y2, y0, y3, frac);

					// update position and always keep fractional part within ]0...1]
					// in order to avoid 32bit floating point precision errors

					frac += (float)format_freq / limit_rate_hz;
					int tpos = (int)Math::floor(frac);
					ipos += tpos;
					frac -= tpos;
				}
			}

			if (loop_mode) {
				loop_begin = (int)(loop_begin * (float)new_data_frames / frames);
				loop_end = (int)(loop_end * (float)new_data_frames / frames);
			}

			data = new_data;
			format_freq = limit_rate_hz;
			frames = new_data_frames;
		}

		bool normalize = p_options["edit/normalize"];

		if (normalize) {
			float max = 0;
			for (int i = 0; i < data.size(); i++) {
				float amp = Math::abs(data[i]);
				if (amp > max) {
					max = amp;
				}
			}

			if (max > 0) {
				float mult = 1.0 / max;
				for (int i = 0; i < data.size(); i++) {
					data.write[i] *= mult;
				}
			}
		}

		bool trim = p_options["edit/trim"];

		if (trim && (loop_mode == AudioStreamQOA::LOOP_DISABLED) && format_channels > 0) {
			int first = 0;
			int last = (frames / format_channels) - 1;
			bool found = false;
			float limit = Math::db_to_linear(TRIM_DB_LIMIT);

			for (int i = 0; i < data.size() / format_channels; i++) {
				float amp_channel_sum = 0;
				for (int j = 0; j < format_channels; j++) {
					amp_channel_sum += Math::abs(data[(i * format_channels) + j]);
				}

				float amp = Math::abs(amp_channel_sum / format_channels);

				if (!found && amp > limit) {
					first = i;
					found = true;
				}

				if (found && amp > limit) {
					last = i;
				}
			}

			if (first < last) {
				Vector<float> new_data;
				new_data.resize((last - first) * format_channels);
				for (int i = first; i < last; i++) {
					float fade_out_mult = 1;

					if (last - i < TRIM_FADE_OUT_FRAMES) {
						fade_out_mult = ((float)(last - i - 1) / (float)TRIM_FADE_OUT_FRAMES);
					}

					for (int j = 0; j < format_channels; j++) {
						new_data.write[((i - first) * format_channels) + j] = data[(i * format_channels) + j] * fade_out_mult;
					}
				}

				data = new_data;
				frames = data.size() / format_channels;
			}
		}

		bool force_mono = p_options["force/mono"];

		if (force_mono && format_channels == 2) {
			Vector<float> new_data;
			new_data.resize(data.size() / 2);
			for (int i = 0; i < frames; i++) {
				new_data.write[i] = (data[i * 2 + 0] + data[i * 2 + 1]) / 2.0;
			}

			data = new_data;
			format_channels = 1;
		}

		Vector<uint8_t> wav_data;
		// Enforce 16 bit samples
		wav_data.resize(data.size() * 2);
		{
			uint8_t *w = wav_data.ptrw();

			int ds = data.size();
			for (int i = 0; i < ds; i++) {
				int16_t v = CLAMP(data[i] * 32768, -32768, 32767);
				encode_uint16(v, &w[i * 2]);
			}
		}

		qoa_desc desc;

		desc.samplerate = format_freq;
		desc.samples = frames;
		desc.channels = format_channels;

		void *encoded = qoa_encode((short *)wav_data.ptrw(), &desc, &len);

		qoa_data.resize(len);
		memcpy(qoa_data.ptrw(), encoded, len);
	}

	if (import_loop_mode >= 2) {
		loop_mode = (AudioStreamQOA::LoopMode)(import_loop_mode - 1);
		// Wrap around to max frames, so `-1` can be used to select the end, etc.
		if (loop_begin < 0) {
			loop_begin = CLAMP(loop_begin + frames + 1, 0, frames);
		}
		if (loop_end < 0) {
			loop_end = CLAMP(loop_end + frames + 1, 0, frames);
		}
	}

	Ref<AudioStreamQOA> qoa_stream;
	qoa_stream.instantiate();

	qoa_stream->set_data(qoa_data);
	qoa_stream->set_loop_mode(loop_mode);
	qoa_stream->set_loop_begin(loop_begin);
	qoa_stream->set_loop_end(loop_end);
	ERR_FAIL_COND_V(qoa_stream->get_data().is_empty(), Ref<AudioStreamQOA>());

	return qoa_stream;
}

Error ResourceImporterQOA::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Ref<AudioStreamQOA> qoa_stream = import_qoa(p_source_file, p_options);
	if (qoa_stream.is_null()) {
		return ERR_CANT_OPEN;
	}

	return ResourceSaver::save(qoa_stream, p_save_path + ".qoastr");
}

ResourceImporterQOA::ResourceImporterQOA() {
}
