Columbia Street, a five-channel jungle sequencer for the Miyoo Mini Plus (OnionOS).

**Install**

With SSH on (Onion: Apps → Tweaks → Network → SSH), run this on a computer on the same network:

```sh
curl -fsSL https://raw.githubusercontent.com/sydilaxe/ColumbiaStreet-dev/main/install.sh | bash
```

Or by hand: unzip `ColumbiaStreet-miyoo.zip` at the root of the SD card (it contains
`App/ColumbiaStreet/`), then open **Apps → Columbia Street**. Press MENU to quit.

**New in 0.3.0**
- **Updates itself:** a few seconds after it starts, Columbia Street asks GitHub for the latest
  release and, when there's a newer one, offers to download it and restart (A) or wait (B). The
  music keeps playing while it downloads; your song and your own breaks are kept, and a failed
  update changes nothing. `CS_UPDATE=0` in `launch.sh` turns it off. This starts working with
  the release after 0.3.0.
- **One-line install** over Wi-Fi with `install.sh` (above), which also updates an installed copy.

**New in 0.2.0**
- **Mono + clock out**, as on HI5: on SONG, set **OUTPUT** to **MONO+CLK** and the sound plays in
  mono on one side of the headphone jack, with clock pulses on the other to lead a Pocket Operator,
  Volca or other gear. **CLOCK ON** picks the side, **PPQN** the pulses per quarter note (2 for
  PO/Volca, 24 for DIN) and **PULSE** the width. Every pan moves to the centre while it is on.
- Songs from 0.1.0 still load (in STEREO).

**New in 0.1.0**
- Four break channels playing slices of sampled breaks (the Amen, four more breaks, the Amen
  pre-chopped and a jungle hit kit) and a bass channel (SUB, REESE, 808, GROWL or 20 jungle bass
  samples through a riff).
- Three Euclidean cells per channel: HIT, ROLL (ratchets) and ALT (reverse, octave, next or random
  slice, stop).
- Per break channel: FIX / WALK / GRID / RAND slice modes, pitch, fit to tempo, filter, bit depth
  1-24, sample-rate reduction and a compressor. On the bus: glue compressor, drive, reverb, the L2
  sweep and R2 + d-pad repeat, reverse and tape stop.
- SELECT makes a new jungle beat (R2+SELECT undoes it); 8 pattern slots switch on the bar line;
  Wi-Fi sync with HI5 and Jaffedrone.
- Your own breaks: drop a WAV loop into `App/ColumbiaStreet/breaks/` (with the tempo in its name,
  e.g. `APACHE_115bpm.wav`) and it is sliced at its hits when the app starts.

If you hear crackles, set `CS_BUFFER=2048` in `App/ColumbiaStreet/launch.sh`. The log is written to
`App/ColumbiaStreet/log.txt`.
