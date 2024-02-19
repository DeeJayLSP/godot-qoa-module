<img src="https://github.com/DeeJayLSP/godot-qoa-module/raw/master/media/AudioStreamQOALogo.svg" alt="AudioStreamQOALogo.svg" width=94/>

# Quite OK Audio for Godot
[QOA](https://qoaformat.org/) is a lossy audio format with cheap decoding, designed to have dozens playing at the same time, with better quality and a similar decoding cost to IMA-ADPCM. This makes it an ideal format for sound effects.

### Usage
Clone this repository into the engine source's `modules/` folder with the name `qoa` before building.

This enables you to use QOA files in your project like you would with MP3 or Ogg.

You can import WAV files as QOA:
- Click on a WAV file at the FileSystem dock, then go to the Import dock;
- On the Import As dropdown, where you see Microsoft WAV, choose Quite OK Audio;
- Reimport. The audio file will be encoded as QOA internally and on exports.

In both cases the audio will be imported as AudioStreamQOA.

### Third-party
The [qoa.h](https://github.com/DeeJayLSP/blob/master/thirdparty/qoa.h) file is actually a mix of the following:
- [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) - The reference header;
- [qoaplay.c](https://github.com/phoboslab/qoa/blob/master/qoaplay.c) - The reference player, modified for Godot;

> All files outside `thirdparty` are under the MIT license, under the copyright statement in [LICENSE](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/LICENSE). The file in `thirdparty` has its own license on it.
