#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include "pc2ps3/audio_callback.hpp"
#include "pc2ps3/win32_audio_output.hpp"
#include "win32_audio_output_internal.hpp"

#include <new>

namespace pc2ps3 {
namespace {

MMRESULT native_open(void*, HWAVEOUT* output, const WAVEFORMATEX* format,
                     HANDLE event) {
  return waveOutOpen(output, WAVE_MAPPER, format,
                     reinterpret_cast<DWORD_PTR>(event), 0, CALLBACK_EVENT);
}

MMRESULT native_prepare(void*, HWAVEOUT output, WAVEHDR* header) {
  return waveOutPrepareHeader(output, header,
                              static_cast<UINT>(sizeof(*header)));
}

MMRESULT native_write(void*, HWAVEOUT output, WAVEHDR* header) {
  return waveOutWrite(output, header, static_cast<UINT>(sizeof(*header)));
}

MMRESULT native_reset(void*, HWAVEOUT output) { return waveOutReset(output); }

MMRESULT native_unprepare(void*, HWAVEOUT output, WAVEHDR* header) {
  return waveOutUnprepareHeader(output, header,
                                static_cast<UINT>(sizeof(*header)));
}

MMRESULT native_close(void*, HWAVEOUT output) { return waveOutClose(output); }

bool valid_functions(const Win32WaveOutFunctions& functions) {
  return functions.open != 0 && functions.prepare != 0 &&
         functions.write != 0 && functions.reset != 0 &&
         functions.unprepare != 0 && functions.close != 0;
}

}  // namespace

const Win32WaveOutFunctions& native_win32_wave_out_functions() {
  static const Win32WaveOutFunctions functions = {
      0, &native_open, &native_prepare, &native_write, &native_reset,
      &native_unprepare, &native_close};
  return functions;
}

Win32AudioOutput::Win32AudioOutput(const Win32WaveOutFunctions* functions)
    : functions_(functions),
      state_(0),
      last_error_(0),
      last_render_succeeded_(true) {}

Win32AudioOutput::~Win32AudioOutput() noexcept {
  for (int attempt = 0; state_ != 0 && attempt < 3; ++attempt) {
    if (shutdown()) return;
    Sleep(1);
  }
  if (state_ != 0) {
    // Keep prepared headers alive if a broken driver refuses reset/unprepare.
    state_ = 0;
  }
}

bool Win32AudioOutput::initialize(AudioManager* manager,
                                  std::uint32_t block_frames,
                                  std::uint32_t queue_depth) {
  last_error_ = 0;
  last_render_succeeded_ = true;
  if (state_ != 0) {
    last_error_ = ERROR_BUSY;
    return false;
  }
  if (manager == 0 || !manager->valid() || manager->output_rate() < 8000 ||
      manager->output_rate() > 192000 || block_frames < 64 ||
      block_frames > 4096 || queue_depth < 2 || queue_depth > 8) {
    last_error_ = ERROR_INVALID_PARAMETER;
    return false;
  }
  const Win32WaveOutFunctions& functions =
      functions_ == 0 ? native_win32_wave_out_functions() : *functions_;
  if (!valid_functions(functions)) {
    last_error_ = ERROR_PROC_NOT_FOUND;
    return false;
  }

  Win32AudioOutputState* state = new (std::nothrow) Win32AudioOutputState();
  if (state == 0) {
    last_error_ = ERROR_NOT_ENOUGH_MEMORY;
    return false;
  }
  state_ = state;
  state->manager = manager;
  state->queue_depth = queue_depth;
  state->block_frames = block_frames;
  state->owner_thread = GetCurrentThreadId();
  state->buffers =
      new (std::nothrow) Win32AudioOutputBuffer[queue_depth];
  if (state->buffers == 0) {
    last_error_ = ERROR_NOT_ENOUGH_MEMORY;
    shutdown();
    return false;
  }
  try {
    for (std::uint32_t i = 0; i < queue_depth; ++i) {
      state->buffers[i].samples.resize(
          static_cast<std::size_t>(block_frames) * 2u);
      WAVEHDR& header = state->buffers[i].header;
      header.lpData = reinterpret_cast<LPSTR>(state->buffers[i].samples.data());
      header.dwBufferLength = block_frames * 4u;
      header.dwUser = i;
    }
  } catch (const std::bad_alloc&) {
    last_error_ = ERROR_NOT_ENOUGH_MEMORY;
    shutdown();
    return false;
  }

  state->event = CreateEventW(0, FALSE, FALSE, 0);
  if (state->event == 0) {
    last_error_ = GetLastError();
    shutdown();
    return false;
  }

  WAVEFORMATEX format = {};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = manager->output_rate();
  format.wBitsPerSample = 16;
  format.nBlockAlign = 4;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  const MMRESULT open_result = functions.open(
      functions.context, &state->device, &format, state->event);
  if (open_result != MMSYSERR_NOERROR || state->device == 0) {
    last_error_ = open_result == MMSYSERR_NOERROR ? MMSYSERR_BADDEVICEID
                                                   : open_result;
    shutdown();
    return false;
  }

  for (std::uint32_t i = 0; i < queue_depth; ++i) {
    const MMRESULT result = functions.prepare(
        functions.context, state->device, &state->buffers[i].header);
    if (result != MMSYSERR_NOERROR) {
      last_error_ = result;
      shutdown();
      return false;
    }
    state->buffers[i].prepared = true;
  }

  for (std::uint32_t i = 0; i < queue_depth; ++i) {
    Win32AudioOutputBuffer& buffer = state->buffers[i];
    last_render_succeeded_ = render_audio_callback(
        manager, buffer.samples.data(), block_frames, block_frames);
    const MMRESULT result = functions.write(
        functions.context, state->device, &buffer.header);
    if (result != MMSYSERR_NOERROR) {
      last_error_ = result;
      shutdown();
      return false;
    }
  }
  state->active = true;
  return true;
}

bool Win32AudioOutput::pump() {
  last_error_ = 0;
  if (state_ == 0 || !state_->active) {
    last_error_ = ERROR_INVALID_STATE;
    return false;
  }
  if (GetCurrentThreadId() != state_->owner_thread) {
    last_error_ = ERROR_INVALID_THREAD_ID;
    return false;
  }
  const DWORD event_status = WaitForSingleObject(state_->event, 0);
  if (event_status == WAIT_FAILED) {
    last_error_ = GetLastError();
    return false;
  }
  if (event_status != WAIT_OBJECT_0 && event_status != WAIT_TIMEOUT) {
    last_error_ = ERROR_INVALID_STATE;
    return false;
  }

  const Win32WaveOutFunctions& functions =
      functions_ == 0 ? native_win32_wave_out_functions() : *functions_;
  bool success = true;
  last_render_succeeded_ = true;
  for (std::uint32_t i = 0; i < state_->queue_depth; ++i) {
    Win32AudioOutputBuffer& buffer = state_->buffers[i];
    if ((buffer.header.dwFlags & WHDR_DONE) == 0) continue;
    last_render_succeeded_ = render_audio_callback(
        state_->manager, buffer.samples.data(), state_->block_frames,
        state_->block_frames);
    if (!last_render_succeeded_) success = false;
    const MMRESULT result = functions.write(
        functions.context, state_->device, &buffer.header);
    if (result != MMSYSERR_NOERROR) {
      last_error_ = result;
      return false;
    }
  }
  return success;
}

bool Win32AudioOutput::shutdown() noexcept {
  if (state_ == 0) return true;
  Win32AudioOutputState* const state = state_;
  state->active = false;
  const Win32WaveOutFunctions& functions =
      functions_ == 0 ? native_win32_wave_out_functions() : *functions_;

  if (state->device != 0) {
    bool all_unprepared = false;
    for (unsigned int attempt = 0; attempt < 3; ++attempt) {
      const MMRESULT reset_result =
          functions.reset(functions.context, state->device);
      if (reset_result != MMSYSERR_NOERROR && last_error_ == 0) {
        last_error_ = reset_result;
      }
      bool still_prepared = false;
      for (std::uint32_t i = 0; i < state->queue_depth; ++i) {
        Win32AudioOutputBuffer& buffer = state->buffers[i];
        if (!buffer.prepared) continue;
        const MMRESULT result = functions.unprepare(
            functions.context, state->device, &buffer.header);
        if (result == MMSYSERR_NOERROR) {
          buffer.prepared = false;
        } else {
          if (last_error_ == 0) last_error_ = result;
          still_prepared = true;
        }
      }
      if (!still_prepared) {
        all_unprepared = true;
        break;
      }
      if (state->event != 0 && attempt < 2) {
        WaitForSingleObject(state->event, 50);
      }
    }
    if (!all_unprepared) return false;

    bool closed = false;
    for (unsigned int attempt = 0; attempt < 3; ++attempt) {
      const MMRESULT close_result =
          functions.close(functions.context, state->device);
      if (close_result == MMSYSERR_NOERROR) {
        closed = true;
        break;
      }
      if (last_error_ == 0) last_error_ = close_result;
      if (attempt < 2) Sleep(1);
    }
    if (!closed) return false;
    state->device = 0;
  }

  delete[] state->buffers;
  state->buffers = 0;
  if (state->event != 0) {
    if (!CloseHandle(state->event)) {
      if (last_error_ == 0) last_error_ = GetLastError();
      return false;
    }
    state->event = 0;
  }
  state_ = 0;
  delete state;
  return true;
}

bool Win32AudioOutput::active() const noexcept {
  return state_ != 0 && state_->active;
}

bool Win32AudioOutput::last_render_succeeded() const noexcept {
  return last_render_succeeded_;
}

std::uint32_t Win32AudioOutput::last_error() const noexcept {
  return last_error_;
}

}  // namespace pc2ps3
