<img src="https://github.com/DeeJayLSP/godot-qoa-module/raw/master/editor/icons/AudioStreamQOA.svg" alt="AudioStreamQOA.svg" width=94/>

# Godot QOA Module
A module to import and play QOA files, just like its MP3 and Ogg Vorbis counterparts.

### Usage
Clone this repository into the engine source's `modules/` folder with the name `qoa` before building. Then use QOA files like you would with MP3 or Ogg.

Or even better: click on a WAV file, change its Import As from Microsoft WAV to Quite OK Audio and reimport. The file will be QOA internally.

QOA is an audio format with cheap decoding, intended to have dozens playing at the same time, with better quality and a similar decoding cost to IMA-ADPCM.

### Third-party
The [qoa.h](https://github.com/DeeJayLSP/blob/master/thirdparty/deqoa.h) file is actually a modified mix of the the following:
- [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) - The reference header;
- [qoaplay.c](https://github.com/phoboslab/qoa/blob/master/qoaplay.c) - The reference player, modified for Godot;

> All files outside `thirdparty` are under the MIT license, under the copyright statement in [LICENSE](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/LICENSE). The file in `thirdparty` has its license on it.
