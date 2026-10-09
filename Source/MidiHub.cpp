#include "MidiHub.h"

namespace
{
    juce::String hexOf (const juce::MidiMessage& m, int maxBytes = 16)
    {
        const auto* d = m.getRawData();
        const int n = m.getRawDataSize();
        auto s = juce::String::toHexString (d, juce::jmin (n, maxBytes), 1);
        if (n > maxBytes)
            s << " ...";
        return s + "  (" + juce::String (n) + " bytes)";
    }
}

juce::File MidiHub::logFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("MC909-Editor-midi-log.txt");
}

void MidiHub::logLine (const juce::String& line)
{
    const juce::ScopedLock sl (logLock);
    if (logLines >= 2500)
        return;

    ++logLines;
    logFile().appendText (juce::Time::getCurrentTime().toString (false, true, true, true) + "  " + line + "\n");
}

MidiHub::MidiHub()
{
    logFile().getParentDirectory().createDirectory();
    logFile().replaceWithText ("MC-909 Editor MIDI log\n");
    startTimer (5);
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

void MidiHub::send (const juce::MidiMessage& m, int gapMs)
{
    const juce::ScopedLock sl (queueLock);
    queue.push_back ({ m, gapMs < 0 ? sendIntervalMs : gapMs });
}

void MidiHub::sendAll (const std::vector<juce::MidiMessage>& ms)
{
    const juce::ScopedLock sl (queueLock);
    for (const auto& m : ms)
        queue.push_back ({ m, sendIntervalMs });
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

    const auto now = juce::Time::getMillisecondCounter();
    if ((juce::int32) (now - nextSendAt) < 0)
        return;

    Item next;

    {
        const juce::ScopedLock sl (queueLock);
        if (queue.empty())
            return;

        next = queue.front();
        queue.pop_front();
    }

    output->sendMessageNow (next.msg);
    nextSendAt = now + (juce::uint32) next.gapMs;
    ++txSent;

    if (next.msg.isSysEx())
        logLine ("TX " + hexOf (next.msg));
}

void MidiHub::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    ++rxAll;

    if (! m.isSysEx())
        return;

    ++rxSysEx;
    logLine ("RX " + hexOf (m));

    // Copy onto the message thread: listeners touch UI state.
    const juce::MidiMessage copy (m);
    juce::MessageManager::callAsync ([this, copy]
    {
        listeners.call ([&copy] (Listener& l) { l.sysExReceived (copy); });
    });
}
