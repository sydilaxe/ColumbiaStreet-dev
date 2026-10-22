#include "breaks.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

Bank banks[MAX_BANKS];
int nbanks;
BassSample bass_smp[MAX_BASS];
int nbass;

static float sr = 48000.f;

void breaks_set_rate(int rate) { sr = (float)rate; }

/* ------------------------------------------------------------------ wav */

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* any PCM or float WAV, mixed to mono and resampled to the engine rate */
static float *load_wav(const char *path, int *out_len, float max_secs)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 44 || sz > 64 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = malloc(sz);
    int ok = buf && fread(buf, 1, sz, f) == (size_t)sz;
    fclose(f);
    if (!ok || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) {
        free(buf);
        return NULL;
    }
    int fmt = 0, ch = 0, rate = 0, bits = 0;
    const uint8_t *data = NULL;
    uint32_t dlen = 0;
    for (long p = 12; p + 8 <= sz;) {
        uint32_t clen = rd32(buf + p + 4);
        if (clen > (uint32_t)(sz - p - 8))
            clen = (uint32_t)(sz - p - 8);
        if (!memcmp(buf + p, "fmt ", 4) && clen >= 16) {
            fmt = rd16(buf + p + 8);
            ch = rd16(buf + p + 10);
            rate = (int)rd32(buf + p + 12);
            bits = rd16(buf + p + 22);
            if (fmt == 0xFFFE && clen >= 26)
                fmt = rd16(buf + p + 32);
        }
        else if (!memcmp(buf + p, "data", 4)) {
            data = buf + p + 8;
            dlen = clen;
        }
        p += 8 + clen + (clen & 1);
    }
    int bps = bits / 8;
    if (!data || ch < 1 || rate < 4000 || bps < 1 || !((fmt == 1 && bps <= 4) || (fmt == 3 && bps == 4))) {
        free(buf);
        return NULL;
    }
    int frames = (int)(dlen / (bps * ch));
    double ratio = (double)rate / sr;
    int n = (int)(frames / ratio);
    if (n > (int)(max_secs * sr))
        n = (int)(max_secs * sr);
    if (n < 16) {
        free(buf);
        return NULL;
    }
    float *mono = malloc(frames * sizeof(float));
    float *out = malloc(n * sizeof(float));
    if (!mono || !out) {
        free(mono);
        free(out);
        free(buf);
        return NULL;
    }
    for (int i = 0; i < frames; i++) {
        float acc = 0.f;
        for (int c = 0; c < ch; c++) {
            const uint8_t *q = data + ((size_t)i * ch + c) * bps;
            float x;
            if (fmt == 3) {
                uint32_t u = rd32(q);
                memcpy(&x, &u, 4);
            }
            else if (bps == 1)
                x = (q[0] - 128) / 128.f;
            else if (bps == 2)
                x = (int16_t)rd16(q) / 32768.f;
            else if (bps == 3)
                x = (int32_t)((uint32_t)q[0] << 8 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 24) / 2147483648.f;
            else
                x = (int32_t)rd32(q) / 2147483648.f;
            acc += x;
        }
        mono[i] = acc / ch;
    }
    free(buf);
    for (int i = 0; i < n; i++) {
        double pos = i * ratio;
        int j = (int)pos;
        float fr = (float)(pos - j);
        float a = mono[j < frames ? j : frames - 1], b = mono[j + 1 < frames ? j + 1 : frames - 1];
        out[i] = a + (b - a) * fr;
    }
    free(mono);
    *out_len = n;
    return out;
}

/* ---------------------------------------------------------------- names */

/* "amen_153bpm.wav" -> 153; "think-112.wav" -> 112; else 0 */
static float bpm_from_name(const char *s)
{
    const char *p = strcasestr(s, "bpm");
    if (p) {
        const char *q = p;
        while (q > s && isdigit((unsigned char)q[-1]))
            q--;
        if (q < p) {
            float v = (float)atof(q);
            if (v >= 50.f && v <= 300.f)
                return v;
        }
        if (isdigit((unsigned char)p[3])) {
            float v = (float)atof(p + 3);
            if (v >= 50.f && v <= 300.f)
                return v;
        }
    }
    for (const char *q = s; *q; q++)
        if (isdigit((unsigned char)*q) && (q == s || !isalnum((unsigned char)q[-1]))) {
            float v = (float)atof(q);
            int len = 0;
            while (isdigit((unsigned char)q[len]))
                len++;
            if (len >= 2 && len <= 3 && v >= 60.f && v <= 250.f && !isalpha((unsigned char)q[len]))
                return v;
        }
    return 0.f;
}

