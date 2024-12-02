#pragma once

#include <JuceHeader.h>
#include <deque>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    enum TransportState
    {
        Unprimed,
        Stopped,
        Starting,
        Stopping,
        Freezing
    };
    
    TransportState state;
    
    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void freezeButtonClicked();
    void transportStateChanged(TransportState newState);
    int getBufferPos(int start, int offset);
    int getBufferDist(int from, int to);
    
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> playSource;
    juce::AudioTransportSource transport;
    
    juce::TextButton openButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton freezeButton;
    
    juce::AudioSampleBuffer circularBuffer;
    juce::dsp::FFT fft;
    int circularBufferSize;
    int currentBufferReadIndex;
    int currentBufferWriteIndex;
    int forecast;
    bool thawing;
    bool justThawed;
    int samplesBeforeFadeIn;
    bool forecasting;
    int freezeSamples;
    float *samples;
    int samplesBeforeFadeOut;
    //==============================================================================
    // Your private member variables go here...


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
