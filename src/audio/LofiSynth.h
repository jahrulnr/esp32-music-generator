#pragma once

#include <Arduino.h>
#include <cmath>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace LofiSynth {

// ============================================================================
// MUSICAL CONSTANTS & TYPES (From JavaScript Analysis)
// ============================================================================

enum ChordProgression {
    I_IV_I_IV = 0,     // C-F-C-F (classic lofi, 40% weight in JS)
    ii_V_I = 1,        // Dm-G-C (jazz influence, 30% weight)
    vi_IV_I_V = 2,     // Am-F-C-G (emotional, 20% weight)  
    I_V_vi_IV = 3      // C-G-Am-F (pop, 10% weight)
};

// Variation engine config for lightweight phrase-based variations
struct VariationConfig {
    bool enabled = true;              // master switch
    uint8_t phraseLengthBars = 4;     // how many bars per phrase
    float microSwingMs = 12.0f;       // default micro-swing in milliseconds
    float microSwingProb = 0.25f;     // chance to apply micro-swing on a step
    uint32_t rngSeed = 0xC0FFEE;      // deterministic seed (0 means random)
};

class VariationEngine {
public:
    VariationEngine();
    void init(float sampleRate, const VariationConfig &cfg);
    // call at bar boundary (barIndex starts at 0)
    void onBarBoundary(int barIndex);
    // called per-step to apply timing offset (returns time offset in seconds)
    float processStepTimingOffset(int stepIndex);
    // quick tick for internal state; call from engine per sample or per step
    void tick();
private:
    VariationConfig cfg_;
    float sampleRate_;
    uint32_t rngState_;
    int currentBarInPhrase_;
    // schedule: whether current phrase has micro-swing enabled
    bool phraseMicroSwingActive_;
    uint32_t lfsr_next();
};

enum ChordType {
    TRIAD = 0,         // Basic 3-note chord (C-E-G)
    MAJ7TH = 1,        // Jazzy 4-note (C-E-G-B)
    MAJ6TH = 2,        // Sweet sound (C-E-G-A)
    ADD9 = 3,          // Dreamy (C-E-G-D)
    MIN7TH = 4,        // Mellow minor (C-Eb-G-Bb)
    FIRST_INV = 5,     // First inversion (E-G-C)
    SECOND_INV = 6     // Second inversion (G-C-E)
};

enum LofiMelody {
    CHILL_AMBIENCE = 0,
    JAZZY_PROGRESSION = 1,
    NOSTALGIC_MELODY = 2,
    UPBEAT_LOFI = 3,
    RAIN_MOOD = 4,
    COFFEE_SHOP = 5,
    STUDY_FOCUS = 6,
    SUNSET_VIBES = 7,
    STOP = 99
};

// ============================================================================
// SYNTHESIS PARAMETERS
// ============================================================================

struct ADSREnvelope {
    float attack;      // Time to reach peak (0.01-0.1s)
    float decay;       // Time to decay to sustain (0.1-0.5s)  
    float sustain;     // Sustain level (0.3-0.8)
    float release;     // Time to fade to zero (0.2-2.0s)
    
    // Default: Piano-like with vintage character
    ADSREnvelope() : attack(0.02f), decay(0.3f), sustain(0.6f), release(0.8f) {}
};

struct HarmonicProfile {
    float fundamental;  // Base frequency amplitude (1.0)
    float third;       // Major/minor 3rd (0.3)
    float fifth;       // Perfect 5th (0.2) 
    float octave;      // Octave harmonic (0.15)
    float ninth;       // 9th for jazz color (0.1)
    
    // OPTIMIZATION: Default values optimized for performance and musicality
    HarmonicProfile() : fundamental(1.0f), third(0.3f), fifth(0.2f), octave(0.15f), ninth(0.1f) {}
};

struct LofiFilter {
    float lowpassFreq;     // 300-700Hz sweep for vintage
    float highpassFreq;    // 40-120Hz to remove mud
    float resonance;       // Filter Q factor (0.7-2.0)
    float vintage;         // amount of saturation / warmth (0.0-1.0)
    
    LofiFilter() : lowpassFreq(500.0f), highpassFreq(80.0f), resonance(1.2f), vintage(0.3f) {}
};

// ============================================================================
// VOICE SYNTHESIS ENGINE  
// ============================================================================

class Voice {
private:
    float frequency_;
    float amplitude_;
    float phase_;
    float timePosition_;
    float noteLength_;
    bool active_;
    
    ADSREnvelope envelope_;
    HarmonicProfile harmonics_;
    
    // Internal state
    float sampleRate_;
    uint32_t sampleCount_;
    
public:
    Voice();
    ~Voice() = default;
    
    // Voice control
    void noteOn(float freq, float amp, float length);
    void noteOff();
    bool isActive() const { return active_; }
    
    // Audio generation
    float renderSample();
    void setSampleRate(float rate) { sampleRate_ = rate; }
    
    // Envelope and harmonic shaping
    void setEnvelope(const ADSREnvelope& env) { envelope_ = env; }
    void setHarmonics(const HarmonicProfile& harm) { harmonics_ = harm; }
    
private:
    float calculateEnvelope(float time);
    float generateHarmonicWave(float freq, float phase);
};

// ============================================================================
// CHORD & MELODY GENERATORS (From JS Musical Intelligence)
// ============================================================================

struct Chord {
    float notes[4];        // Up to 4-note chords
    uint8_t noteCount;     // Actual notes in chord
    float duration;        // Chord length in seconds
    ChordType type;        // Chord characteristics
    
