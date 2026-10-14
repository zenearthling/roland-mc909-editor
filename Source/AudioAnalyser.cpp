#include "AudioAnalyser.h"
#include <cmath>

namespace
{
    /** Raise an atomic float to at least `v` (audio thread writes, the editor resets it to zero). */
    void raiseTo (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed))
        {}
    }

    float toDb (double meanSquare) noexcept
    {
        return meanSquare <= 1.0e-12 ? AudioAnalyser::kFloorDb : (float) (10.0 * std::log10 (meanSquare));
    }
}

AudioAnalyser::AudioAnalyser()
{
    for (auto& s : spectrum)  s.store (kFloorDb);
    for (auto& b : bandRms)   b.store (kFloorDb);
    for (auto& b : bandPeak)  b.store (0.0f);
    for (auto& c : chanPeak)  c.store (0.0f);
    prepare (48000.0);
}

void AudioAnalyser::prepare (double sampleRate)
{
    rate.store (sampleRate > 0.0 ? sampleRate : 48000.0);
    const double sr = rate.load();

    const juce::dsp::ProcessSpec spec { sr, 512u, 1u };
    splitA.prepare (spec);
    splitB.prepare (spec);
    appliedLow = xoverLow.load();
    appliedHigh = xoverHigh.load();
    splitA.setCutoffFrequency (appliedLow);
    splitB.setCutoffFrequency (appliedHigh);
    splitA.reset();
    splitB.reset();

    ring.fill (0.0f);
    ringPos = sinceFft = 0;
    for (auto& s : spectrum) s.store (kFloorDb);

    for (double& m : bandMeanSq) m = 0.0;
    for (auto& b : bandRms) b.store (kFloorDb);

    setUpKWeighting();
    blockSum = 0.0;
    blockCount = 0;
    blockLength = juce::jmax (1, (int) std::lround (sr * 0.1));
    for (double& h : blockHistory) h = 0.0;
    historyFill = historyPos = 0;
    momentary.store (kFloorDb);
    shortTerm.store (kFloorDb);
    silentSamples.store (1LL << 40);
}

// BS.1770 K-weighting: a high shelf, then a 38 Hz high-pass. Coefficients are worked out for the
// actual sample rate (the standard only prints them for 48 kHz).
void AudioAnalyser::setUpKWeighting()
{
    const double fs = rate.load();
    const double pi = juce::MathConstants<double>::pi;

    Biquad s;
    {
        const double f0 = 1681.974450955533, gain = 3.999843853973347, q = 0.7071752369554196;
        const double k = std::tan (pi * f0 / fs);
        const double vh = std::pow (10.0, gain / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;
        s.b0 = (vh + vb * k / q + k * k) / a0;
        s.b1 = 2.0 * (k * k - vh) / a0;
        s.b2 = (vh - vb * k / q + k * k) / a0;
        s.a1 = 2.0 * (k * k - 1.0) / a0;
        s.a2 = (1.0 - k / q + k * k) / a0;
    }

    Biquad h;
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double k = std::tan (pi * f0 / fs);
        const double a0 = 1.0 + k / q + k * k;
        h.b0 = 1.0; h.b1 = -2.0; h.b2 = 1.0;
        h.a1 = 2.0 * (k * k - 1.0) / a0;
        h.a2 = (1.0 - k / q + k * k) / a0;
    }

    for (int c = 0; c < 2; ++c)
    {
        shelf[c] = s;
        highPass[c] = h;
    }
}

void AudioAnalyser::pushSpectrumSample (float mono)
{
    ring[(size_t) ringPos] = mono;
    ringPos = (ringPos + 1) % kFftSize;

    if (++sinceFft >= kFftSize / 2)
    {
        sinceFft = 0;
        computeSpectrum();
    }
}

