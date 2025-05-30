#pragma once

/**
 * Utils for converting delay parameters
 * to tempo-synced delay times.
 */
namespace TempoSyncUtils
{
/** A simple struct containing a rhythmic delay length */
struct DelayRhythm
{
    constexpr DelayRhythm (const std::string_view& name, const std::string_view& label, double tempoFactor) : name (name),
                                                                                                              label (label),
                                                                                                              tempoFactor (tempoFactor) {}

    inline juce::String getLabel() const { return juce::String (static_cast<std::string> (label)); }

    std::string_view name;
    std::string_view label;
    double tempoFactor;
};

static constexpr std::array<DelayRhythm, 19> rhythms {
    DelayRhythm ("Thirty-Second", "1/32", 0.125),
    DelayRhythm ("Sixteenth", "1/16", 0.25),
    DelayRhythm ("Sixteenth Dot", "1/16 D", 0.25 * 1.5),
    DelayRhythm ("Eigth Triplet", "1/8 T", 1.0 / 3.0),
    DelayRhythm ("Eigth", "1/8", 0.5),
    DelayRhythm ("Eigth Dot", "1/8 D", 0.5 * 1.5),
    DelayRhythm ("Quarter Triplet", "1/4 T", 2.0 / 3.0),
    DelayRhythm ("Quarter", "1/4", 1.0),
    DelayRhythm ("Quarter Dot", "1/4 D", 1.0 * 1.5),
    DelayRhythm ("Half", "1/2", 2.0),
    DelayRhythm ("Whole", "1/1", 4.0),
    DelayRhythm ("Whole Triplet", "1/1 T", 8.0 / 3.0),
    DelayRhythm ("Two Whole", "2/1", 8.0),
    DelayRhythm ("Whole Dot", "1/1 D", 4.0 * 1.5),
    DelayRhythm ("Four Whole", "4/1", 16.0),
    DelayRhythm ("Four Dot", "4/1", 16.0 * 1.5),
    DelayRhythm ("Four Dot", "4/1", 16.0 * 1.5),
    DelayRhythm ("Four Triplet", "4/1 T", 32.0 / 3.0),
    DelayRhythm ("Eight Whole", "8/1", 32.0),
};

/** Return time in seconds for rhythm and tempo */
static inline double getTimeForRythm (double tempoBPM, const DelayRhythm& rhythm)
{
    const auto beatLength = 1.0 / (tempoBPM / 60.0);
    return beatLength * rhythm.tempoFactor;
}

/** Returns the corresponding rhythm for a 0-1 param value */
static inline const DelayRhythm& getRhythmForParam (float param01)
{
    auto idx = static_cast<size_t> ((rhythms.size() - 1) * std::pow (param01, 1.5f));
    return rhythms[idx];
}

} // namespace TempoSyncUtils
