# libSceAudio3d

## change [libSceAudio3d](../../../core/libs/prx/libSceAudio3d/Export.cpp)

- [libSceAudio3d](../../../core/libs/prx/libSceAudio3d/Export.cpp) (libSceAudio3d) - `sceAudio3dObjectSetAttributes` makes the firmware 13.42 checks (count and array, port, attribute ids 1 to 11, PCM format and sample count, reserved object, value for every id except `RESET_STATE`) and validates `PCM` and `RESET_STATE` but applies nothing. Attribute ids whose PS5 meaning is unknown throw after those checks; the PS4 `GAIN`/`POSITION` meanings are not assumed. `sceAudio3dPortSetAttribute` checks late reverb level, downmix spread radius and height-aware value as firmware 13.42 does and ignores them, and `sceAudio3dPortFlush` drops frames queued by `sceAudio3dPortAdvance` instead of submitting them.