void AudioAnalyser::computeSpectrum()
{
    // oldest sample first
    for (int i = 0; i < kFftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % kFftSize)];
    std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) kFftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // A Hann-windowed sine of amplitude A gives a peak of A * N / 4, so this reads 0 dB for full scale.
    constexpr float scale = 4.0f / (float) kFftSize;
    for (int k = 0; k < kBins; ++k)
    {
        const float mag = fftData[(size_t) k] * scale;
        spectrum[(size_t) k].store (mag < 1.0e-6f ? kFloorDb : 20.0f * std::log10 (mag), std::memory_order_relaxed);
    }
}

void AudioAnalyser::process (const juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (n <= 0 || channels <= 0)
        return;

    const float lo = xoverLow.load (std::memory_order_relaxed), hi = xoverHigh.load (std::memory_order_relaxed);
    if (lo != appliedLow)  { splitA.setCutoffFrequency (lo); appliedLow = lo; }
    if (hi != appliedHigh) { splitB.setCutoffFrequency (hi); appliedHigh = hi; }

    const float* left = buffer.getReadPointer (0);
    const float* right = channels > 1 ? buffer.getReadPointer (1) : left;
    const double sr = rate.load (std::memory_order_relaxed);

    // about 300 ms of smoothing for the band RMS
    const double smooth = std::exp (-1.0 / (sr * 0.3));

    float peakL = 0.0f, peakR = 0.0f, bandPk[3] {};
    float blockMax = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float a = left[i], b = right[i];
        const float absA = std::abs (a), absB = std::abs (b);
        peakL = juce::jmax (peakL, absA);
        peakR = juce::jmax (peakR, absB);
        blockMax = juce::jmax (blockMax, absA, absB);

        const float mono = 0.5f * (a + b);
        pushSpectrumSample (mono);

        float low = 0.0f, rest = 0.0f, mid = 0.0f, high = 0.0f;
        splitA.processSample (0, mono, low, rest);
        splitB.processSample (0, rest, mid, high);
        const float band[3] { low, mid, high };
        for (int k = 0; k < 3; ++k)
        {
            bandPk[k] = juce::jmax (bandPk[k], std::abs (band[k]));
            bandMeanSq[k] = bandMeanSq[k] * smooth + (double) band[k] * (double) band[k] * (1.0 - smooth);
        }

        const double ya = highPass[0].process (shelf[0].process ((double) a));
        const double yb = highPass[1].process (shelf[1].process ((double) b));
        blockSum += ya * ya + yb * yb;

        if (++blockCount >= blockLength)
        {
            blockHistory[historyPos] = blockSum / (double) blockLength;
            historyPos = (historyPos + 1) % 30;
            historyFill = juce::jmin (30, historyFill + 1);
            blockSum = 0.0;
            blockCount = 0;

            auto mean = [this] (int count)
            {
                double sum = 0.0;
                for (int j = 1; j <= count; ++j)
                    sum += blockHistory[(historyPos - j + 60) % 30];
                return sum / (double) count;
            };

            const int m = juce::jmin (4, historyFill);
            const double ms = mean (m), ss = mean (historyFill);
            momentary.store (ms < 1.0e-10 ? kFloorDb : (float) (-0.691 + 10.0 * std::log10 (ms)), std::memory_order_relaxed);
            shortTerm.store (ss < 1.0e-10 ? kFloorDb : (float) (-0.691 + 10.0 * std::log10 (ss)), std::memory_order_relaxed);
        }
    }

    for (int k = 0; k < 3; ++k)
    {
        bandRms[(size_t) k].store (toDb (bandMeanSq[k]), std::memory_order_relaxed);
        raiseTo (bandPeak[(size_t) k], bandPk[k]);
    }
    raiseTo (chanPeak[0], peakL);
    raiseTo (chanPeak[1], peakR);

    if (blockMax > 1.0e-5f)
        silentSamples.store (0, std::memory_order_relaxed);
    else
        silentSamples.store (juce::jmin (silentSamples.load (std::memory_order_relaxed) + n, 1LL << 40), std::memory_order_relaxed);
}