/* labels are unique: a repeat ends in 2, 3, ... */
static void dedupe(char *out, const char *taken, int stride, int n)
{
    for (int tries = 2; tries < 10; tries++) {
        int clash = 0;
        for (int i = 0; i < n; i++)
            clash |= !strcmp(taken + i * stride, out);
        if (!clash)
            return;
        int l = (int)strlen(out);
        if (l >= 6)
            l = 5;
        out[l] = (char)('0' + tries);
        out[l + 1] = 0;
    }
}

/* bass names: the first 6 letters and digits after the number prefix,
 * separators dropped ("001_fat_808_sub" -> FAT808) */
static void label_compact(const char *s, char *out)
{
    int k = 0;
    while (isdigit((unsigned char)*s) || *s == '_' || *s == '-' || *s == ' ')
        s++;
    for (; *s && *s != '.' && k < 6; s++)
        if (isalnum((unsigned char)*s))
            out[k++] = (char)toupper((unsigned char)*s);
    if (!k)
        out[k++] = '?';
    out[k] = 0;
}

/* up to 6 characters before the first '_', '.', '-' or space, upper-cased */
static void label_from_name(const char *s, char *out)
{
    int k = 0;
    for (; *s && k < 6; s++) {
        if (*s == '_' || *s == '.' || *s == '-' || *s == ' ') {
            if (k)
                break;
            continue;
        }
        if (isdigit((unsigned char)*s) && !k)
            continue; /* "000_amen" -> AMEN */
        out[k++] = (char)toupper((unsigned char)*s);
    }
    if (!k)
        out[k++] = '?';
    out[k] = 0;
}

/* -------------------------------------------------------------- slicing */

static int band_tag(const float *x, int n)
{
    /* first 40 ms: share of energy below ~150 Hz and above ~5 kHz */
    float lp = 0.f, lp2 = 0.f, el = 0.f, eh = 0.f, et = 1e-9f;
    float kl = 1.f - expf(-2.f * (float)M_PI * 150.f / sr);
    float kh = 1.f - expf(-2.f * (float)M_PI * 5000.f / sr);
    int m = (int)(0.04f * sr);
    if (m > n)
        m = n;
    for (int i = 0; i < m; i++) {
        lp += kl * (x[i] - lp);
        lp2 += kh * (x[i] - lp2);
        float hi = x[i] - lp2;
        el += lp * lp;
        eh += hi * hi;
        et += x[i] * x[i];
    }
    if (el / et > 0.35f)
        return TAG_KICK;
    if (eh / et > 0.3f)
        return TAG_HAT;
    return TAG_SNARE;
}

static void finish_bank(Bank *b)
{
    b->start[b->nslices] = b->len;
    for (int i = 0; i < b->nslices; i++)
        b->tag[i] = (uint8_t)band_tag(b->data + b->start[i], b->start[i + 1] - b->start[i]);
    float pk = 1e-9f;
    for (int i = 0; i < b->len; i++)
        pk = fmaxf(pk, fabsf(b->data[i]));
    /* bring every bank to the same peak so switching keeps the level */
    float g = 0.9f / pk;
    for (int i = 0; i < b->len; i++)
        b->data[i] *= g;
    for (int k = 0; k < NPEAK; k++) {
        int a = (int)((long long)b->len * k / NPEAK), e = (int)((long long)b->len * (k + 1) / NPEAK);
        float m = 0.f;
        for (int i = a; i < e; i++)
            m = fmaxf(m, fabsf(b->data[i]));
        b->peak[k] = m;
    }
}

/* tempo for a loop with none in its name: the bar count that puts it
 * nearest 140 BPM */
static float guess_bpm(float secs)
{
    static const int bars[] = {1, 2, 4, 8, 3};
    float best = 0.f, err = 1e9f;
    for (unsigned i = 0; i < sizeof bars / sizeof bars[0]; i++) {
        float bpm = bars[i] * 240.f / secs;
        if (bpm < 70.f || bpm > 200.f)
            continue;
        float e = fabsf(bpm - 140.f) + (bars[i] == 3 ? 30.f : 0.f);
        if (e < err) {
            err = e;
            best = bpm;
        }
    }
    return best > 0.f ? best : 240.f / secs;
}

