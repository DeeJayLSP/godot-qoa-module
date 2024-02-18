#ifndef RESOURCE_IMPORTER_QOA_H
#define RESOURCE_IMPORTER_QOA_H

#include "audio_stream_qoa.h"

#include "core/io/resource_importer.h"

class ResourceImporterQOA : public ResourceImporter {
    GDCLASS(ResourceImporterQOA, ResourceImporter);

public:
	virtual String get_importer_name() const override;
	virtual String get_visible_name() const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual String get_save_extension() const override;
	virtual String get_resource_type() const override;

	virtual int get_preset_count() const override;
	virtual String get_preset_name(int p_idx) const override;

	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

#ifdef TOOLS_ENABLED
	virtual bool has_advanced_options() const override;
	virtual void show_advanced_options(const String &p_path) override;
#endif
    static Ref<AudioStreamQOA> import_qoa(const String &p_path);
	static Ref<AudioStreamQOA> import_wav(const String &p_path);

	virtual Error import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

    ResourceImporterQOA();
};

#endif // RESOURCE_IMPORTER_QOA_H