    Chord() : noteCount(0), duration(2.0f), type(TRIAD) {
        for(int i = 0; i < 4; i++) notes[i] = 0.0f;
    }
};

struct MelodyPattern {
    float notes[32];       // Melody notes (0 = rest)
    float durations[32];   // Note lengths
    uint8_t noteCount;     // Pattern length
    
    MelodyPattern() : noteCount(0) {
        for(int i = 0; i < 32; i++) {
            notes[i] = 0.0f;
            durations[i] = 0.25f;  // Default quarter notes
        }
    }
};

struct RhythmPattern {
    uint8_t kicks[16];     // Kick pattern (0/1 for 16th notes)
    uint8_t snares[16];    // Snare pattern  
    uint8_t hats[16];      // Hi-hat pattern
    float bpm;             // Tempo
    
    RhythmPattern() : bpm(85.0f) {  // Typical lofi tempo
        for(int i = 0; i < 16; i++) {
            kicks[i] = 0;
            snares[i] = 0; 
            hats[i] = 0;
        }
    }
};

// ============================================================================
// MAIN LOFI SYNTHESIS ENGINE
// ============================================================================

class LofiEngine {
private:
    // Voice allocation
    Voice chordVoices_[4];     // 4-voice polyphonic chords
    Voice melodyVoice_;        // Lead melody line
    Voice bassVoice_;          // Bass line
    Voice drumVoices_[3];      // Kick, snare, hi-hat
    
    // Musical state
    ChordProgression currentProgression_;
    Chord chordSequence_[8];   // Max 8 chords in progression
    MelodyPattern melody_;
    RhythmPattern rhythm_;
    
    // Audio processing
    LofiFilter filter_;
    float sampleRate_;
    float masterVolume_;
    VariationEngine variationEngine_;
    
    // Timing and pattern control
    uint32_t sampleCount_;
    uint32_t patternPosition_;
    uint32_t chordPosition_;
    float timePosition_;
    // Simple scheduler for sample-accurate event dispatch (drum micro-timing)
    struct ScheduledEvent {
        uint32_t triggerSample; // absolute sample index to trigger
        uint8_t type;           // 0=kick,1=snare,2=hat
        float freq;
        float amp;
        float length;
    };
    static const int SCHED_SIZE = 64;
    ScheduledEvent sched_[SCHED_SIZE];
    uint8_t schedHead_;
    uint8_t schedTail_;
    
    // Thread safety (following Note.cpp pattern)
    static SemaphoreHandle_t synthMutex_;
    static bool mutexInitialized_;
    
public:
    LofiEngine();
    ~LofiEngine();
    
    // Engine control
    bool init(float sampleRate = 44100.0f);
    void cleanup();
    
    bool startMelody();
    void stopMelody();
    bool isPlaying() const;
    
    // Audio generation
    float renderSample();
    void renderBuffer(float* buffer, size_t samples);
    
    // Configuration
    void setMasterVolume(float volume) { masterVolume_ = volume; }
    void setFilter(const LofiFilter& filter) { filter_ = filter; }
    void setBPM(float bpm) { rhythm_.bpm = bpm; }
    void togglePlayback();
    void simulatePiano();
    void simulateDrums();
    void simulateChords();
    void testSimulations();
    
private:
    // Musical pattern generation (ported from JavaScript)
    void generateChordProgression(ChordProgression prog);
    void generateMelody(int key, const Chord* chords, int chordCount);
    void generateRhythm(float bpm);
    
    // Audio processing
    float applyLofiFilter(float sample);
    void updateFilterAutomation();
    
    // Pattern playback
    void updateChordPlayback();
    void updateMelodyPlayback();  
    void updateRhythmPlayback();
    
    // Utility functions
    float noteToFrequency(int midiNote);
    void initMutex();
    void lockSynth();
    void unlockSynth();
};

// ============================================================================
// HELPER FUNCTIONS (Musical Intelligence from JS)
// ============================================================================

namespace MusicUtils {
    // Chord generation
    Chord createChord(int rootNote, ChordType type, int octave = 4);
    void voiceChord(Chord& chord, bool spread = true);  // Spread across octaves
    
    // Scale and melody utilities  
    std::vector<int> getScale(int key, bool minor = false);  // Major/minor scales
    int quantizeToScale(int note, const std::vector<int>& scale);
    
    // Rhythm patterns (from JS analysis)
    RhythmPattern createLofiRhythm(float bpm);
    RhythmPattern createJazzRhythm(float bpm);
    RhythmPattern createChillRhythm(float bpm);
    
    // Random selection with weights (like JS choose() function)
    template<typename T>
    T weightedChoice(const std::vector<T>& options, const std::vector<float>& weights);
    
    // Timing utilities
    float bpmToSamplePeriod(float bpm, float subdivision = 4.0f);  // 4 = quarter notes
}

// utils
static float fastSin(float x) {
    // Normalize to [-PI, PI] range first
    while (x > 3.141592654f) x -= 6.283185307f;
    while (x < -3.141592654f) x += 6.283185307f;
    
    // Taylor series approximation: sin(x) ≈ x - x³/6 + x⁵/120
    float x2 = x * x;
    return x * (1.0f - x2 * (0.16666667f - x2 * 0.00833333f));
}

static float fastTanh(float x) {
    // Fast tanh approximation: tanh(x) ≈ x / (1 + |x|) for moderate values
    return (x > 0) ? x / (1.0f + x) : x / (1.0f - x);
}

float midiToFreq(int midiNote) ;

} // namespace LofiSynth
