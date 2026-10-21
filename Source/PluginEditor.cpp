#include "PluginEditor.h"

namespace
{
    const juce::Colour bg      { 0xff16181d };
    const juce::Colour panel   { 0xff1f232b };
    const juce::Colour accentA { 0xff4fd1c5 }; // compressor
    const juce::Colour accentB { 0xffff9f4a }; // saturation
}

void CompSatAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
}

CompSatAudioProcessorEditor::CompSatAudioProcessorEditor (CompSatAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addKnob (thresh,  "thresh",  "THRESHOLD");
    addKnob (ratio,   "ratio",   "RATIO");
    addKnob (attack,  "attack",  "ATTACK");
    addKnob (release, "release", "RELEASE");
    addKnob (knee,    "knee",    "KNEE");
    addKnob (makeup,  "makeup",  "MAKEUP");
    addKnob (compMix, "compMix", "MIX");

    addKnob (drive,   "drive",   "DRIVE");
    addKnob (tone,    "tone",    "TONE");
    addKnob (satMix,  "satMix",  "MIX");
    addKnob (output,  "output",  "OUTPUT");

    for (auto* k : { &thresh, &ratio, &attack, &release, &knee, &makeup, &compMix })
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, accentA);
    for (auto* k : { &drive, &tone, &satMix })
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, accentB);
    output.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::white.withAlpha (0.8f));

    satType.addItemList ({ "Tape", "Tube", "Soft Clip" }, 1);
    order.addItemList ({ "Comp > Sat", "Sat > Comp" }, 1);
    addAndMakeVisible (satType);
    addAndMakeVisible (order);
    satTypeAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, "satType", satType);
    orderAttachment   = std::make_unique<ComboBoxAttachment> (processor.apvts, "order", order);

    for (auto* l : { &satTypeLabel, &orderLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        addAndMakeVisible (l);
    }
    satTypeLabel.setText ("TYPE", juce::dontSendNotification);
    orderLabel.setText ("ORDER", juce::dontSendNotification);

    setSize (780, 440);
    startTimerHz (30);
}

CompSatAudioProcessorEditor::~CompSatAudioProcessorEditor() { stopTimer(); }

void CompSatAudioProcessorEditor::timerCallback()
{
    const float target = processor.grMeterDb.load();
    // quick fall-back so the meter doesn't flicker
    meterDb = target < meterDb ? target : meterDb + (target - meterDb) * 0.25f;
    repaint (meterArea);
}

void CompSatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("COMPSAT", 20, 10, 200, 30, juce::Justification::centredLeft);

    auto compPanel = juce::Rectangle<float> (14.0f, 56.0f, (float) getWidth() - 28.0f, 170.0f);
    auto satPanel  = juce::Rectangle<float> (14.0f, 236.0f, (float) getWidth() - 28.0f, 190.0f);

    g.setColour (panel);
    g.fillRoundedRectangle (compPanel, 8.0f);
    g.fillRoundedRectangle (satPanel, 8.0f);

    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (accentA);
    g.drawText ("COMPRESSOR", compPanel.getX() + 12, compPanel.getY() + 6, 160, 18, juce::Justification::centredLeft);
    g.setColour (accentB);
    g.drawText ("SATURATION", satPanel.getX() + 12, satPanel.getY() + 6, 160, 18, juce::Justification::centredLeft);

    // Gain-reduction meter (0 .. -24 dB)
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (11.0f);
    g.drawText ("GR", meterArea.getX() - 28, meterArea.getY() - 2, 24, 16, juce::Justification::centredRight);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, -meterDb / 24.0f);
    g.setColour (accentA);
    g.fillRoundedRectangle (meterArea.toFloat().withWidth (meterArea.getWidth() * frac), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawText (juce::String (meterDb, 1) + " dB", meterArea.withTrimmedLeft (4), juce::Justification::centredLeft);
}

void CompSatAudioProcessorEditor::resized()
{
    meterArea = { getWidth() - 260, 16, 240, 16 };

    auto place = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (16));
        k.slider.setBounds (r);
    };

    // Compressor row: 7 knobs
    {
        auto row = juce::Rectangle<int> (24, 84, getWidth() - 48, 136);
        const int w = row.getWidth() / 7;
        for (auto* k : { &thresh, &ratio, &attack, &release, &knee, &makeup, &compMix })
            place (*k, row.removeFromLeft (w));
    }

    // Saturation row: drive, tone, mix, output + type/order combos
    {
        auto row = juce::Rectangle<int> (24, 264, getWidth() - 48, 150);
        const int w = row.getWidth() / 6;
        for (auto* k : { &drive, &tone, &satMix, &output })
            place (*k, row.removeFromLeft (w));

        auto typeCol = row.removeFromLeft (w).reduced (8, 0);
        satTypeLabel.setBounds (typeCol.removeFromTop (16));
        satType.setBounds (typeCol.removeFromTop (28).translated (0, 24));

        auto orderCol = row.reduced (8, 0);
        orderLabel.setBounds (orderCol.removeFromTop (16));
        order.setBounds (orderCol.removeFromTop (28).translated (0, 24));
    }
}
