#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CompSatAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pid = [] (const char* s) { return ParameterID { s, 1 }; };
    auto fp  = [] (const char* label) { return AudioParameterFloatAttributes().withLabel (label); };

    // --- Compressor ---
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("thresh"), "Threshold",
        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f, fp ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("ratio"), "Ratio",
        NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f), 4.0f, fp (":1")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("attack"), "Attack",
        NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.4f), 10.0f, fp ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("release"), "Release",
        NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 120.0f, fp ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("knee"), "Knee",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, fp ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("makeup"), "Makeup",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, fp ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("compMix"), "Comp Mix",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, fp ("%")));

    // --- Saturation ---
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("drive"), "Drive",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, fp ("dB")));
    p.push_back (std::make_unique<AudioParameterChoice> (pid ("satType"), "Sat Type",
        StringArray { "Tape", "Tube", "Soft Clip" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("tone"), "Tone",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f, fp ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("satMix"), "Sat Mix",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, fp ("%")));

    // --- Global ---
    p.push_back (std::make_unique<AudioParameterChoice> (pid ("order"), "Order",
        StringArray { "Comp > Sat", "Sat > Comp" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (pid ("output"), "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, fp ("dB")));

    return { p.begin(), p.end() };
}

//==============================================================================
CompSatAudioProcessor::CompSatAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool CompSatAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CompSatAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = (size_t) juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };

    oversampler.initProcessing (maxBlock);
    oversampler.reset();

    satMixer.prepare (spec);
    satMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    satMixer.setWetLatency (oversampler.getLatencyInSamples());

    toneFilter.prepare (spec);
    toneFilter.reset();
    lastToneDb = 1000.0f;

    setLatencySamples (juce::roundToInt (oversampler.getLatencyInSamples()));

    makeupSm.reset (sampleRate, 0.02);
    compMixSm.reset (sampleRate, 0.02);
    outSm.reset (sampleRate, 0.02);
    driveSm.reset (sampleRate * oversamplingFactor, 0.02);

    makeupSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (load ("makeup")));
    compMixSm.setCurrentAndTargetValue (load ("compMix") * 0.01f);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (load ("output")));
    driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (load ("drive")));

    grState = 0.0f;
    for (int i = 0; i < 2; ++i) { dcX1[i] = 0.0f; dcY1[i] = 0.0f; }
    dcR = 1.0f - (juce::MathConstants<float>::twoPi * 8.0f / (float) sampleRate); // ~8 Hz DC blocker
}

//==============================================================================
void CompSatAudioProcessor::runCompressor (juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    const float threshold = load ("thresh");
    const float ratio     = load ("ratio");
    const float knee      = load ("knee");
    const float atkMs     = load ("attack");
    const float relMs     = load ("release");

    const float atkCoef = std::exp (-1.0f / (0.001f * atkMs * (float) sr));
    const float relCoef = std::exp (-1.0f / (0.001f * relMs * (float) sr));
    const float slope   = 1.0f / ratio - 1.0f;

    makeupSm.setTargetValue (juce::Decibels::decibelsToGain (load ("makeup")));
    compMixSm.setTargetValue (load ("compMix") * 0.01f);

    float minGr = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Stereo-linked peak detection
        float level = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            level = juce::jmax (level, std::abs (buffer.getReadPointer (ch)[i]));

        const float xDb = juce::Decibels::gainToDecibels (level, -100.0f);
        const float over = xDb - threshold;

        // Soft-knee gain computer: returns gain change in dB (<= 0)
        float grDb;
        if (2.0f * over < -knee)
            grDb = 0.0f;
        else if (knee > 0.0f && 2.0f * std::abs (over) <= knee)
        {
            const float t = over + knee * 0.5f;
            grDb = slope * t * t / (2.0f * knee);
        }
        else
            grDb = slope * over;

        // Attack when reduction increases, release when it recovers
        const float coef = grDb < grState ? atkCoef : relCoef;
        grState = coef * grState + (1.0f - coef) * grDb;
        minGr = juce::jmin (minGr, grState);

        const float mix = compMixSm.getNextValue();
        const float gain = juce::Decibels::decibelsToGain (grState) * makeupSm.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const float dry = d[i];
            d[i] = dry + mix * (dry * gain - dry);
        }
    }

    grMeterDb.store (minGr);
}

