#include "LofiSynth.h"
#include "samples/drum/hat.h"
#include "samples/drum/kick.h"
#include "samples/drum/snare.h"
#include "samples/piano/A1v1.h"
#include "samples/piano/A1v3.h"
#include "samples/piano/A2v1.h"
#include "samples/piano/A2v3.h"
#include "samples/piano/A3v1.h"
#include "samples/piano/A3v3.h"
#include "samples/piano/A4v1.h"
#include "samples/piano/A4v3.h"
#include "samples/piano/A5v1.h"
#include "samples/piano/A5v3.h"
#include "samples/piano/A6v1.h"
#include "samples/piano/A6v3.h"
#include "samples/piano/C1v1.h"
#include "samples/piano/C1v3.h"
#include "samples/piano/C2v1.h"
#include "samples/piano/C2v3.h"
#include "samples/piano/C3v1.h"
#include "samples/piano/C3v3.h"
#include "samples/piano/C4v1.h"
#include "samples/piano/C4v3.h"
#include "samples/piano/C5v1.h"
#include "samples/piano/C5v3.h"
#include "samples/piano/C6v1.h"
#include "samples/piano/C6v3.h"
#include "samples/piano/Dsharp1v1.h"
#include "samples/piano/Dsharp1v3.h"
#include "samples/piano/Dsharp2v1.h"
#include "samples/piano/Dsharp2v3.h"
#include "samples/piano/Dsharp3v1.h"
#include "samples/piano/Dsharp3v3.h"
#include "samples/piano/Dsharp4v1.h"
#include "samples/piano/Dsharp4v3.h"
#include "samples/piano/Dsharp5v1.h"
#include "samples/piano/Dsharp5v3.h"
#include "samples/piano/Dsharp6v1.h"
#include "samples/piano/Dsharp6v3.h"
#include "samples/piano/Fsharp1v1.h"
#include "samples/piano/Fsharp1v3.h"
#include "samples/piano/Fsharp2v1.h"
#include "samples/piano/Fsharp2v3.h"
#include "samples/piano/Fsharp3v1.h"
#include "samples/piano/Fsharp3v3.h"
#include "samples/piano/Fsharp4v1.h"
#include "samples/piano/Fsharp4v3.h"
#include "samples/piano/Fsharp5v1.h"
#include "samples/piano/Fsharp5v3.h"
#include "samples/piano/Fsharp6v1.h"
#include "samples/piano/Fsharp6v3.h"
#include <algorithm>
#include <cstring>
#include <random>
#include <map>
#include <vector>

