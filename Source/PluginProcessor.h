#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class CompSatAudioProcessor : public juce::AudioProcessor
{
public:
    CompSatAudioProcessor();
    ~CompSatAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CompSat"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor to draw the gain-reduction meter (dB, <= 0)
    std::atomic<float> grMeterDb { 0.0f };

private:
    void runCompressor (juce::AudioBuffer<float>& buffer);
    void runSaturator  (juce::AudioBuffer<float>& buffer);

    float load (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    static constexpr int oversamplingOrder = 2; // 2^2 = 4x oversampling for the saturator
    static constexpr int oversamplingFactor = 1 << oversamplingOrder;

    double sr = 44100.0;
    size_t maxBlock = 512;

    // Compressor state
    float grState = 0.0f; // smoothed gain reduction in dB (<= 0)
    juce::SmoothedValue<float> makeupSm, compMixSm;

    // Saturator state
    juce::dsp::Oversampling<float> oversampler { 2, oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::dsp::DryWetMixer<float> satMixer;
    juce::SmoothedValue<float> driveSm; // runs at the oversampled rate
    float dcX1[2] { 0.0f, 0.0f }, dcY1[2] { 0.0f, 0.0f };
    float dcR = 0.999f;

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::ProcessorDuplicator<Filter, Coeffs> toneFilter;
    float lastToneDb = 1000.0f; // force first update

    juce::SmoothedValue<float> outSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompSatAudioProcessor)
};
