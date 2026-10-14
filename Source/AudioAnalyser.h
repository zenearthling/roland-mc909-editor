#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/** Looks at the audio passing through the plugin and keeps the numbers the Master tab draws:
    a spectrum, the level of each of the three bands, left/right peaks and loudness.

    It only reads the audio, never changes it. process() runs on the audio thread and writes
    atomics; everything the editor reads is an atomic, so no locks are involved.

    With the MC-909's audio routed through the track the plugin sits on, this is what comes BACK from
    the unit, i.e. after its own compressor. The signal before the compressor is not available, so
    gain reduction cannot be measured, only the result. */
class AudioAnalyser
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kBins     = kFftSize / 2;
    static constexpr float kFloorDb = -120.0f;

    AudioAnalyser();

    /** Not on the audio thread while process() is running. */
    void prepare (double sampleRate);

    /** Audio thread. Reads the buffer, does not modify it. */
    void process (const juce::AudioBuffer<float>& buffer);

    /** Any thread: where the low/mid and mid/high crossovers are, as set on the MC-909. */
    void setCrossovers (double lowHz, double highHz)
    {
        xoverLow.store ((float) lowHz);
        xoverHigh.store ((float) highHz);
    }

    //==========================================================================
    // Editor side. All cheap, all lock-free.

    double sampleRate() const noexcept { return rate.load(); }

    /** Magnitude of FFT bin 0..kBins-1 in dBFS (a full-scale sine reads about 0). */
    float spectrumDb (int bin) const noexcept { return spectrum[(size_t) bin].load (std::memory_order_relaxed); }

    /** Smoothed RMS of a band (0 low, 1 mid, 2 high) in dBFS, taken on the mono sum. */
    float bandRmsDb (int band) const noexcept { return bandRms[(size_t) band].load (std::memory_order_relaxed); }

    /** Largest sample of the band since the last call, linear. Resets as it is read, so give it ONE reader. */
    float takeBandPeak (int band) noexcept { return bandPeak[(size_t) band].exchange (0.0f); }

    /** Same for the left (0) and right (1) channel. */
    float takePeak (int channel) noexcept { return chanPeak[(size_t) channel].exchange (0.0f); }

    /** ITU-R BS.1770 K-weighted loudness over the last 0.4 s / 3 s, in LUFS (kFloorDb when silent). */
    float momentaryLufs() const noexcept { return momentary.load (std::memory_order_relaxed); }
    float shortTermLufs() const noexcept { return shortTerm.load (std::memory_order_relaxed); }

    /** False once nothing but silence has arrived for about two seconds. */
    bool hasSignal() const noexcept { return silentSamples.load (std::memory_order_relaxed) < (long long) (rate.load() * 2.0); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void setUpKWeighting();
    void pushSpectrumSample (float mono);
    void computeSpectrum();

    std::atomic<double> rate { 48000.0 };
    std::atomic<float> xoverLow { 400.0f }, xoverHigh { 4000.0f };

    // spectrum
    juce::dsp::FFT fft { kFftOrder };
    // not normalised: computeSpectrum() applies the scaling itself, so a full-scale sine reads 0 dB
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::array<float, kFftSize> ring {};
    std::array<float, kFftSize * 2> fftData {};
    int ringPos = 0, sinceFft = 0;
    std::array<std::atomic<float>, kBins> spectrum;

    // bands: mono sum split with two 4th-order Linkwitz-Riley crossovers
    juce::dsp::LinkwitzRileyFilter<float> splitA, splitB;
    float appliedLow = 0.0f, appliedHigh = 0.0f;
    double bandMeanSq[3] {};
    std::array<std::atomic<float>, 3> bandRms, bandPeak;
    std::array<std::atomic<float>, 2> chanPeak;

    // loudness
    Biquad shelf[2], highPass[2];
    double blockSum = 0.0;
    int blockCount = 0, blockLength = 4800;
    double blockHistory[30] {};
    int historyFill = 0, historyPos = 0;
    std::atomic<float> momentary { kFloorDb }, shortTerm { kFloorDb };

    std::atomic<long long> silentSamples { 1LL << 40 };
};