namespace LofiSynth {

// ============================================================================
// OPTIMIZATION: LOOKUP TABLES FOR FASTER TRIGONOMETRY
// ============================================================================

// Precomputed sine wave lookup table (256 samples for one period)
static const uint16_t SIN_TABLE_SIZE = 256;
static const uint16_t SIN_TABLE_MASK = SIN_TABLE_SIZE - 1;
static float sinTable[SIN_TABLE_SIZE];
static bool tablesInitialized = false;

// Initialize lookup tables
static void initTables() {
    if (tablesInitialized) return;
    
    // Generate sine table
    for (int i = 0; i < SIN_TABLE_SIZE; i++) {
        sinTable[i] = sinf(2.0f * M_PI * i / SIN_TABLE_SIZE);
    }
    
    tablesInitialized = true;
}

// OPTIMIZED: Table-based sine with linear interpolation
static inline float fastSinTable(float phase) {
    // Normalize phase to [0, 2π] - more robust normalization
    phase = fmodf(phase, 2.0f * M_PI);
    if (phase < 0) phase += 2.0f * M_PI;
    
    // Convert to table index
    float tablePos = phase * (SIN_TABLE_SIZE / (2.0f * M_PI));
    int index = (int)tablePos;
    
    // Bounds check to prevent array overflow
    if (index >= SIN_TABLE_SIZE) index = SIN_TABLE_SIZE - 1;
    if (index < 0) index = 0;
    
    // Simple lookup without interpolation for now (to debug)
    return sinTable[index];
}

// ============================================================================
// STATIC MEMBER INITIALIZATION (Following Note.cpp pattern)
// ============================================================================

// --------------------- VariationEngine implementation ----------------------

VariationEngine::VariationEngine()
    : cfg_(), sampleRate_(44100.0f), rngState_(0xC0FFEE), currentBarInPhrase_(0), phraseMicroSwingActive_(false) {}

void VariationEngine::init(float sampleRate, const VariationConfig &cfg) {
    cfg_ = cfg;
    sampleRate_ = sampleRate;
    if (cfg_.rngSeed != 0) rngState_ = cfg_.rngSeed;
    else rngState_ = (uint32_t)xTaskGetTickCount();
    currentBarInPhrase_ = 0;
    phraseMicroSwingActive_ = false;
}

uint32_t VariationEngine::lfsr_next() {
    // simple xorshift for cheap PRNG
    uint32_t x = rngState_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState_ = x;
    return x;
}

void VariationEngine::onBarBoundary(int barIndex) {
    if (!cfg_.enabled) return;
    currentBarInPhrase_ = (barIndex % cfg_.phraseLengthBars);
    // schedule micro-swing for the whole phrase with some probability
    uint32_t r = lfsr_next();
    float p = (r & 0xFFFF) / (float)0x10000;
    phraseMicroSwingActive_ = (p < 0.5f);
}

float VariationEngine::processStepTimingOffset(int stepIndex) {
    if (!cfg_.enabled) return 0.0f;
    if (!phraseMicroSwingActive_) return 0.0f;
    // decide per-step if micro-swing applies
    uint32_t r = lfsr_next();
    float p = (r & 0xFFFF) / (float)0x10000;
    if (p > cfg_.microSwingProb) return 0.0f;
    // produce a timing offset in seconds (microSwingMs randomized ±20%)
    float variance = cfg_.microSwingMs * 0.2f;
    float base = cfg_.microSwingMs + ((int)(r & 0xFF) - 128) / 128.0f * variance;
    // random direction (before/after beat)
    bool before = (r & 0x100) == 0;
    float ms = before ? -base : base;
    return ms * 0.001f;
}

void VariationEngine::tick() {
    // placeholder - currently no per-sample work
}


SemaphoreHandle_t LofiEngine::synthMutex_ = nullptr;
bool LofiEngine::mutexInitialized_ = false;

// ============================================================================
// VOICE IMPLEMENTATION - Harmonic Synthesis with ADSR
// ============================================================================

Voice::Voice() 
    : frequency_(0.0f)
    , amplitude_(0.0f)
    , phase_(0.0f)
    , timePosition_(0.0f)
    , noteLength_(0.0f)
    , active_(false)
    , sampleRate_(44100.0f)
    , sampleCount_(0)
{
}

void Voice::noteOn(float freq, float amp, float length) {
    frequency_ = freq;
    amplitude_ = amp;
    noteLength_ = length;
    timePosition_ = 0.0f;
    phase_ = 0.0f;
    sampleCount_ = 0;
    active_ = true;
}

void Voice::noteOff() {
    active_ = false;
    amplitude_ = 0.0f;
}

float Voice::renderSample() {
    if (!active_) return 0.0f;

    timePosition_ = sampleCount_ / sampleRate_;

    // Check if note has finished
    if (timePosition_ >= noteLength_) {
        active_ = false;
        return 0.0f;
    }

    // Generate harmonic-rich waveform
    float sample = generateHarmonicWave(frequency_, phase_);

    // Apply ADSR envelope (this is your improved fade!)
    float envelope = calculateEnvelope(timePosition_);

    // Update phase for next sample
    phase_ += (2.0f * M_PI * frequency_) / sampleRate_;
    if (phase_ > 2.0f * M_PI) {
        phase_ -= 2.0f * M_PI;
    }

    sampleCount_++;
    return sample * envelope * amplitude_;
}

float Voice::calculateEnvelope(float time) {
    // OPTIMIZATION: Cache envelope timing calculations
    static float cachedTotalTime = 0.0f;
    static float cachedAttackTime = 0.0f;
    static float cachedDecayTime = 0.0f;
    static float cachedReleaseStart = 0.0f;
    
    if (cachedTotalTime != noteLength_) {
        cachedTotalTime = noteLength_;
        cachedAttackTime = envelope_.attack;
        cachedDecayTime = envelope_.decay;
        cachedReleaseStart = cachedTotalTime - envelope_.release;
    }
    
    float totalTime = cachedTotalTime;
    float attackTime = cachedAttackTime;
    float decayTime = cachedDecayTime;
    float releaseStart = cachedReleaseStart;

    if (time < attackTime) {
        // Attack phase: Quick rise to peak
        return time / attackTime;
    }
    else if (time < attackTime + decayTime) {
        // Decay phase: Exponential decay to sustain level
        float decayProgress = (time - attackTime) / decayTime;
        float decayAmount = 1.0f - envelope_.sustain;

        // Fast exp approximation: e^(-3x) ≈ 1/(1 + 3x + 4.5x² + 2.25x³) for x in [0,1]
        float x = 3.0f * decayProgress;
        float fastExp = 1.0f / (1.0f + x + 2.25f * x * x + 0.75f * x * x * x);

        return 1.0f - decayAmount * (1.0f - fastExp);
    }
    else if (time < releaseStart) {
        // Use original fastSin for reliability
        static float driftPhase = 0.0f;
        driftPhase += 0.01f; // Increment for next sample
        if (driftPhase > 6.283185307f) driftPhase -= 6.283185307f;

        float drift = 0.05f * fastSin(driftPhase);  // Revert to original
        return envelope_.sustain + drift;
    }
    else {
        // Release phase: Smooth exponential fade to zero
        float releaseProgress = (time - releaseStart) / envelope_.release;

        // Fast exp approximation: e^(-4x) ≈ 1/(1 + 4x + 8x² + 5.33x³) for x in [0,1]
        float x = 4.0f * releaseProgress;
        float fastExp = 1.0f / (1.0f + x + 4.0f * x * x + 1.33f * x * x * x);

        return envelope_.sustain * fastExp;
    }
}

float Voice::generateHarmonicWave(float freq, float phase) {
    // Revert to original fastSin calls for reliability
    float fundamental = fastSin(phase);

    // Major 3rd harmonic (determines major/minor character)
    float thirdPhase = phase * 1.25f;  // Major 3rd ratio
    float third = harmonics_.third * fastSin(thirdPhase);

    // Perfect 5th harmonic (adds musical richness)
    float fifthPhase = phase * 1.5f;   // Perfect 5th ratio
    float fifth = harmonics_.fifth * fastSin(fifthPhase);

    // Octave harmonic (adds fullness)
    float octavePhase = phase * 2.0f;  // Octave ratio
    float octave = harmonics_.octave * fastSin(octavePhase);

    // 9th harmonic (jazz color, subtle)
    float ninthPhase = phase * 2.25f;  // 9th ratio
    float ninth = harmonics_.ninth * fastSin(ninthPhase);

    // Combine harmonics - this creates the "rich tune" you wanted!
    float harmonicSum = fundamental + third + fifth + octave + ninth;
    
    static const float normalizationFactor = 1.0f / (1.0f + 0.3f + 0.2f + 0.15f + 0.1f); // Precomputed for performance
    return harmonicSum * normalizationFactor;
}

// ============================================================================
// LOFI ENGINE IMPLEMENTATION - Main Synthesis Controller
// ============================================================================

LofiEngine::LofiEngine() : drumVoices_{} // Ensure drumVoices_ is initialized in the constructor
    , currentProgression_(I_IV_I_IV)
    , sampleRate_(16000UL)
    , masterVolume_(3.f)
    , sampleCount_(0)
    , patternPosition_(0)
    , chordPosition_(0)
    , timePosition_(0.0f)
{
    initMutex();
    
    // Initialize lookup tables for optimized performance
    if (!tablesInitialized) initTables();

    // Initialize all voices
    for (int i = 0; i < 4; i++) {
        chordVoices_[i].setSampleRate(sampleRate_);
    }
    melodyVoice_.setSampleRate(sampleRate_);
    bassVoice_.setSampleRate(sampleRate_);
    for (int i = 0; i < 3; i++) {
        drumVoices_[i].setSampleRate(sampleRate_);
    }
    // Initialize scheduler
    schedHead_ = 0;
    schedTail_ = 0;
    for (int i = 0; i < SCHED_SIZE; ++i) {
        sched_[i].triggerSample = 0xFFFFFFFF;
        sched_[i].type = 0;
        sched_[i].freq = 0.0f;
        sched_[i].amp = 0.0f;
        sched_[i].length = 0.0f;
    }
}

LofiEngine::~LofiEngine() {
    cleanup();
}

bool LofiEngine::init(float sampleRate) {
    sampleRate_ = sampleRate;

    // Update all voice sample rates
    for (int i = 0; i < 4; i++) {
        chordVoices_[i].setSampleRate(sampleRate_);
    }
    melodyVoice_.setSampleRate(sampleRate_);
    bassVoice_.setSampleRate(sampleRate_);
    for (int i = 0; i < 3; i++) {
        drumVoices_[i].setSampleRate(sampleRate_);
    }

    // Generate default musical content
    generateChordProgression(I_IV_I_IV);
    generateRhythm(85.0f);  // Classic lofi tempo
    
    return true;
}

void LofiEngine::cleanup() {
    // Stop all voices
    for (int i = 0; i < 4; i++) {
        chordVoices_[i].noteOff();
    }
    melodyVoice_.noteOff();
    bassVoice_.noteOff();
    for (int i = 0; i < 3; i++) {
        drumVoices_[i].noteOff();
    }
}

bool LofiEngine::startMelody() {
    // Stop current playback
    cleanup();

    // Generate new musical content based on melody type
    // Randomly select a melody type
    int randomMelody = rand() % 4;

    switch (randomMelody) {
        case 0:
            generateChordProgression(I_IV_I_IV);
            generateMelody(0, chordSequence_, 4);  // Generate melody based on chords
            generateRhythm(75.0f);  // Slower tempo
            break;

        case 1:
            generateChordProgression(ii_V_I);
            generateMelody(0, chordSequence_, 3);  // Generate melody based on chords
            generateRhythm(95.0f);  // Slightly faster
            break;

        case 2:
            generateChordProgression(vi_IV_I_V);
            generateMelody(0, chordSequence_, 4);  // Generate melody based on chords
            generateRhythm(80.0f);  // Medium tempo
            break;

        case 3:
            generateChordProgression(I_V_vi_IV);
            generateMelody(0, chordSequence_, 4);  // Generate melody based on chords
            generateRhythm(88.0f);  // Classic cafe tempo
            break;

        default:
            generateChordProgression(I_IV_I_IV);
            generateMelody(0, chordSequence_, 4);  // Generate melody based on chords
            generateRhythm(85.0f);
            break;
    }

    // Reset playback state
    sampleCount_ = 0;
    patternPosition_ = 0;
    chordPosition_ = 0;
    timePosition_ = 0.0f;

    // Initialize variation engine with conservative defaults
    VariationConfig vcfg;
    vcfg.enabled = true;
    vcfg.phraseLengthBars = 4;
    vcfg.microSwingMs = 12.0f;
    vcfg.microSwingProb = 0.25f;
    vcfg.rngSeed = 0; // use esp_random-based seed by default
    variationEngine_.init(sampleRate_, vcfg);

    return true;
}

void LofiEngine::stopMelody() {
    cleanup();
}

bool LofiEngine::isPlaying() const {
    // Check if any voice is active
    for (const auto& voice : chordVoices_) {
        if (voice.isActive()) return true;
    }
    if (melodyVoice_.isActive() || bassVoice_.isActive()) return true;
    return false;
}

float LofiEngine::renderSample() {
    // --- Modular, pattern-driven, and variation-rich lofi logic ---
    // 1. Update time and sample rate reciprocal
    static float sampleRateRecip = 1.0f / sampleRate_;
    static float lastSampleRate = sampleRate_;
    if (sampleRate_ != lastSampleRate) {
        sampleRateRecip = 1.0f / sampleRate_;
        lastSampleRate = sampleRate_;
    }
    timePosition_ = sampleCount_ * sampleRateRecip;

    // 2. Real-time event scheduling (drums, etc)
    while (schedHead_ != schedTail_) {
        auto &ev = sched_[schedHead_];
        if (ev.triggerSample > sampleCount_) break;
        switch (ev.type) {
            case 0: drumVoices_[0].noteOn(ev.freq, ev.amp, ev.length); break;
            case 1: drumVoices_[1].noteOn(ev.freq, ev.amp, ev.length); break;
            case 2: drumVoices_[2].noteOn(ev.freq, ev.amp, ev.length); break;
        }
        schedHead_ = (schedHead_ + 1) % SCHED_SIZE;
    }

    // 3. Pattern and progression switching (dynamic, as in TS engine)
    if ((sampleCount_ & 0xF) == 0) {
        updateChordPlayback();
        updateMelodyPlayback();
        updateRhythmPlayback();
    }

    // 4. Mix all active voices (chords, melody, bass, drums)
    float output = 0.0f;
    for (int i = 0; i < 4; i++) {
        if (chordVoices_[i].isActive()) {
            output += chordVoices_[i].renderSample() * 1.2f;
        }
    }
    if (melodyVoice_.isActive()) {
        output += melodyVoice_.renderSample() * 1.2f;
    }
    if (bassVoice_.isActive()) {
        output += bassVoice_.renderSample() * 1.0f;
    }
    if (drumVoices_[0].isActive()) {
        output += drumVoices_[0].renderSample() * 1.5f;
    }
    if (drumVoices_[1].isActive()) {
        output += drumVoices_[1].renderSample() * 0.8f;
        output += drumVoices_[2].renderSample() * 0.6f;
    }

    // 5. Apply lofi filtering and vintage character
    output = applyLofiFilter(output);

    // 6. Update filter automation and tempo drift
    updateFilterAutomation();

    // 7. Increment sample count
    sampleCount_++;

    // 8. Dynamic master volume for breathing effect (as in TS/JS engine)
    float volumeBreathing = 1.0f + 0.15f * fastSin(timePosition_ * 0.01f);
    return output * masterVolume_ * volumeBreathing;
}

void LofiEngine::renderBuffer(float* buffer, size_t samples) {
    for (size_t i = 0; i < samples; ++i) {
        float sample = 0.0f;

        // Simulate piano, drums, and chords
        simulatePiano();
        // simulateDrums();
        // simulateChords();

        // Mix the generated samples
        sample += melodyVoice_.renderSample();
        // for (int j = 0; j < 3; ++j) {
        //     sample += drumVoices_[j].renderSample();
        // }
        // for (int j = 0; j < 4; ++j) {
        //     sample += chordVoices_[j].renderSample();
        // }

        // Apply master volume and write to buffer
        buffer[i] = sample * masterVolume_;
    }
}

// ============================================================================
// MUSICAL PATTERN GENERATION (Ported from JavaScript Intelligence)
// ============================================================================

void LofiEngine::generateChordProgression(ChordProgression prog) {
    // Define scales and keys dynamically
    const std::vector<int> majorScale = {0, 2, 4, 5, 7, 9, 11};
    const std::vector<std::string> keys = {"C", "D", "E", "F", "G", "A", "B"};

    // Randomly select a key
    int keyIndex = rand() % keys.size();
    int rootNote = 60 + keyIndex; // C4 as base MIDI note

    // Define chord intervals for a major scale
    const std::vector<std::vector<int>> chordIntervals = {
        {0, 4, 7},    // Major
        {2, 5, 9},    // Minor
        {4, 7, 11},   // Major 7th
        {5, 9, 12},   // Dominant 7th
        {7, 11, 14}   // Major 9th
    };

    // Generate a random progression
    std::vector<int> progression = {0, 3, 4, 1}; // I-IV-V-ii

    for (size_t i = 0; i < progression.size(); ++i) {
        int scaleDegree = progression[i];
        const auto& intervals = chordIntervals[scaleDegree % chordIntervals.size()];

        Chord chord;
        chord.type = TRIAD; // Simplified for now
        chord.duration = 2.0f;
        chord.noteCount = intervals.size();

        for (size_t j = 0; j < intervals.size(); ++j) {
            chord.notes[j] = LofiSynth::midiToFreq(rootNote + majorScale[scaleDegree] + intervals[j]);
        }

        // Voice the chord for richness
        MusicUtils::voiceChord(chord, true);

        // Store the chord in the sequence
        chordSequence_[i] = chord;
    }

    currentProgression_ = prog;
}

void LofiEngine::togglePlayback() {
    if (isPlaying()) {
        stopMelody();
    } else {
        startMelody();
    }
}

void LofiEngine::generateMelody(int key, const Chord* chords, int chordCount) {
    std::vector<int> scale = MusicUtils::getScale(key, false);  // Major scale
    int rangeStart = 72;  // C5 (higher octave for melody)
    int rangeEnd = 84;    // C6

    melody_.noteCount = 16;  // 16 notes in pattern

    for (int i = 0; i < melody_.noteCount; i++) {
        if (i % 4 == 0) {
            // Chord tones on strong beats
            int chordIndex = (i / 4) % chordCount;
            float chordFreq = chords[chordIndex].notes[0];
            // OPTIMIZATION: Avoid expensive log2f calculation - just octave up the existing frequency
            melody_.notes[i] = chordFreq * 2.0f;  // Simple octave up instead of MIDI conversion
            melody_.durations[i] = 0.5f;  // Half note
        } else if (i % 2 == 0) {
            // Scale tones on weak beats
            int scaleNote = scale[i % scale.size()] + rangeStart;
            melody_.notes[i] = noteToFrequency(scaleNote);
            melody_.durations[i] = 0.25f;  // Quarter note
        } else {
            // Rest or passing tone
            if (i % 3 == 0) {
                melody_.notes[i] = 0.0f;  // Rest
            } else {
                int passingNote = scale[(i + 1) % scale.size()] + rangeStart;
                melody_.notes[i] = noteToFrequency(passingNote);
            }
            melody_.durations[i] = 0.25f;
        }
    }
}

void LofiEngine::generateRhythm(float bpm) {
    rhythm_.bpm = bpm;

    // Classic lofi rhythm patterns from JavaScript analysis
    uint8_t lofiKick[] = {1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,1,0};
    uint8_t lofiSnare[] = {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0};
    uint8_t lofiHat[] = {1,1,0,1, 1,0,1,1, 1,1,0,1, 1,0,1,1};

    memcpy(rhythm_.kicks, lofiKick, 16);
    memcpy(rhythm_.snares, lofiSnare, 16);
    memcpy(rhythm_.hats, lofiHat, 16);
}

// ============================================================================
// AUDIO PROCESSING - Lofi Character & Filtering
// ============================================================================

float LofiEngine::applyLofiFilter(float sample) {
    static float lowpassState = 0.0f;
    static float highpassState = 0.0f;
    
    // OPTIMIZATION: Cache filter coefficients to avoid division every sample
    static float cachedLowpassAlpha = 0.0f;
    static float cachedHighpassAlpha = 0.0f;
    static float lastLowpassFreq = 0.0f;
    static float lastHighpassFreq = 0.0f;
    
    // Only recalculate coefficients when frequency changes
    if (filter_.lowpassFreq != lastLowpassFreq) {
        cachedLowpassAlpha = filter_.lowpassFreq / (filter_.lowpassFreq + sampleRate_);
        lastLowpassFreq = filter_.lowpassFreq;
    }
    if (filter_.highpassFreq != lastHighpassFreq) {
        cachedHighpassAlpha = filter_.highpassFreq / (filter_.highpassFreq + sampleRate_);
        lastHighpassFreq = filter_.highpassFreq;
    }

    // Lowpass filter (removes harsh highs)
    lowpassState += cachedLowpassAlpha * (sample - lowpassState);

    // Highpass filter (removes mud)  
    highpassState += cachedHighpassAlpha * (lowpassState - highpassState);

    float filtered = lowpassState - highpassState;
    
    // Add vintage saturation
    float x = filtered * (1.0f + filter_.vintage);
    // Use the fastTanh function from header
    filtered = fastTanh(x) / (1.0f + filter_.vintage);

    return filtered;
}

void LofiEngine::updateFilterAutomation() {
    // Revert to original implementation for reliability
    float time = timePosition_ * 0.1f;  // Slow modulation

    filter_.lowpassFreq = 500.0f + 200.0f * fastSin(time) + 100.0f * fastSin(time * 1.7f);
    filter_.highpassFreq = 80.0f + 40.0f * fastSin(time * 0.7f + 1.57f) + 20.0f * fastSin(time * 1.3f); // +1.57f for cos equivalent

    // ADD VARIATION: Subtle tempo drift for human feel
    static float baseBpm = rhythm_.bpm;
    float tempoVariation = 1.0f + 0.02f * fastSin(timePosition_ * 0.005f);  // ±2% tempo drift
    rhythm_.bpm = baseBpm * tempoVariation;
}

// ============================================================================
// PATTERN PLAYBACK - Real-time Music Generation
// ============================================================================

void LofiEngine::updateChordPlayback() {
    // Play chord progression in time with VARIATION
    float chordDuration = 60.0f / rhythm_.bpm * 4.0f;  // 4 beats per chord
    uint32_t chordsPerPattern = 4;

    uint32_t currentChord = (uint32_t)(timePosition_ / chordDuration) % chordsPerPattern;

    // ADD VARIATION: Change progression every 32 bars (2 minutes at 75 BPM)
    uint32_t progressionCycle = (uint32_t)(timePosition_ / (chordDuration * chordsPerPattern * 8)) % 4;
    static uint32_t lastProgressionCycle = 0;

    if (progressionCycle != lastProgressionCycle) {
        lastProgressionCycle = progressionCycle;

        // Cycle through different progressions for variety
        ChordProgression newProgression;
        switch (progressionCycle) {
            case 0: newProgression = I_IV_I_IV; break;      // Chill
            case 1: newProgression = vi_IV_I_V; break;      // Emotional
            case 2: newProgression = ii_V_I; break;         // Jazz
            case 3: newProgression = I_V_vi_IV; break;      // Pop
        }

        if (newProgression != currentProgression_) {
            generateChordProgression(newProgression);
            generateMelody(0, chordSequence_, 4);  // Regenerate melody for new chords
        }
    }

    if (currentChord != chordPosition_) {
        chordPosition_ = currentChord;
        
        Chord& chord = chordSequence_[currentChord];
        float dynamicVolume = 0.7f + 0.3f * fastSin(timePosition_ * 0.05f);  // Revert to original

        for (int i = 0; i < chord.noteCount && i < 4; i++) {
            chordVoices_[i].noteOn(chord.notes[i], dynamicVolume, chordDuration * 0.9f);
        }
    }
}

void LofiEngine::updateMelodyPlayback() {
    if (melody_.noteCount == 0) {
        return;  // No melody to play
    }
    
    static uint32_t lastMelodyRegen = 0;
    uint32_t melodyRegenCycle = (uint32_t)(timePosition_ / (60.0f / rhythm_.bpm * 16.0f));

    if (melodyRegenCycle != lastMelodyRegen && melodyRegenCycle % 4 == 0) {
        lastMelodyRegen = melodyRegenCycle;
        // Regenerate melody with slight variations
        generateMelody(0, chordSequence_, 4);
    }

    // Simple melody playback with RHYTHMIC VARIATION
    static uint32_t lastMelodyNote = 0;
    float noteDuration = 60.0f / rhythm_.bpm;  // Quarter note duration

    // ADD VARIATION: Sometimes play double-time or half-time
    float rhythmVariation = 1.0f;
    if ((int)(timePosition_ / 8.0f) % 3 == 0) {
        rhythmVariation = 0.5f;  // Double-time occasionally
    } else if ((int)(timePosition_ / 12.0f) % 4 == 0) {
        rhythmVariation = 2.0f;  // Half-time occasionally
    }

    uint32_t currentNote = (uint32_t)(timePosition_ / (noteDuration * rhythmVariation)) % melody_.noteCount;

    if (currentNote != lastMelodyNote) {
        lastMelodyNote = currentNote;

        if (melody_.notes[currentNote] > 0.0f) {
            float melodyVolume = 0.6f + 0.4f * fastSin(timePosition_ * 0.03f);
            float frequency = melody_.notes[currentNote];

            // Occasional octave variation for interest
            if ((int)(timePosition_) % 23 == 0) {
                frequency *= 2.0f;  // Octave up
            } else if ((int)(timePosition_) % 29 == 0) {
                frequency *= 0.5f;  // Octave down
            }

            melodyVoice_.noteOn(frequency, melodyVolume, melody_.durations[currentNote] * rhythmVariation);
        }
    }
}

void LofiEngine::updateRhythmPlayback() {
    float sixteenthDuration = 60.0f / rhythm_.bpm / 4.0f;  // 16th note duration
    uint32_t currentSixteenth = (uint32_t)(timePosition_ / sixteenthDuration) % 16;
    
    uint32_t rhythmVariation = (uint32_t)(timePosition_ / (sixteenthDuration * 32)) % 4;

    static uint32_t lastSixteenth = 0;
    if (currentSixteenth != lastSixteenth) {
        lastSixteenth = currentSixteenth;

        // detect bar boundary (4/4 bars of 16 sixteenths) and notify variation engine
        uint32_t currentBar = (uint32_t)(timePosition_ / (sixteenthDuration * 16));
        static uint32_t lastBar = 0xFFFFFFFF;
        if (currentBar != lastBar) {
            lastBar = currentBar;
            variationEngine_.onBarBoundary((int)currentBar);
        }

        // Base patterns with variations
        bool kickHit = false, snareHit = false, hatHit = false;

        switch (rhythmVariation) {
            case 0: // Standard lofi pattern
                kickHit = rhythm_.kicks[currentSixteenth];
                snareHit = rhythm_.snares[currentSixteenth];
                hatHit = rhythm_.hats[currentSixteenth];
                break;

            case 1: // Sparse pattern (more chill)
                kickHit = rhythm_.kicks[currentSixteenth] && (currentSixteenth % 2 == 0);
                snareHit = rhythm_.snares[currentSixteenth] && (currentSixteenth % 4 == 2);
                hatHit = rhythm_.hats[currentSixteenth] && (currentSixteenth % 3 != 0);
                break;

            case 2: // Busy pattern (more energy)
                kickHit = rhythm_.kicks[currentSixteenth] || (currentSixteenth % 6 == 1);
                snareHit = rhythm_.snares[currentSixteenth];
                hatHit = rhythm_.hats[currentSixteenth] || (currentSixteenth % 2 == 1);
                break;

            case 3: // Broken pattern (shuffle feel)
                kickHit = rhythm_.kicks[currentSixteenth] && (currentSixteenth % 3 != 1);
                snareHit = rhythm_.snares[currentSixteenth] && (currentSixteenth % 5 != 2);
                hatHit = rhythm_.hats[currentSixteenth];
                break;
        }
        
        float dynamicKick = 0.8f + 0.4f * fastSin(timePosition_ * 0.02f);
        float dynamicSnare = 0.7f + 0.3f * fastSin(timePosition_ * 0.03f + 1.57f); // +1.57f for cos equivalent
        float dynamicHat = 0.5f + 0.3f * fastSin(timePosition_ * 0.07f);

        // Occasional breaks for breathing room
        bool isBreak = ((int)(timePosition_ / 16.0f) % 8 == 7);
        if (isBreak) {
            dynamicKick *= 0.3f;
            dynamicSnare *= 0.1f;
            dynamicHat *= 0.5f;
        }

        // Trigger drum hits with variation (apply micro-timing offsets)
        auto scheduleNoteAt = [&](int type, float freq, float amp, float length, float offsetSec) {
            // Compute target sample. Negative offsets mean trigger immediately.
            int64_t offsetSamples = (int64_t)(offsetSec * sampleRate_);
            uint32_t targetSample = sampleCount_ + (offsetSamples > 0 ? (uint32_t)offsetSamples : 0u);

            // enqueue if space
            uint8_t nextTail = (schedTail_ + 1) % SCHED_SIZE;
            if (nextTail == schedHead_) {
                // buffer full, drop oldest (advance head)
                schedHead_ = (schedHead_ + 1) % SCHED_SIZE;
            }

            sched_[schedTail_].triggerSample = targetSample;
            sched_[schedTail_].type = (uint8_t)type;
            sched_[schedTail_].freq = freq;
            sched_[schedTail_].amp = amp;
            sched_[schedTail_].length = length;
            schedTail_ = nextTail;
        };

        if (kickHit) {
            float off = variationEngine_.processStepTimingOffset(currentSixteenth);
            scheduleNoteAt(0, 60.0f, dynamicKick, sixteenthDuration * 2, off);
        }
        if (snareHit) {
            float off = variationEngine_.processStepTimingOffset(currentSixteenth);
            scheduleNoteAt(1, 200.0f, dynamicSnare, sixteenthDuration, off);
        }
        if (hatHit) {
            float off = variationEngine_.processStepTimingOffset(currentSixteenth);
            scheduleNoteAt(2, 8000.0f, dynamicHat, sixteenthDuration * 0.5f, off);
        }
    }
}

// Add a function to simulate drum patterns using sample arrays
void LofiEngine::simulateDrums() {
    // Define a simple rhythm pattern
    RhythmPattern rhythm = MusicUtils::createLofiRhythm(85.0f); // 85 BPM
    float sixteenthDuration = 60.0f / rhythm.bpm / 4.0f; // 16th note duration
    // Play one step per call (for real-time)
    static int step = 0;
    if (rhythm.kicks[step]) {
        // Play kick sample (replace with your playback logic)
        for (unsigned int j = 0; j < kick_wav_len; ++j) {
            float sample = kick_wav[j] / 255.0f;
            drumVoices_[0].noteOn(sample, 1.0f, sixteenthDuration);
            drumVoices_[0].renderSample();
            drumVoices_[0].noteOff();
        }
    }
    if (rhythm.snares[step]) {
        for (unsigned int j = 0; j < snare_wav_len; ++j) {
            float sample = snare_wav[j] / 255.0f;
            drumVoices_[1].noteOn(sample, 0.8f, sixteenthDuration);
            drumVoices_[1].renderSample();
            drumVoices_[1].noteOff();
        }
    }
    if (rhythm.hats[step]) {
        for (unsigned int j = 0; j < hat_wav_len; ++j) {
            float sample = hat_wav[j] / 255.0f;
            drumVoices_[2].noteOn(sample, 0.5f, sixteenthDuration);
            drumVoices_[2].renderSample();
            drumVoices_[2].noteOff();
        }
    }
    step = (step + 1) % 16;
}

// Add a function to simulate piano sample playback
void LofiEngine::simulatePiano() {
    // Example: Play a C major arpeggio using the C4, E4, G4, C5 samples
    // Map: C4 = C1v1, E4 = C3v1, G4 = C5v1, C5 = C6v1 (approximate)
    static int noteStep = 0;
    const struct {
        const unsigned char* data;
        unsigned int len;
    } pianoSamples[] = {
        {piano_C1v1_wav, sizeof(piano_C1v1_wav)}, // C
        {piano_C3v1_wav, sizeof(piano_C3v1_wav)}, // E
        {piano_C5v1_wav, sizeof(piano_C5v1_wav)}, // G
        {piano_C6v1_wav, sizeof(piano_C6v1_wav)}  // C (octave)
    };
    // Play one note per call (for real-time)
    const float noteDuration = 0.25f; // quarter note
    for (unsigned int j = 0; j < pianoSamples[noteStep].len; ++j) {
        float sample = pianoSamples[noteStep].data[j] / 255.0f;
        melodyVoice_.noteOn(sample, 1.0f, noteDuration);
        melodyVoice_.renderSample();
        melodyVoice_.noteOff();
    }
    noteStep = (noteStep + 1) % 4;
}

// Add a function to simulate chord playback
void LofiEngine::simulateChords() {
    // Define a simple chord progression
    ChordProgression progression = I_IV_I_IV;
    generateChordProgression(progression);

    for (int i = 0; i < 4; ++i) { // Play 4 chords in the progression
        Chord& chord = chordSequence_[i];
        for (int j = 0; j < chord.noteCount; ++j) {
            chordVoices_[j].noteOn(chord.notes[j], 0.8f, chord.duration);
            chordVoices_[j].renderSample();
            chordVoices_[j].noteOff();
        }
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

float LofiEngine::noteToFrequency(int midiNote) {
    // OPTIMIZATION: Use the fast midiToFreq function from header instead of pow()
    return midiToFreq(midiNote);
}

void LofiEngine::initMutex() {
    if (!mutexInitialized_) {
        synthMutex_ = xSemaphoreCreateMutex();
        mutexInitialized_ = true;
    }
}

void LofiEngine::lockSynth() {
    if (synthMutex_ != nullptr) {
        xSemaphoreTake(synthMutex_, portMAX_DELAY);
    }
}

void LofiEngine::unlockSynth() {
    if (synthMutex_ != nullptr) {
        xSemaphoreGive(synthMutex_);
    }
}

// ============================================================================
// MUSIC UTILITIES - Helper Functions for Musical Intelligence
// ============================================================================

namespace MusicUtils {

void voiceChord(Chord& chord, bool spread) {
    if (!spread) return;

    // Spread chord across octaves for fuller sound (like JavaScript voicing)
    if (chord.noteCount >= 2) {
        chord.notes[1] *= 1.5f;  // Move up a 5th  
    }
    if (chord.noteCount >= 3) {
        chord.notes[2] *= 2.0f;  // Move up an octave
    }
    if (chord.noteCount >= 4) {
        chord.notes[3] *= 2.5f;  // Move up an octave + 5th
    }
}

std::vector<int> getScale(int key, bool minor) {
    std::vector<int> scale;

    if (minor) {
        // Natural minor scale intervals
        int intervals[] = {0, 2, 3, 5, 7, 8, 10};
        for (int i = 0; i < 7; i++) {
            scale.push_back((key + intervals[i]) % 12);
        }
    } else {
        // Major scale intervals  
        int intervals[] = {0, 2, 4, 5, 7, 9, 11};
        for (int i = 0; i < 7; i++) {
            scale.push_back((key + intervals[i]) % 12);
        }
    }

    return scale;
}

} // namespace MusicUtils

float midiToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f); // Standard MIDI to frequency conversion
}

} // namespace LofiSynth
