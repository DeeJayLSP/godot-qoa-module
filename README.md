# Godot QOA Module
A module to import and play QOA files, just like its MP3 and Ogg Vorbis counterparts.

To use it, clone this repository into the engine source's `modules/` folder with the name `qoa` before building.

QOA is an audio format with cheap decoding, intended to have dozens playing at the same time, with better quality and a similar decoding cost to IMA-ADPCM.

This module uses the reference [qoa.h](https://github.com/phoboslab/qoa/blob/master/qoa.h) and borrows [qoaplay.c](https://github.com/raysan5/raudio/blob/master/src/external/qoaplay.c) from raudio, the only implementation with support for opening from memory. Both files contain some patches to remove what's not needed in this module;
