#pragma once
#include <juce_core/juce_core.h>
#include <vector>

/** Effect type names and the per-type parameter labels printed in the MC-909 Owner's Manual
    ("Multi-Effects List", pp. 92-107, and "Reverb", pp. 90-91). The effect parameters themselves are
    stored as raw 12768..52768 values; the manual gives each one a label and a displayed range. */
namespace fxdata
{
    struct Param
    {
        enum class Kind { integer, choice, free };

        juce::String label;
        Kind kind = Kind::free;
        int lo = 0, hi = 0;            ///< integer: displayed range. choice: 0..options-1
        juce::StringArray options;     ///< choice
        juce::String unit;             ///< integer: "dB", "%", ...
        juce::String hint;             ///< free: range as printed in the manual
    };

    /** Value 0 is "OFF"; values 1..47 follow the manual's numbered list. */
    const juce::StringArray& mfxTypeNames();      // 48 entries
    const juce::StringArray& reverbTypeNames();   // 5 entries

    /** Parameters of an MFX type (value 1..47) or reverb type (value 1..4). Empty for OFF / out of range. */
    const std::vector<Param>& mfxParams (int type);
    const std::vector<Param>& reverbParams (int type);

    /** Short explanation for a parameter label, or empty. */
    juce::String labelHelp (const juce::String& label);
}
