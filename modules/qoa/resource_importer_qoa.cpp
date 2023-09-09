#include "resource_importer_qoa.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "scene/resources/texture.h"

#ifdef TOOLS_ENABLED
#include "editor/import/audio_stream_import_settings.h"
#endif

String ResourceImporterQOA::get_importer_name() const {
    return "qoa";
}

String ResourceImporterQOA::get_visible_name() const {
    return "QOA";
}

void ResourceImporterQOA::get_recognized_extensions(List<String> *p_extensions) const {
    p_extensions->push_back("qoa");
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
    Ref<AudioStreamQOA> qoa_stream = import_qoa(p_path);
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

Error ResourceImporterQOA::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
    bool loop = p_options["loop"];
    float loop_offset = p_options["loop_offset"];
    double bpm = p_options["bpm"];
    float beat_count = p_options["beat_count"];
    float bar_beats = p_options["bar_beats"];

    Ref<AudioStreamQOA> qoa_stream = import_qoa(p_source_file);
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