//==============================================================================
static inline float shape (float x, int type, float d)
{
    switch (type)
    {
        case 0: // Tape: smooth arctan curve, mostly odd harmonics
            return std::atan (1.6f * d * x) * (2.0f / juce::MathConstants<float>::pi) * 1.02f;

        case 1: // Tube: asymmetric tanh, adds even harmonics (DC removed afterwards)
        {
            constexpr float bias = 0.25f;
            return (std::tanh (d * x + bias) - std::tanh (bias)) * 1.06f;
        }

        default: // Soft clip: cubic, unity slope near zero, hard ceiling at +/-1
        {
            const float v = juce::jlimit (-1.5f, 1.5f, d * x);
            return v - (4.0f / 27.0f) * v * v * v;
        }
    }
}

void CompSatAudioProcessor::runSaturator (juce::AudioBuffer<float>& buffer)
{
    const int type = (int) load ("satType");

    driveSm.setTargetValue (juce::Decibels::decibelsToGain (load ("drive")));
    satMixer.setWetMixProportion (load ("satMix") * 0.01f);

    const float toneDb = load ("tone") * 0.01f * 6.0f; // +/- 6 dB high shelf @ 3 kHz
    if (std::abs (toneDb - lastToneDb) > 0.001f)
    {
        *toneFilter.state = *Coeffs::makeHighShelf (sr, 3000.0f, 0.707f,
                                                    juce::Decibels::decibelsToGain (toneDb));
        lastToneDb = toneDb;
    }

    juce::dsp::AudioBlock<float> full (buffer);
    const size_t total = full.getNumSamples();
    const size_t numCh = full.getNumChannels();

    for (size_t start = 0; start < total; start += maxBlock)
    {
        const size_t len = juce::jmin (maxBlock, total - start);
        auto blk = full.getSubBlock (start, len);

        satMixer.pushDrySamples (blk);

        auto up = oversampler.processSamplesUp (blk);
        const size_t upLen = up.getNumSamples();

        for (size_t i = 0; i < upLen; ++i)
        {
            const float d = driveSm.getNextValue();
            const float comp = 1.0f / std::sqrt (d); // rough auto-gain so Drive isn't just "louder"

            for (size_t ch = 0; ch < numCh; ++ch)
            {
                auto* s = up.getChannelPointer (ch);
                s[i] = shape (s[i], type, d) * comp;
            }
        }

        oversampler.processSamplesDown (blk);

        // DC blocker (needed for the asymmetric Tube mode)
        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* s = blk.getChannelPointer (ch);
            for (size_t i = 0; i < len; ++i)
            {
                const float x = s[i];
                const float y = x - dcX1[ch] + dcR * dcY1[ch];
                dcX1[ch] = x;
                dcY1[ch] = y;
                s[i] = y;
            }
        }

        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        toneFilter.process (ctx);

        satMixer.mixWetSamples (blk);
    }
}

//==============================================================================
void CompSatAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const bool compFirst = (int) load ("order") == 0;

    if (compFirst) { runCompressor (buffer); runSaturator (buffer); }
    else           { runSaturator (buffer);  runCompressor (buffer); }

    outSm.setTargetValue (juce::Decibels::decibelsToGain (load ("output")));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float g = outSm.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

//==============================================================================
juce::AudioProcessorEditor* CompSatAudioProcessor::createEditor()
{
    return new CompSatAudioProcessorEditor (*this);
}

void CompSatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CompSatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompSatAudioProcessor();
}