/* Onsets from a 4 ms high-passed energy envelope: a hop louder than the
 * last 24 ms by more than e^1 starts a slice. Each start is moved back to
 * just before the attack, and with a known tempo snapped to the 16th grid
 * when it lies close to it. Takes ownership of data. */
int bank_from_loop(Bank *b, const char *name, float *data, int len, float bpm)
{
    memset(b, 0, sizeof *b);
    snprintf(b->name, sizeof b->name, "%s", name);
    b->data = data;
    b->len = len;
    float secs = len / sr;
    b->bpm = bpm > 0.f ? bpm : guess_bpm(secs);
    float g = sr * 15.f / b->bpm; /* samples per 16th */
    b->steps = (int)(len / g + 0.5f);
    if (b->steps < 1)
        b->steps = 1;

    int hop = (int)(0.004f * sr), nh = len / hop;
    float *e = malloc((nh + 1) * sizeof(float));
    float emax = 1e-9f, prev = 0.f;
    for (int h = 0; h < nh; h++) {
        float s = 1e-7f;
        for (int i = h * hop; i < (h + 1) * hop; i++) {
            float d = data[i] - prev;
            prev = data[i];
            s += d * d;
        }
        e[h] = s;
        emax = fmaxf(emax, s);
    }
    b->nslices = 1;
    b->start[0] = 0;
    b->at[0] = 0.f;
    int last = 0;
    for (int h = 1; h + 1 < nh && b->nslices < MAX_SLICES; h++) {
        float mean = 0.f;
        int k0 = h - 6 < 0 ? 0 : h - 6;
        for (int k = k0; k < h; k++)
            mean += e[k];
        mean /= (h - k0);
        if (logf(e[h]) - logf(mean) <= 1.f || e[h] < emax * 0.001f || logf(e[h]) < logf(e[h + 1]) - 0.3f)
            continue;
        if ((h - last) * hop < 0.05f * sr)
            continue;
        last = h;
        /* back to where the hop's attack begins, less a millisecond */
        int a = h * hop;
        float pk = 0.f;
        for (int i = a; i < a + hop; i++)
            pk = fmaxf(pk, fabsf(data[i]));
        int s = a;
        while (s < a + hop && fabsf(data[s]) < pk * 0.2f)
            s++;
        s -= (int)(0.001f * sr);
        float at = s / g;
        float snap = roundf(at);
        if (bpm > 0.f && fabsf(at - snap) < 0.3f)
            at = snap;
        if (s <= b->start[b->nslices - 1] + hop)
            continue;
        b->start[b->nslices] = s;
        b->at[b->nslices] = at;
        b->nslices++;
    }
    free(e);
    finish_bank(b);
    return 1;
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* sorted names of the entries in dir; wavs only, or directories only */
static int list_dir(const char *dir, char **names, int max, int want_dirs)
{
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) && n < max) {
        const char *nm = ent->d_name;
        if (nm[0] == '.')
            continue;
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", dir, nm);
        struct stat st;
        if (stat(path, &st) != 0)
            continue;
        size_t l = strlen(nm);
        if (want_dirs ? S_ISDIR(st.st_mode) : (S_ISREG(st.st_mode) && l > 4 && !strcasecmp(nm + l - 4, ".wav")))
            names[n++] = strdup(nm);
    }
    closedir(d);
    qsort(names, n, sizeof *names, cmp_str);
    return n;
}

