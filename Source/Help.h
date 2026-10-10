#pragma once
#include <juce_core/juce_core.h>
#include "MC909Map.h"

/** Plain-English descriptions for the editor's Help mode. Texts follow the
    MC-909 Owner's Manual; where the manual does not describe a parameter in a
    way that can be stated safely, the text says so rather than guessing. */
namespace help
{
    /** One-line "what values are allowed" summary built from the definition. */
    juce::String rangeLine (const mc909::ParamDef&);

    /** What the parameter does. Never empty. */
    juce::String describe (const mc909::ParamDef&);
}
