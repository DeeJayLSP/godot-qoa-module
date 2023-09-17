<img src="https://github.com/DeeJayLSP/godot-qoa-module/raw/master/editor/icons/AudioStreamQOA.svg" alt="AudioStreamQOA.svg" width=94/>

# Godot QOA Module
A module to import and play QOA files, just like its MP3 and Ogg Vorbis counterparts.

### Usage
Clone this repository into the engine source's `modules/` folder with the name `qoa` before building. Then use QOA files like you would with MP3 or Ogg.

QOA is an audio format with cheap decoding, intended to have dozens playing at the same time, with better quality and a similar decoding cost to IMA-ADPCM.

### Third-party
This repository is also home for the [deqoa.h](https://github.com/DeeJayLSP/blob/master/thirdparty/deqoa.h) file, which is derived from the following:
- [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) - The reference header;
- [qoaplay.c](https://github.com/raysan5/raudio/blob/master/src/external/qoaplay.c) - A reference file modified for raudio to support opening and decoding from memory, which is required by Godot;

> All files are under the MIT license. Apart from deqoa.h, which has its own license, the copyright statement in [LICENSE](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/LICENSE) should be considered.

### Notes
- Only Mono and Stereo audio files are fully supported. Channels other than L and R will be discarded;