/* a folder of slices, joined into one loop with a slice per file */
static int bank_from_folder(Bank *b, const char *dir, const char *name)
{
    char *files[MAX_SLICES];
    int nf = list_dir(dir, files, MAX_SLICES, 0);
    if (!nf)
        return 0;
    float *parts[MAX_SLICES];
    int lens[MAX_SLICES], total = 0, np = 0;
    for (int i = 0; i < nf; i++) {
        char path[2200];
        snprintf(path, sizeof path, "%s/%s", dir, files[i]);
        int n = 0;
        float *d = load_wav(path, &n, 4.f);
        if (d) {
            parts[np] = d;
            lens[np++] = n;
            total += n;
        }
        free(files[i]);
    }
    if (!np)
        return 0;
    memset(b, 0, sizeof *b);
    label_from_name(name, b->name);
    b->data = malloc(total * sizeof(float));
    b->len = total;
    int pos = 0;
    for (int i = 0; i < np; i++) {
        memcpy(b->data + pos, parts[i], lens[i] * sizeof(float));
        free(parts[i]);
        b->start[i] = pos;
        pos += lens[i];
    }
    b->nslices = np;
    float bpm = bpm_from_name(name);
    b->bpm = bpm > 0.f ? bpm : guess_bpm(total / sr);
    float g = sr * 15.f / b->bpm;
    b->steps = (int)(total / g + 0.5f);
    if (b->steps < 1)
        b->steps = 1;
    /* named with a tempo, the slices are taken as evenly spread over the
     * loop (a break cut on the grid); otherwise by their lengths */
    for (int i = 0; i < np; i++)
        b->at[i] = bpm > 0.f ? (float)i * b->steps / np : b->start[i] / g;
    finish_bank(b);
    /* a kit named by its files tags better than the spectrum guess */
    char *fn[MAX_SLICES];
    int nn = list_dir(dir, fn, MAX_SLICES, 0);
    for (int i = 0; i < nn; i++) {
        if (i < b->nslices) {
            if (strcasestr(fn[i], "kick") || strcasestr(fn[i], "bd"))
                b->tag[i] = TAG_KICK;
            else if (strcasestr(fn[i], "snare") || strcasestr(fn[i], "sd"))
                b->tag[i] = TAG_SNARE;
            else if (strcasestr(fn[i], "hh") || strcasestr(fn[i], "hat") || strcasestr(fn[i], "ride"))
                b->tag[i] = TAG_HAT;
        }
        free(fn[i]);
    }
    return 1;
}

int breaks_load_dir(const char *dir)
{
    char *names[MAX_BANKS * 2];
    int added = 0;
    int n = list_dir(dir, names, MAX_BANKS * 2, 0);
    for (int i = 0; i < n; i++) {
        char path[1100], label[8];
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        int len = 0;
        float *d = nbanks < MAX_BANKS ? load_wav(path, &len, 16.f) : NULL;
        if (d) {
            label_from_name(names[i], label);
            dedupe(label, banks[0].name, sizeof(Bank), nbanks);
            bank_from_loop(&banks[nbanks++], label, d, len, bpm_from_name(names[i]));
            added++;
            fprintf(stderr, "breaks: %s -> %s, %d slices, %.1f BPM, %d steps\n", names[i], label,
                    banks[nbanks - 1].nslices, banks[nbanks - 1].bpm, banks[nbanks - 1].steps);
        }
        else
            fprintf(stderr, "breaks: skipped %s\n", path);
        free(names[i]);
    }
    n = list_dir(dir, names, MAX_BANKS * 2, 1);
    for (int i = 0; i < n; i++) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        if (nbanks < MAX_BANKS && bank_from_folder(&banks[nbanks], path, names[i])) {
            dedupe(banks[nbanks].name, banks[0].name, sizeof(Bank), nbanks);
            nbanks++;
            added++;
            fprintf(stderr, "breaks: %s/ -> %s, %d slices\n", names[i], banks[nbanks - 1].name,
                    banks[nbanks - 1].nslices);
        }
        free(names[i]);
    }
    return added;
}

/* ----------------------------------------------------------------- bass */

/* the fundamental of a bass one-shot by autocorrelation (30-300 Hz),
 * taking the shortest lag that comes within 10% of the best */
static float detect_pitch(const float *x, int n)
{
    int a = (int)(0.03f * sr), w = (int)(0.08f * sr);
    int lmin = (int)(sr / 300.f), lmax = (int)(sr / 30.f);
    if (a + w + lmax > n) {
        a = 0;
        if (w + lmax > n)
            return 0.f;
    }
    static float r[4096];
    if (lmax >= 4096)
        lmax = 4095;
    float e0 = 1e-9f, best = 0.f;
    for (int i = 0; i < w; i++)
        e0 += x[a + i] * x[a + i];
    for (int l = lmin; l <= lmax; l++) {
        float s = 0.f, e1 = 1e-9f;
        for (int i = 0; i < w; i++) {
            s += x[a + i] * x[a + i + l];
            e1 += x[a + i + l] * x[a + i + l];
        }
        r[l] = s / sqrtf(e0 * e1);
        best = fmaxf(best, r[l]);
    }
    if (best < 0.5f)
        return 0.f;
    for (int l = lmin + 1; l < lmax; l++)
        if (r[l] >= best * 0.9f && r[l] >= r[l - 1] && r[l] >= r[l + 1]) {
            /* parabolic peak for a fractional lag */
            float d = r[l - 1] - 2.f * r[l] + r[l + 1];
            float off = d != 0.f ? 0.5f * (r[l - 1] - r[l + 1]) / d : 0.f;
            return sr / (l + off);
        }
    return 0.f;
}

