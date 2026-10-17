/*
 * Banana OS Audio Driver for DOOM (doomgeneric)
 *
 * Implements real-time multi-channel PCM software sound mixing
 * directly into the Banana OS kernel DMA audio queue via banana_play().
 */

#include "config.h"
#include "doomfeatures.h"
#include "doomtype.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "deh_str.h"
#include "sounds.h"
#include "m_misc.h"
#include "m_argv.h"
#include "oplplayer.h"
#include <banana.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BANANA_NUM_CHANNELS 16
#define BANANA_AUDIO_RATE   48000
#define BANANA_SAMPLES_PER_TIC (BANANA_AUDIO_RATE / 35) /* ~1371 samples per tic */

typedef struct {
    const uint8_t* data;
    uint32_t length;
    uint32_t rate;
} sfx_cache_t;

typedef struct {
    sfx_cache_t* sfx;
    uint64_t pos_fp;      /* 16.16 fixed point playback index */
    uint64_t step_fp;     /* 16.16 fixed point step per output sample */
    int vol;              /* 0..127 */
    int sep;              /* 0..254 (128 = center) */
    int active;
} banana_channel_t;

static banana_channel_t s_channels[BANANA_NUM_CHANNELS];
static boolean s_sound_initialized = false;
static boolean s_use_sfx_prefix = true;

int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

/* Buffer for mixing (35 tics/sec at 48000 Hz = max 1400 samples/tic) */
static int32_t s_mix_l[1440];
static int32_t s_mix_r[1440];
static int16_t s_pcm_out[1440 * 2];
static int16_t s_music_buf[1440 * 2];
static uint32_t s_frac_acc = 0;

static boolean s_music_initialized = false;
static boolean s_music_playing = false;
static int s_music_volume = 127;
static void* s_current_song = NULL;

static snddevice_t sound_banana_devices[] = {
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
    SNDDEVICE_PCSPEAKER,
};

static snddevice_t music_banana_devices[] = {
    SNDDEVICE_GENMIDI,
    SNDDEVICE_SB,
    SNDDEVICE_GUS,
};

/* ── SFX Caching ────────────────────────────────────────────────── */

static sfx_cache_t* CacheSFX(sfxinfo_t* sfxinfo) {
    if (sfxinfo->driver_data != NULL) {
        return (sfx_cache_t*)sfxinfo->driver_data;
    }

    int lumpnum = sfxinfo->lumpnum;
    if (lumpnum < 0) {
        char namebuf[16];
        if (sfxinfo->link != NULL) {
            sfxinfo = sfxinfo->link;
        }
        if (s_use_sfx_prefix) {
            M_snprintf(namebuf, sizeof(namebuf), "ds%s", DEH_String(sfxinfo->name));
        } else {
            M_StringCopy(namebuf, DEH_String(sfxinfo->name), sizeof(namebuf));
        }
        lumpnum = W_CheckNumForName(namebuf);
        sfxinfo->lumpnum = lumpnum;
    }

    if (lumpnum < 0) return NULL;

    const uint8_t* raw = (const uint8_t*)W_CacheLumpNum(lumpnum, PU_STATIC);
    int lumplen = W_LumpLength(lumpnum);
    if (!raw || lumplen < 8) return NULL;

    /* Header check: DOOM sound lumps start with format = 3 (0x0003) */
    if (raw[0] != 0x03 || raw[1] != 0x00) return NULL;

    uint16_t rate = (uint16_t)(raw[2] | (raw[3] << 8));
    uint32_t len = (uint32_t)(raw[4] | (raw[5] << 8) | (raw[6] << 16) | (raw[7] << 24));
    if (len > (uint32_t)(lumplen - 8)) {
        len = (uint32_t)(lumplen - 8);
    }

    const uint8_t* pcm = raw + 8;
    if (len > 48) {
        pcm += 16;
        len -= 32;
    }

    sfx_cache_t* sc = (sfx_cache_t*)malloc(sizeof(sfx_cache_t));
    if (!sc) return NULL;
    sc->data = pcm;
    sc->length = len;
    sc->rate = rate ? rate : 11025;

    sfxinfo->driver_data = sc;
    return sc;
}

/* ── Sound Module Functions ─────────────────────────────────────── */

static boolean I_Banana_InitSound(boolean use_sfx_prefix) {
    s_use_sfx_prefix = use_sfx_prefix;
    memset(s_channels, 0, sizeof(s_channels));
    s_frac_acc = 0;

    /* Check audio output */
    int16_t test_buf[4] = {0, 0, 0, 0};
    int rc = banana_play(test_buf, sizeof(test_buf), BANANA_AUDIO_RATE, 2, 16);
    if (rc < 0) {
        printf("DOOM Audio: Pas de peripherique audio detecte dans Banana OS (muet)\n");
    } else {
        printf("DOOM Audio: Systeme audio Banana OS initialise (48 kHz 16-bit stereo)\n");
    }

    s_sound_initialized = true;
    return true;
}

