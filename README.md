<img src="https://github.com/DeeJayLSP/godot-qoa-module/raw/master/editor/icons/AudioStreamQOA.svg" alt="AudioStreamQOA.svg" width=94/>

# Godot QOA Module
A module to import and play QOA files, just like the MP3 and Ogg Vorbis counterparts.

#### Why QOA?

Because it is an audio format suitable for performance, with better quality and a slighly more expensive decoding cost when compared to Godot's IMA-ADPCM, but still cheaper than other lossy codecs and ADPCM variants.

## Usage
- First, grab a copy of Godot's source code (4.0, newer, or a custom version based on those).

- Clone this repository into the source's `modules/` folder with the name `qoa`, then build the engine.

- Use QOA files on your project like you would with MP3 or Ogg.

> The binary size penalty from a minimal build should be in the range of 20-40KB depending on the platform you're building to.

## Third-party
This repository is also home for the [deqoa.h](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/thirdparty/deqoa.h) file, which is derived from the following:
- [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) - The reference header;
- [qoaplay.c](https://github.com/raysan5/raudio/blob/master/src/external/qoaplay.c) - A reference file modified for raudio to support opening and decoding from memory, which is required by Godot;

The resource icon (also at the top of this README) is an edit of [QOA's official logo](https://qoaformat.org/qoa-logo-new.svg) with Godot's AudioStream color scheme.

> All files are under the MIT license. Apart from deqoa.h, which has its own copyright statement, the one in [LICENSE](https://github.com/DeeJayLSP/godot-qoa-module/blob/master/LICENSE) should be considered.

## Notes
- Only Mono and Stereo audio files are fully supported. Channels other than L and R will be discarded by the decoder;
