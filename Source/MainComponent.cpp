#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() 
    : state(Unprimed),
      openButton("Open"),
      playButton("Play"),
      stopButton("Stop"),
      freezeButton("Freeze"),
      circularBufferSize(0),
      currentBufferReadIndex(0),
      currentBufferWriteIndex(0),
      forecast(0),
      thawing(false),
      justThawed(false),
      forecasting(false),
      freezeDuration(0.3),
      samples(nullptr),
      samplesBeforeFadeOut(0)
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (200, 200);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (0, 2);
    }
    
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(&openButton);
    
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setEnabled(false);
    addAndMakeVisible(&playButton);
    
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.setEnabled(false);
    addAndMakeVisible(&stopButton);
    
    freezeButton.onClick = [this] { freezeButtonClicked(); };
    freezeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lightblue);
    freezeButton.setEnabled(false);
    addAndMakeVisible(&freezeButton);

    formatManager.registerBasicFormats();
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    circularBufferSize = static_cast<int>(freezeDuration * sampleRate);
    circularBuffer.setSize(2, circularBufferSize); // Stereo (2 channels)
    samples = new float[circularBufferSize];
    
    // juce error: noramlisation param is inverted?
    juce::dsp::WindowingFunction<float>::fillWindowingTables(samples, circularBufferSize, juce::dsp::WindowingFunction<float>::hann, false);
    circularBuffer.clear();
    currentBufferWriteIndex = 0;
    currentBufferReadIndex = 0;
    transport.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::openButtonClicked()
{
    // Choose a file
    juce::FileChooser chooser ("Choose a wav or aiff file", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav; *.aiff; *.mp3");
    // If the user chooses a file
    if (chooser.browseForFileToOpen()) {
        juce::File myFile = chooser.getResult();
        // Read file
        juce::AudioFormatReader* reader = formatManager.createReaderFor(myFile);
        
        if (reader)
        {
            std::unique_ptr<juce::AudioFormatReaderSource> tempSource (new juce::AudioFormatReaderSource (reader, true));
            
            transport.setSource(tempSource.get());
            transportStateChanged(Stopped);
            
            playSource.reset(tempSource.release());
        }
    }
}

void MainComponent::playButtonClicked()
{
    transportStateChanged(Starting);
}

void MainComponent::stopButtonClicked()
{
    transportStateChanged(Stopping);
}

void MainComponent::freezeButtonClicked()
{
    transportStateChanged(Freezing);
}

void MainComponent::transportStateChanged(TransportState newState)
{
    if (newState == state) return;
    
    TransportState oldState = state;
    state = newState;
    
    // Update UI state
    switch (state) {
        case Unprimed:
            stopButton.setEnabled(false);
            playButton.setEnabled(false);
            freezeButton.setEnabled(false);
            break;
        case Stopped:
            stopButton.setEnabled(false);
            playButton.setEnabled(true);
            freezeButton.setEnabled(false);
            transport.setPosition(0);
            break;
        case Starting:
            if (oldState == Freezing) thawing = true;
            stopButton.setEnabled(true);
            freezeButton.setEnabled(true);
            playButton.setEnabled(false);
            transport.start();
            break;
        case Stopping:
            stopButton.setEnabled(false);
            freezeButton.setEnabled(false);
            playButton.setEnabled(true);
            transport.stop();
            break;
        case Freezing:
            stopButton.setEnabled(true);
            playButton.setEnabled(true);
            freezeButton.setEnabled(false);
            forecasting = true;
            break;
    }
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;
    int numSamples = bufferToFill.numSamples;
    
    if (thawing) {
        samplesBeforeFadeIn = getBufferDist(currentBufferReadIndex, currentBufferWriteIndex);
        
        // if we are on the last numSamples samples in our circle buffer
        if (samplesBeforeFadeIn < numSamples) {
            // set the read pos in the file based on remaining samples in the circular buffer
            long long lastPos = transport.getNextReadPosition();
            long long curPos = lastPos - samplesBeforeFadeIn;
            transport.setNextReadPosition(curPos);

            // set the new write pos to the circular buffer for the next time we read in more samples
            currentBufferWriteIndex = getBufferPos(currentBufferWriteIndex, -samplesBeforeFadeIn);
            
            thawing = false;
            justThawed = true;
        }
    }
    
    // if this is first iteration of forecasting
    if (forecasting && forecast == 0) {
        samplesBeforeFadeOut = (circularBufferSize / 2) % numSamples;
    }
    
    // read next numsamples from the circular buffer
    if (!forecasting && (state == Freezing || thawing)) {
        
        // for all audio channels, for each sample
        for (int channel = 0; channel < buffer->getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                // read the first sample value out of the circular buffer, multiply by window val
                int readIndex = (currentBufferReadIndex + sample) % circularBufferSize;
                int windowIdx = getBufferDist((currentBufferWriteIndex + 1) % circularBufferSize, readIndex);
                float windowVal = samples[windowIdx];
                float sampleValue = circularBuffer.getSample(channel, readIndex) * windowVal;
                
                // read the second sample value out of the circular buffer, multiply by window val
                int secondReadIndex = (readIndex + circularBufferSize / 2) % circularBufferSize;
                float secondWindowVal = 1.0 - windowVal;
                float secondSampleValue = circularBuffer.getSample(channel, secondReadIndex) * secondWindowVal;
                
                buffer->setSample(channel, sample, sampleValue + secondSampleValue);
                
//                DBG("first window: " + std::to_string(windowVal));
//                DBG("second window: " + std::to_string(secondWindowVal));
            }
        }
    
        currentBufferReadIndex = (currentBufferReadIndex + numSamples) % circularBufferSize;
    } 
    else // read next numsamples from the file into the buffer, write them to the circle buffer
    {
        // read from file, if no modifications are made later (i.e. forecasting or justThawed), sends audio to output buffer
        transport.getNextAudioBlock(bufferToFill);

        // write to circle buffer
        for (int channel = 0; channel < buffer->getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                if (forecasting) {
                    // fade out: multiply by the window, or 1 if we are before the window kicks in
                    int fadeOutWindowIdx = circularBufferSize / 2 + forecast + sample - samplesBeforeFadeOut;
                    float fadeOutWindowVal = (fadeOutWindowIdx >= circularBufferSize / 2) ? samples[fadeOutWindowIdx] : 1.0;
                    float fadingOutSample = buffer->getSample(channel, sample) * fadeOutWindowVal;
                    
                    // fade in the first half of the buffer
                    float fadeInWindowVal = 1.0 - fadeOutWindowVal;
                    int fadeInSampleIdx = (currentBufferWriteIndex + 1 + circularBufferSize / 2 + sample) % circularBufferSize;
                    float fadingInSample = circularBuffer.getSample(channel, fadeInSampleIdx) * fadeInWindowVal;
                    
                    buffer->setSample(channel, sample, fadingOutSample + fadingInSample);
                }
                
                if (justThawed) {
                    
                    if (getBufferDist(currentBufferReadIndex, currentBufferWriteIndex) > circularBufferSize / 2)
                    {
                        justThawed = false;
                    }
                    
                    int fadeOutSampleIdx = (currentBufferWriteIndex + circularBufferSize / 2 + sample) % circularBufferSize;
                    int progressSinceThawing = getBufferDist(currentBufferReadIndex, currentBufferWriteIndex);
                    int windowIdx = circularBufferSize / 2 - samplesBeforeFadeIn + progressSinceThawing;
                    float windowVal = (windowIdx < circularBufferSize) ? samples[windowIdx] : 0.0;
                    float fadeOutSampleVal = circularBuffer.getSample(channel, fadeOutSampleIdx) * windowVal;
                    
                    float sampleVal = buffer->getSample(channel, sample) * (1 - windowVal);
                    
                    buffer->setSample(channel, sample, fadeOutSampleVal + sampleVal);
                }
                
                int writeIndex = (currentBufferWriteIndex + sample) % circularBufferSize;
                circularBuffer.setSample(channel, writeIndex, buffer->getSample(channel, sample));
            }
        }
        
        currentBufferWriteIndex = (currentBufferWriteIndex + numSamples) % circularBufferSize;
        
        // inc count of forecasted samples and check if its time to freeze
        if (forecasting) {
            forecast += numSamples;
            if (forecast > circularBufferSize / 2) {
                // reset read idx to start of buffer, stop forecasting
                currentBufferReadIndex = (currentBufferWriteIndex + 1) % circularBufferSize;
                forecasting = false;
                forecast = 0;
                return;
            }
        }
    }
}



// return the positive index at an arbitrary positive or negative offset
// from an arbitrary start index
int MainComponent::getBufferPos(int start, int offset) {
    int absolutePos = start + offset;
    int wrappedPos = absolutePos % circularBufferSize;
    return (wrappedPos < 0) ? wrappedPos + circularBufferSize : wrappedPos;
}

// distance in the forward direction from one circular buffer index to another
int MainComponent::getBufferDist(int from, int to) {
    int dist = to - from;
    return (dist < 0) ? dist + circularBufferSize : dist;
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    openButton.setBounds(10, 10, getWidth() - 20, 30);
    playButton.setBounds(10, 50, getWidth() - 20, 30);
    stopButton.setBounds(10, 90, getWidth() - 20, 30);
    freezeButton.setBounds(10, 130, getWidth() - 20, 30);
}
