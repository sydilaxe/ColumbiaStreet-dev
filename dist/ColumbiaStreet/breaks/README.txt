Break loops for Columbia Street.

A .wav file here is one break loop. Columbia Street finds its hits and cuts a
slice at each one. Put the tempo in the name so the slices land on the 16th
grid, for example:

    APACHE_115bpm.wav
    THINK_114bpm.wav

Without a tempo it guesses one from the length (1, 2, 4 or 8 bars).

A folder here is a break that is already chopped: its .wav files, in name
order, are the slices (for example AMENCUT_149bpm/001.wav, 002.wav, ...).

Any PCM or float WAV works, mono or stereo, any sample rate, up to 16 s per
loop and 64 slices. Up to 32 banks load, files first, then folders, in name
order. Banks are shown by the first 6 letters of the name.

The curated set (Amen, PLEAD, RAWCLN, SDSTCK, BLLSTP, the Amen pre-chopped
and a jungle hit kit) comes from TidalCycles' Dirt-Samples library and is
fetched by tools/fetch_samples.sh.