static void I_Banana_ShutdownSound(void) {
    banana_audio_stop();
    memset(s_channels, 0, sizeof(s_channels));
    s_sound_initialized = false;
}

static int I_Banana_GetSfxLumpNum(sfxinfo_t* sfxinfo) {
    char namebuf[16];
    if (sfxinfo->link != NULL) {
        sfxinfo = sfxinfo->link;
    }
    if (s_use_sfx_prefix) {
        M_snprintf(namebuf, sizeof(namebuf), "ds%s", DEH_String(sfxinfo->name));
    } else {
        M_StringCopy(namebuf, DEH_String(sfxinfo->name), sizeof(namebuf));
    }
    return W_CheckNumForName(namebuf);
}

static void I_Banana_UpdateSoundParams(int channel, int vol, int sep) {
    if (!s_sound_initialized || channel < 0 || channel >= BANANA_NUM_CHANNELS) return;
    s_channels[channel].vol = vol;
    s_channels[channel].sep = sep;
}

static int I_Banana_StartSound(sfxinfo_t* sfxinfo, int channel, int vol, int sep) {
    if (!s_sound_initialized || channel < 0 || channel >= BANANA_NUM_CHANNELS) return -1;
    if (sfxinfo->link != NULL) sfxinfo = sfxinfo->link;

    sfx_cache_t* sc = CacheSFX(sfxinfo);
    if (!sc || sc->length == 0) return -1;

    s_channels[channel].sfx = sc;
    s_channels[channel].pos_fp = 0;
    s_channels[channel].step_fp = ((uint64_t)sc->rate << 16) / BANANA_AUDIO_RATE;
    s_channels[channel].vol = vol;
    s_channels[channel].sep = sep;
    s_channels[channel].active = 1;

    return channel;
}

static void I_Banana_StopSound(int channel) {
    if (channel >= 0 && channel < BANANA_NUM_CHANNELS) {
        s_channels[channel].active = 0;
    }
}

static boolean I_Banana_SoundIsPlaying(int channel) {
    if (channel >= 0 && channel < BANANA_NUM_CHANNELS) {
        return s_channels[channel].active != 0;
    }
    return false;
}

static void I_Banana_PrecacheSounds(sfxinfo_t* sounds, int num_sounds) {
    /* Cache on-demand to keep startup instant */
    (void)sounds;
    (void)num_sounds;
}

static void I_Banana_UpdateSound(void) {
    if (!s_sound_initialized) return;

    /* Compute number of output samples for this tic (48000 / 35 = 1371.428) */
    s_frac_acc += BANANA_AUDIO_RATE;
    uint32_t nsamples = s_frac_acc / 35;
    s_frac_acc %= 35;
    if (nsamples > 1440) nsamples = 1440;

    int active_count = 0;
    for (int c = 0; c < BANANA_NUM_CHANNELS; c++) {
        if (s_channels[c].active) { active_count++; }
    }

    if (!active_count && (!s_music_playing || s_music_volume == 0)) return;

    /* Safety guard: only throttle if audio queue has accumulated > 600ms backlog */
    if (banana_audio_queued_ms() > 600) return;

    memset(s_mix_l, 0, nsamples * sizeof(int32_t));
    memset(s_mix_r, 0, nsamples * sizeof(int32_t));

    /* 1. Mix SFX channels */
    if (active_count) {
        for (int c = 0; c < BANANA_NUM_CHANNELS; c++) {
            banana_channel_t* ch = &s_channels[c];
            if (!ch->active || !ch->sfx) continue;

            sfx_cache_t* sc = ch->sfx;
            uint32_t sfx_len = sc->length;
            const uint8_t* pcm = sc->data;
            uint64_t pos = ch->pos_fp;
            uint64_t step = ch->step_fp;

            int sep = ch->sep;
            if (sep < 0) sep = 0; else if (sep > 254) sep = 254;
            int vol = ch->vol;
            if (vol < 0) vol = 0; else if (vol > 127) vol = 127;

            /* Stereo panning: sep 0 = full left, 128 = center, 254 = full right */
            int lvol = (vol * (254 - sep)) / 128;
            int rvol = (vol * sep) / 128;

            for (uint32_t i = 0; i < nsamples; i++) {
                uint32_t s_idx = (uint32_t)(pos >> 16);
                if (s_idx >= sfx_len) {
                    ch->active = 0;
                    break;
                }

                /* High quality linear interpolation between consecutive samples */
                uint32_t frac = (uint32_t)(pos & 0xFFFF);
                int s0 = (int)pcm[s_idx] - 128;
                int s1 = (s_idx + 1 < sfx_len) ? ((int)pcm[s_idx + 1] - 128) : s0;
                int sample = s0 + (int)(((int64_t)(s1 - s0) * (int64_t)frac) >> 16);

                /* Scale to 16-bit without clipping */
                s_mix_l[i] += sample * lvol * 2;
                s_mix_r[i] += sample * rvol * 2;

                pos += step;
            }
            ch->pos_fp = pos;
        }
    }

    /* 2. Mix OPL FM synthesized music */
    if (s_music_playing && s_music_volume > 0 && s_music_initialized) {
        memset(s_music_buf, 0, nsamples * 2 * sizeof(int16_t));
        opl_synth_player.render(s_music_buf, nsamples);

        for (uint32_t i = 0; i < nsamples; i++) {
            s_mix_l[i] += s_music_buf[i * 2 + 0];
            s_mix_r[i] += s_music_buf[i * 2 + 1];
        }
    }

    /* Clamp and convert 32-bit mix to 16-bit signed stereo output */
    for (uint32_t i = 0; i < nsamples; i++) {
        int32_t l = s_mix_l[i];
        if (l > 32767) l = 32767;
        else if (l < -32768) l = -32768;

        int32_t r = s_mix_r[i];
        if (r > 32767) r = 32767;
        else if (r < -32768) r = -32768;

        s_pcm_out[i * 2 + 0] = (int16_t)l;
        s_pcm_out[i * 2 + 1] = (int16_t)r;
    }

    banana_play(s_pcm_out, nsamples * sizeof(int16_t) * 2, BANANA_AUDIO_RATE, 2, 16);
}

