/* Break banks and bass samples, loaded from the app folder at start-up.
 *
 * A bank is one mono loop with slice points at its hits. A .wav file in
 * breaks/ is a whole loop, cut by onset detection; a tempo in its name
 * ("apache_115bpm.wav") puts the slices on the 16th grid. A folder in
 * breaks/ is a loop that is already chopped: its .wav files, in name order,
 * are the slices. bass/ holds one-shots the bass channel can play pitched. */
#pragma once
#include <stdint.h>

#define MAX_BANKS 32
#define MAX_SLICES 64
#define MAX_BASS 32
#define NPEAK 256

/* what a slice starts with, guessed from its spectrum (for the generator) */
enum { TAG_KICK = 1, TAG_SNARE = 2, TAG_HAT = 4 };

typedef struct {
    char name[8];
    float *data;   /* mono, at the engine rate */
    int len;
    float bpm;     /* native tempo */
    int steps;     /* 16th notes the loop spans */
    int nslices;
    int start[MAX_SLICES + 1]; /* sample offsets; start[nslices] == len */
    float at[MAX_SLICES];      /* each slice's position in 16ths */
    uint8_t tag[MAX_SLICES];
    float peak[NPEAK];         /* overview for the waveform display */
} Bank;

typedef struct {
    char name[8];
    float *data;
    int len;
    float root_hz; /* detected pitch, 0 if none was found */
} BassSample;

extern Bank banks[MAX_BANKS];
extern int nbanks;
extern BassSample bass_smp[MAX_BASS];
extern int nbass;

void breaks_set_rate(int sample_rate);
int breaks_load_dir(const char *dir); /* returns how many banks were added */
int bass_load_dir(const char *dir);
void breaks_fallback(void);            /* a small drum-machine bank when no samples are installed */
void breaks_free(void);

int bank_slice_at(const Bank *b, double step); /* the slice sounding at a grid position */
int bank_slice_exact(const Bank *b, double step); /* a slice starts on this step */
int bank_slice_end(const Bank *b, int i);      /* GRID end of slice i */
int bank_find_tag(const Bank *b, int tag, int nth); /* nth slice with the tag, or -1 */
int bank_count_tag(const Bank *b, int tag);

/* exposed for the test harness */
int bank_from_loop(Bank *b, const char *name, float *data, int len, float bpm);