int bass_load_dir(const char *dir)
{
    char *names[MAX_BASS];
    int n = list_dir(dir, names, MAX_BASS, 0), added = 0;
    for (int i = 0; i < n; i++) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        int len = 0;
        float *d = nbass < MAX_BASS ? load_wav(path, &len, 6.f) : NULL;
        if (d) {
            BassSample *s = &bass_smp[nbass++];
            label_compact(names[i], s->name);
            dedupe(s->name, bass_smp[0].name, sizeof(BassSample), nbass - 1);
            s->data = d;
            s->len = len;
            s->root_hz = detect_pitch(d, len);
            float pk = 1e-9f;
            for (int k = 0; k < len; k++)
                pk = fmaxf(pk, fabsf(d[k]));
            for (int k = 0; k < len; k++)
                d[k] *= 0.9f / pk;
            added++;
            fprintf(stderr, "bass: %s -> %s, root %.1f Hz\n", names[i], s->name, s->root_hz);
        }
        free(names[i]);
    }
    return added;
}

/* --------------------------------------------------------------- lookup */

/* Each slice belongs to the step nearest its onset. Two onsets closer
 * than a 16th share a step: the first is played and runs on through the
 * second. Returns the slice starting at this step and sets *exact, or the
 * slice still sounding from an earlier step. */
static int step_of(const Bank *b, int i) { return (int)floorf(b->at[i] + 0.5f); }

int bank_slice_at(const Bank *b, double step)
{
    double s = fmod(step, (double)b->steps);
    if (s < 0)
        s += b->steps;
    int si = (int)floor(s + 1e-3), i = 0;
    while (i + 1 < b->nslices && step_of(b, i + 1) <= si)
        i++;
    while (i > 0 && step_of(b, i - 1) == step_of(b, i))
        i--;
    return i;
}

int bank_slice_exact(const Bank *b, double step)
{
    int i = bank_slice_at(b, step);
    double s = fmod(step, (double)b->steps);
    if (s < 0)
        s += b->steps;
    return step_of(b, i) == (int)floor(s + 1e-3);
}

/* the end of slice i, past any later onsets sharing its step */
int bank_slice_end(const Bank *b, int i)
{
    int j = i + 1;
    while (j < b->nslices && step_of(b, j) == step_of(b, i))
        j++;
    return b->start[j];
}

int bank_count_tag(const Bank *b, int tag)
{
    int c = 0;
    for (int i = 0; i < b->nslices; i++)
        c += (b->tag[i] & tag) != 0;
    return c;
}

int bank_find_tag(const Bank *b, int tag, int nth)
{
    for (int i = 0; i < b->nslices; i++)
        if (b->tag[i] & tag) {
            if (!nth)
                return i;
            nth--;
        }
    return -1;
}

/* ------------------------------------------------------------- fallback */

/* With no samples installed the app still plays: one bar of a plain drum
 * machine at 160 BPM, so the screens make sense until breaks/ is filled. */
void breaks_fallback(void)
{
    static const char pat[] = "KH.HSH.KK.KHS.HH"; /* K kick, S snare, H hat per 16th */
    float g = sr * 15.f / 160.f;
    int len = (int)(16 * g);
    float *d = calloc(len, sizeof(float));
    uint32_t rng = 1;
    for (int s = 0; s < 16; s++) {
        int p0 = (int)(s * g);
        for (int i = 0; p0 + i < len && i < (int)(0.4f * sr); i++) {
            float t = i / sr, x = 0.f;
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            float nz = (rng >> 8) * (2.f / 16777216.f) - 1.f;
            if (pat[s] == 'K')
                x = sinf(2.f * (float)M_PI * (50.f * t + 1.6f * (1.f - expf(-t / 0.03f)))) * expf(-t / 0.18f);
            else if (pat[s] == 'S')
                x = (0.6f * sinf(2.f * (float)M_PI * 190.f * t) * expf(-t / 0.06f) + 0.7f * nz * expf(-t / 0.12f));
            else if (pat[s] == 'H')
                x = 0.35f * nz * expf(-t / 0.03f);
            d[p0 + i] += x;
        }
    }
    bank_from_loop(&banks[nbanks++], "NOSMPL", d, len, 160.f);
}

void breaks_free(void)
{
    for (int i = 0; i < nbanks; i++)
        free(banks[i].data);
    for (int i = 0; i < nbass; i++)
        free(bass_smp[i].data);
    nbanks = nbass = 0;
}