sound_module_t DG_sound_module = {
    sound_banana_devices,
    (int)(sizeof(sound_banana_devices) / sizeof(sound_banana_devices[0])),
    I_Banana_InitSound,
    I_Banana_ShutdownSound,
    I_Banana_GetSfxLumpNum,
    I_Banana_UpdateSound,
    I_Banana_UpdateSoundParams,
    I_Banana_StartSound,
    I_Banana_StopSound,
    I_Banana_SoundIsPlaying,
    I_Banana_PrecacheSounds,
};

/* ── Music Module (Authentic OPL2 FM Synthesis) ─────────────────── */

static boolean I_Banana_Music_Init(void) {
    if (!opl_synth_player.init(BANANA_AUDIO_RATE)) {
        printf("DOOM Audio: Synthetiseur OPL non disponible\n");
        return false;
    }
    s_music_initialized = true;
    opl_synth_player.setvolume(s_music_volume * 15 / 127);
    printf("DOOM Audio: Synthetiseur OPL2 FM actif (Bobby Prince / Sound Blaster @ %d Hz)\n", BANANA_AUDIO_RATE);
    return true;
}

static void I_Banana_Music_Shutdown(void) {
    if (s_music_initialized) {
        opl_synth_player.shutdown();
        s_music_initialized = false;
        s_music_playing = false;
        s_current_song = NULL;
    }
}

static void I_Banana_Music_SetMusicVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;
    s_music_volume = volume;
    if (s_music_initialized) {
        opl_synth_player.setvolume(volume * 15 / 127);
    }
}

static void I_Banana_Music_PauseMusic(void) {
    if (s_music_initialized && s_music_playing) {
        opl_synth_player.pause();
    }
}

static void I_Banana_Music_ResumeMusic(void) {
    if (s_music_initialized && s_music_playing) {
        opl_synth_player.resume();
    }
}

static void* I_Banana_Music_RegisterSong(void* data, int len) {
    if (!s_music_initialized || !data || len < 4) return NULL;
    return (void*)opl_synth_player.registersong(data, len);
}

static void I_Banana_Music_UnRegisterSong(void* handle) {
    if (s_music_initialized && handle) {
        if (s_current_song == handle) {
            s_music_playing = false;
            s_current_song = NULL;
        }
        opl_synth_player.unregistersong(handle);
    }
}

static void I_Banana_Music_PlaySong(void* handle, boolean looping) {
    if (!s_music_initialized || !handle) return;
    s_current_song = handle;
    s_music_playing = true;
    opl_synth_player.play(handle, looping);
}

static void I_Banana_Music_StopSong(void) {
    if (s_music_initialized && s_music_playing) {
        opl_synth_player.stop();
        s_music_playing = false;
        s_current_song = NULL;
    }
}

static boolean I_Banana_Music_MusicIsPlaying(void) {
    return s_music_playing;
}

static void I_Banana_Music_Poll(void) {
    /* OPL audio is rendered synchronously in I_Banana_UpdateSound() */
}

music_module_t DG_music_module = {
    music_banana_devices,
    (int)(sizeof(music_banana_devices) / sizeof(music_banana_devices[0])),
    I_Banana_Music_Init,
    I_Banana_Music_Shutdown,
    I_Banana_Music_SetMusicVolume,
    I_Banana_Music_PauseMusic,
    I_Banana_Music_ResumeMusic,
    I_Banana_Music_RegisterSong,
    I_Banana_Music_UnRegisterSong,
    I_Banana_Music_PlaySong,
    I_Banana_Music_StopSong,
    I_Banana_Music_MusicIsPlaying,
    I_Banana_Music_Poll,
};
