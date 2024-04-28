<img src="https://github.com/DeeJayLSP/godot-qoa-module/raw/master/media/AudioStreamQOALogo.svg" alt="AudioStreamQOALogo.svg" width=94/>

# Quite OK Audio for Godot
[QOA](https://qoaformat.org/) is a lossy audio format with cheap decoding, designed to have dozens playing at the same time, with better quality and a similar decoding cost to IMA-ADPCM. This makes it an ideal format for sound effects.

## Usage

This module enables you to use the QOA format in your project.

Importing follows two approaches:

### QOA files
Drop a QOA file into your project's folder and it will be imported as is.

### WAV files
- Click on a WAV file at the FileSystem dock, then go to the Import dock;
- On the Import As dropdown, where you see Microsoft WAV, choose Quite OK Audio.
- The import options are more or less the same as Microsoft WAV and will be carried over automatically if imported before;
- Reimport. The audio file will be encoded as QOA internally.

In both cases the audio will be imported as an AudioStreamQOA resource.

## Building

Clone the repository into `modules/`. Make sure the folder is named `qoa`;

Before building, make sure to apply a patch to enable audio previews for QOA in the inspector. Run this command at the root of the engine's source:
```bash
git apply ./modules/qoa/inspector_audio_preview.patch
```

Finally, build the engine as usual.

## Third-party
- [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) - The reference header with some patches;

> All files outside `thirdparty` are under the MIT license, under the copyright statement in [LICENSE](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/LICENSE). The file in `thirdparty` has its own license on it.
