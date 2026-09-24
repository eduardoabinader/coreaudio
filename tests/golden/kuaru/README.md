# Kuaru pre-extraction golden vectors

These files are interleaved stereo IEEE-754 float32 little-endian PCM at
48 kHz. They capture Kuaru before production DSP moves into this repository.

The vectors were promoted manually after two complete fresh-process captures
produced identical SHA-256 hashes for every vector. The 24-bit WAV outputs are
not authoritative because JUCE's writer applies dither. See `manifest.json`
for fixture provenance, block geometry, seed, comparison policy, and hashes.

Golden files must never be overwritten by the capture script. A changed vector
requires diagnosis and a documented acceptance decision.
