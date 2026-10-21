#pragma once

#include "PluginProcessor.h"

class CompSatAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit CompSatAudioProcessorEditor (CompSatAudioProcessor&);
    ~CompSatAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& name);

    CompSatAudioProcessor& processor;

    Knob thresh, ratio, attack, release, knee, makeup, compMix;
    Knob drive, tone, satMix, output;

    juce::ComboBox satType, order;
    juce::Label satTypeLabel, orderLabel;
    std::unique_ptr<ComboBoxAttachment> satTypeAttachment, orderAttachment;

    float meterDb = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompSatAudioProcessorEditor)
};
