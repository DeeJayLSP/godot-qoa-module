# Godot QOA Module
A module to import and play QOA files, just like its MP3 and Ogg Vorbis counterparts.

### Usage
Clone this repository into the engine source's `modules/` folder with the name `qoa` before building. Then use QOA files like you would with MP3 or Ogg.

QOA is an audio format with cheap decoding, intended to have dozens playing at the same time, with better quality and a similar decoding cost to IMA-ADPCM.

### Third-party
These are the third-party files used in this module:
- [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) - The reference header;
- [qoaplay.c](https://github.com/raysan5/raudio/blob/master/src/external/qoaplay.c) - A reference file with decoding functions modified for raudio to support opening from memory;

Both files above contain some patches to remove what's not needed in this module.

> All files are under the MIT license. Apart from thirdparty, the copyright statement in [LICENSE](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/LICENSE) should be considered.

### Notes
- Only Mono and Stereo audio files are guaranteed to work;
