#include "common.h"

#include "apu.h"
#include "audio.h"
#include "blip_buf.h"
#include "sdl_backend.h"
#include "timing.h"

// We try to keep the internal audio buffer 50% full for maximum protection
// against under- and overflow. To maintain that level, we adjust the playback
// rate slightly depending on the current buffer fill level. This sets the
// maximum adjustment allowed (1.5%), though typical adjustments will be much
// smaller.
double const max_adjust = 0.015;

// To avoid an immediate underflow, we wait for the audio buffer to fill up
// before we start playing. This is set true when we're happy with the fill
// level.
static bool playback_started;

// Offset in CPU cycles within the current frame
static unsigned audio_frame_offset;

static blip_t *blip;

// Leave some extra room in the buffer to allow audio to be slowed down.
// (The expression is equivalent to 1.3*sample_rate/frames_per_second, but a
// compile-time constant in C++03. TODO: Make dependent on max_adjust.)
static int16_t blip_samples[1300*sample_rate/milliframes_per_second];

void end_audio_frame() {
    if (audio_frame_offset == 0)
        // No audio added; blip_end_frame() dislikes being called with an
        // offset of 0
        return;

    blip_end_frame(blip, audio_frame_offset);
    audio_frame_offset = 0;

    if (playback_started) {
        // Fudge playback rate by an amount proportional to the difference
        // between the desired and current buffer fill levels to try to steer
        // towards it

        double const fudge_factor = 1.0 + 2*max_adjust*(0.5 - audio_buf_fill_level());
        blip_set_rates(blip, ntsc_cpu_clock_rate, sample_rate*fudge_factor);
    }
    else {
        if (audio_buf_fill_level() >= 0.5) {
            start_audio_playback();
            playback_started = true;
        }
    }

    int const n_samples = blip_read_samples(blip, blip_samples, ARRAY_LEN(blip_samples), 0);
    // We expect to read all samples from blip_buf. If something goes wrong and
    // we don't, clear the buffer to prevent data piling up in blip_buf's
    // buffer (which lacks bounds checking).
    int const avail = blip_samples_avail(blip);
    if (avail != 0) {
        printf("Warning: didn't read all samples from blip_buf (%d samples remain) - dropping samples\n",
          avail);
        blip_clear(blip);
    }
    add_audio_samples(blip_samples, n_samples);
}

void set_audio_signal_level(int16_t level) {
    // TODO: Do something to reduce the initial pop here?
    static int16_t previous_signal_level;
    blip_add_delta(blip, audio_frame_offset, level - previous_signal_level);
    previous_signal_level = level;
}

void tick_audio() { ++audio_frame_offset; }


void init_audio() {
    // Maximum number of unread samples the buffer can hold
    blip = blip_new(sample_rate/10);
    blip_set_rates(blip, ntsc_cpu_clock_rate, sample_rate);
}

void deinit_audio() {
    blip_delete(blip);
}
