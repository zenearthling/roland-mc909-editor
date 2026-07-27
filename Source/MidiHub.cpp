#include "MidiHub.h"

MidiHub::MidiHub()
{
    startTimer (sendIntervalMs);
}

MidiHub::~MidiHub()
{
    stopTimer();
    closeAll();
}

bool MidiHub::openOutput (const juce::String& identifier)
{
    output.reset();
    outputId.clear();

    if (identifier.isEmpty())
    {
        listeners.call ([] (Listener& l) { l.connectionChanged(); });
        return false;
    }

    output = juce::MidiOutput::openDevice (identifier);
    if (output != nullptr)
        outputId = identifier;

    listeners.call ([] (Listener& l) { l.connectionChanged(); });
    return output != nullptr;
}

bool MidiHub::openInput (const juce::String& identifier)
{
    if (input != nullptr)
        input->stop();

    input.reset();
    inputId.clear();

    if (identifier.isEmpty())
    {
        listeners.call ([] (Listener& l) { l.connectionChanged(); });
        return false;
    }

    input = juce::MidiInput::openDevice (identifier, this);
    if (input != nullptr)
    {
        input->start();
        inputId = identifier;
    }

    listeners.call ([] (Listener& l) { l.connectionChanged(); });
    return input != nullptr;
}

void MidiHub::closeAll()
{
    clearQueue();

    if (input != nullptr)
        input->stop();

    input.reset();
    output.reset();
    inputId.clear();
    outputId.clear();
}

void MidiHub::send (const juce::MidiMessage& m)
{
    const juce::ScopedLock sl (queueLock);
    queue.push_back (m);
}

void MidiHub::sendAll (const std::vector<juce::MidiMessage>& ms)
{
    const juce::ScopedLock sl (queueLock);
    for (const auto& m : ms)
        queue.push_back (m);
}

void MidiHub::clearQueue()
{
    const juce::ScopedLock sl (queueLock);
    queue.clear();
}

int MidiHub::pendingCount() const
{
    const juce::ScopedLock sl (queueLock);
    return (int) queue.size();
}

void MidiHub::timerCallback()
{
    if (output == nullptr)
        return;

    juce::MidiMessage next;

    {
        const juce::ScopedLock sl (queueLock);
        if (queue.empty())
            return;

        next = queue.front();
        queue.pop_front();
    }

    output->sendMessageNow (next);
}

void MidiHub::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! m.isSysEx())
        return;

    // Copy onto the message thread: listeners touch UI state.
    const juce::MidiMessage copy (m);
    juce::MessageManager::callAsync ([this, copy]
    {
        listeners.call ([&copy] (Listener& l) { l.sysExReceived (copy); });
    });
}
