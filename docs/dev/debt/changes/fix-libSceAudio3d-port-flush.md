# libSceAudio3d

## change [libSceAudio3d](../../../core/libs/prx/libSceAudio3d/Export.cpp)

- [libSceAudio3d](../../../core/libs/prx/libSceAudio3d/Export.cpp) (libSceAudio3d) - `sceAudio3dPortSetAttribute` checks the late reverb level (0 to 1), the downmix spread radius (0.1 to 2) and the height-aware value as firmware 13.42's AudioOut2 (libSceAudioOut.sprx) does when Audio3d passes them on, and ignores them

## add Functional

- [sceAudio3dPortFlush](../../../core/libs/prx/libSceAudio3d/Export.cpp) (libSceAudio3d) - with registered AudioOut handles it clears their pending blocks and null-outputs each handle; otherwise it advances once when the mixed queue is empty and a bed is queued, then submits every queued mixed frame through the Audio3d output, returning output errors and restoring a frame whose submission fails. A final null output drains the host device; console behavior for that extra drain is unmeasured. Advancing inside flush uses a no-report helper so report handlers can call flush without recursive report-lock acquisition, and failed frames are restored only while the original port generation is open. Follows public shadPS4 PS4 behavior at `e16a59be`; PS5 firmware/AudioOut2 behavior is unmeasured.
