#include "pc2ps3/win32_audio_output.hpp"
#include "win32_audio_output_internal.hpp"

#include <cassert>
#include <cstdint>

namespace {
struct FakeWaveOut {
  WAVEHDR* headers[8];
  std::uint32_t header_count;
  std::uint32_t open_calls;
  std::uint32_t prepare_calls;
  std::uint32_t write_calls;
  std::uint32_t reset_calls;
  std::uint32_t unprepare_calls;
  std::uint32_t close_calls;
  std::uint32_t fail_prepare_at;
  std::uint32_t fail_write_at;
  std::uint32_t fail_reset_attempts;
  std::uint32_t fail_unprepare_attempts;
  std::uint32_t fail_close_attempts;
  bool fail_open;

  FakeWaveOut()
      : headers(),
        header_count(0),
        open_calls(0),
        prepare_calls(0),
        write_calls(0),
        reset_calls(0),
        unprepare_calls(0),
        close_calls(0),
        fail_prepare_at(0),
        fail_write_at(0),
        fail_reset_attempts(0),
        fail_unprepare_attempts(0),
        fail_close_attempts(0),
        fail_open(false) {}
};

MMRESULT fake_open(void* context, HWAVEOUT* output,
                   const WAVEFORMATEX* format, HANDLE event) {
  FakeWaveOut* const fake = static_cast<FakeWaveOut*>(context);
  ++fake->open_calls;
  assert(format->wFormatTag == WAVE_FORMAT_PCM);
  assert(format->nChannels == 2 && format->wBitsPerSample == 16);
  assert(format->nSamplesPerSec == 44100 && format->nBlockAlign == 4);
  assert(event != 0);
  if (fake->fail_open) return WAVERR_BADFORMAT;
  *output = reinterpret_cast<HWAVEOUT>(static_cast<UINT_PTR>(1));
  return MMSYSERR_NOERROR;
}

MMRESULT fake_prepare(void* context, HWAVEOUT, WAVEHDR* header) {
  FakeWaveOut* const fake = static_cast<FakeWaveOut*>(context);
  ++fake->prepare_calls;
  if (fake->fail_prepare_at == fake->prepare_calls) return MMSYSERR_ERROR;
  assert(fake->header_count < 8);
  header->dwFlags |= WHDR_PREPARED;
  fake->headers[fake->header_count++] = header;
  return MMSYSERR_NOERROR;
}

MMRESULT fake_write(void* context, HWAVEOUT, WAVEHDR* header) {
  FakeWaveOut* const fake = static_cast<FakeWaveOut*>(context);
  ++fake->write_calls;
  if (fake->fail_write_at == fake->write_calls) return MMSYSERR_ERROR;
  header->dwFlags &= ~WHDR_DONE;
  header->dwFlags |= WHDR_INQUEUE;
  return MMSYSERR_NOERROR;
}

MMRESULT fake_reset(void* context, HWAVEOUT) {
  FakeWaveOut* const fake = static_cast<FakeWaveOut*>(context);
  ++fake->reset_calls;
  for (std::uint32_t i = 0; i < fake->header_count; ++i) {
    fake->headers[i]->dwFlags &= ~WHDR_INQUEUE;
    fake->headers[i]->dwFlags |= WHDR_DONE;
  }
  if (fake->fail_reset_attempts != 0) {
    --fake->fail_reset_attempts;
    return MMSYSERR_ERROR;
  }
  return MMSYSERR_NOERROR;
}

MMRESULT fake_unprepare(void* context, HWAVEOUT, WAVEHDR* header) {
  FakeWaveOut* const fake = static_cast<FakeWaveOut*>(context);
  ++fake->unprepare_calls;
  if (fake->fail_unprepare_attempts != 0) {
    --fake->fail_unprepare_attempts;
    return WAVERR_STILLPLAYING;
  }
  if ((header->dwFlags & WHDR_INQUEUE) != 0) return WAVERR_STILLPLAYING;
  header->dwFlags &= ~(WHDR_PREPARED | WHDR_DONE);
  return MMSYSERR_NOERROR;
}

MMRESULT fake_close(void* context, HWAVEOUT) {
  FakeWaveOut* const fake = static_cast<FakeWaveOut*>(context);
  ++fake->close_calls;
  if (fake->fail_close_attempts != 0) {
    --fake->fail_close_attempts;
    return MMSYSERR_ERROR;
  }
  return MMSYSERR_NOERROR;
}

pc2ps3::Win32WaveOutFunctions fake_functions(FakeWaveOut* fake) {
  pc2ps3::Win32WaveOutFunctions functions = {
      fake, &fake_open, &fake_prepare, &fake_write, &fake_reset,
      &fake_unprepare, &fake_close};
  return functions;
}

void test_initialize_recycle_and_shutdown() {
  FakeWaveOut fake;
  pc2ps3::Win32WaveOutFunctions functions = fake_functions(&fake);
  pc2ps3::AudioManager manager(44100, 4);
  pc2ps3::Win32AudioOutput output(&functions);
  assert(output.initialize(&manager, 64, 3));
  assert(output.active() && fake.write_calls == 3);
  assert(output.last_render_succeeded());

  fake.headers[1]->dwFlags &= ~WHDR_INQUEUE;
  fake.headers[1]->dwFlags |= WHDR_DONE;
  assert(output.pump());
  assert(fake.write_calls == 4);
  assert((fake.headers[1]->dwFlags & WHDR_INQUEUE) != 0);

  assert(output.shutdown());
  assert(!output.active() && fake.reset_calls == 1);
  assert(fake.unprepare_calls == 3 && fake.close_calls == 1);
  assert(output.shutdown());
  assert(fake.close_calls == 1);
}

void test_invalid_configuration_and_open_failure() {
  FakeWaveOut fake;
  pc2ps3::Win32WaveOutFunctions functions = fake_functions(&fake);
  pc2ps3::AudioManager manager(44100, 4);
  pc2ps3::Win32AudioOutput output(&functions);
  assert(!output.initialize(&manager, 32, 3));
  assert(fake.open_calls == 0);
  assert(!output.initialize(&manager, 64, 1));
  assert(fake.open_calls == 0);

  fake.fail_open = true;
  assert(!output.initialize(&manager, 64, 3));
  assert(!output.active());
  fake.fail_open = false;
  assert(output.initialize(&manager, 64, 3));
  assert(output.shutdown());
}

void test_prepare_and_initial_write_failures_cleanup() {
  pc2ps3::AudioManager manager(44100, 4);
  {
    FakeWaveOut fake;
    fake.fail_prepare_at = 2;
    pc2ps3::Win32WaveOutFunctions functions = fake_functions(&fake);
    pc2ps3::Win32AudioOutput output(&functions);
    assert(!output.initialize(&manager, 64, 3));
    assert(fake.reset_calls == 1 && fake.unprepare_calls == 1);
    assert(fake.close_calls == 1 && !output.active());
  }
  {
    FakeWaveOut fake;
    fake.fail_write_at = 2;
    pc2ps3::Win32WaveOutFunctions functions = fake_functions(&fake);
    pc2ps3::Win32AudioOutput output(&functions);
    assert(!output.initialize(&manager, 64, 3));
    assert(fake.reset_calls == 1 && fake.unprepare_calls == 3);
    assert(fake.close_calls == 1 && !output.active());
  }
}

void test_pump_write_error_and_shutdown_retries() {
  FakeWaveOut fake;
  pc2ps3::Win32WaveOutFunctions functions = fake_functions(&fake);
  pc2ps3::AudioManager manager(44100, 4);
  pc2ps3::Win32AudioOutput output(&functions);
  assert(output.initialize(&manager, 64, 3));
  fake.headers[0]->dwFlags &= ~WHDR_INQUEUE;
  fake.headers[0]->dwFlags |= WHDR_DONE;
  fake.fail_write_at = fake.write_calls + 1;
  assert(!output.pump());
  assert(output.last_error() == MMSYSERR_ERROR);

  fake.fail_unprepare_attempts = 1;
  assert(output.shutdown());
  assert(!output.active());
  assert(fake.unprepare_calls == 4);
  assert(output.shutdown());
  assert(fake.close_calls == 1);

  FakeWaveOut close_fake;
  pc2ps3::Win32WaveOutFunctions close_functions = fake_functions(&close_fake);
  pc2ps3::Win32AudioOutput close_output(&close_functions);
  assert(close_output.initialize(&manager, 64, 2));
  close_fake.fail_close_attempts = 1;
  assert(close_output.shutdown());
  assert(close_fake.close_calls == 2);
  assert(close_output.shutdown());
  assert(close_fake.close_calls == 2);

  FakeWaveOut persistent_close_fake;
  pc2ps3::Win32WaveOutFunctions persistent_close_functions =
      fake_functions(&persistent_close_fake);
  pc2ps3::Win32AudioOutput persistent_close_output(&persistent_close_functions);
  assert(persistent_close_output.initialize(&manager, 64, 2));
  persistent_close_fake.fail_close_attempts = 3;
  assert(!persistent_close_output.shutdown());
  assert(!persistent_close_output.active());
  assert(persistent_close_fake.close_calls == 3);
  persistent_close_fake.fail_close_attempts = 0;
  assert(persistent_close_output.shutdown());
  assert(persistent_close_fake.close_calls == 4);
  assert(persistent_close_output.shutdown());
  assert(persistent_close_fake.close_calls == 4);

  FakeWaveOut reset_fake;
  pc2ps3::Win32WaveOutFunctions reset_functions = fake_functions(&reset_fake);
  pc2ps3::Win32AudioOutput reset_output(&reset_functions);
  assert(reset_output.initialize(&manager, 64, 2));
  reset_fake.fail_reset_attempts = 1;
  assert(reset_output.shutdown());
  assert(reset_output.last_error() == MMSYSERR_ERROR);
  assert(reset_fake.close_calls == 1);

  FakeWaveOut retry_fake;
  pc2ps3::Win32WaveOutFunctions retry_functions = fake_functions(&retry_fake);
  pc2ps3::Win32AudioOutput retry_output(&retry_functions);
  assert(retry_output.initialize(&manager, 64, 2));
  retry_fake.fail_unprepare_attempts = 6;
  assert(!retry_output.shutdown());
  assert(!retry_output.active());
  retry_fake.fail_unprepare_attempts = 0;
  assert(retry_output.shutdown());
}
}  // namespace

int main() {
  test_initialize_recycle_and_shutdown();
  test_invalid_configuration_and_open_failure();
  test_prepare_and_initial_write_failures_cleanup();
  test_pump_write_error_and_shutdown_retries();
  return 0;
}
