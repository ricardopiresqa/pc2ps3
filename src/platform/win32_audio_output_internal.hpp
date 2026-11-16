#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include "pc2ps3/audio.hpp"

#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct Win32WaveOutFunctions {
  void* context;
  MMRESULT (*open)(void* context, HWAVEOUT* output,
                   const WAVEFORMATEX* format, HANDLE event);
  MMRESULT (*prepare)(void* context, HWAVEOUT output, WAVEHDR* header);
  MMRESULT (*write)(void* context, HWAVEOUT output, WAVEHDR* header);
  MMRESULT (*reset)(void* context, HWAVEOUT output);
  MMRESULT (*unprepare)(void* context, HWAVEOUT output, WAVEHDR* header);
  MMRESULT (*close)(void* context, HWAVEOUT output);
};

struct Win32AudioOutputBuffer {
  std::vector<std::int16_t> samples;
  WAVEHDR header;
  bool prepared;

  Win32AudioOutputBuffer() : samples(), header(), prepared(false) {
    ZeroMemory(&header, sizeof(header));
  }
};

struct Win32AudioOutputState {
  AudioManager* manager;
  HWAVEOUT device;
  HANDLE event;
  Win32AudioOutputBuffer* buffers;
  std::uint32_t queue_depth;
  std::uint32_t block_frames;
  DWORD owner_thread;
  bool active;

  Win32AudioOutputState()
      : manager(0),
        device(0),
        event(0),
        buffers(0),
        queue_depth(0),
        block_frames(0),
        owner_thread(0),
        active(false) {}
};

const Win32WaveOutFunctions& native_win32_wave_out_functions();

}  // namespace pc2ps3
