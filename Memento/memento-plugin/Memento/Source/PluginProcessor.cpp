#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

MementoAudioProcessor::MementoAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true))
{
}

void MementoAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    freePos = 0;
}

bool MementoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Sortie mono ou stéréo, pas d'entrée (instrument).
    const auto& out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void MementoAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    double bpm = engine.getHostBpm();
    bool playing = true;
    int64 hostPos = freePos;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())            bpm = *b;
            playing = pos->getIsPlaying();
            const double spb = 60.0 / jmax (1.0, bpm) * getSampleRate();
            if (auto ppq = pos->getPpqPosition())  hostPos = (int64) std::llround (*ppq * spb);
            else                                   hostPos = freePos;
        }
    }

    engine.setHostBpm (bpm);
    engine.process (buffer, hostPos, playing);

    if (playing) freePos += buffer.getNumSamples();
}

void MementoAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto s = engine.saveStateString();
    destData.replaceWith (s.toRawUTF8(), s.getNumBytesAsUTF8());
}

void MementoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto s = String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);
    engine.loadStateString (s);
}

AudioProcessorEditor* MementoAudioProcessor::createEditor()
{
    return new MementoAudioProcessorEditor (*this);
}

// Point d'entrée du plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MementoAudioProcessor();
}
