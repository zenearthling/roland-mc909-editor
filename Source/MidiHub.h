#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <deque>
#include <atomic>

/**
    Opens its own CoreMIDI ports rather than relying on the host.

    This is deliberate. Most DAWs filter or reorder SysEx on a plugin's MIDI
    output, and several drop it entirely, so routing hardware edits through the
    host is unreliable. Talking to CoreMIDI directly means the editor behaves
    identically in Logic, Live, Bitwig, Reaper and standalone.

    Bulk dumps are paced: the MC-909 expects roughly 20 ms between packets when
    receiving data larger than 256 bytes, and it will drop bytes if you flood it.
*/
class MidiHub : private juce::Timer,
                private juce::MidiInputCallback
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void sysExReceived (const juce::MidiMessage&) = 0;
        virtual void connectionChanged() {}
    };

    MidiHub();
    ~MidiHub() override;

    juce::Array<juce::MidiDeviceInfo> availableOutputs() const { return juce::MidiOutput::getAvailableDevices(); }
    juce::Array<juce::MidiDeviceInfo> availableInputs()  const { return juce::MidiInput::getAvailableDevices(); }

    bool openOutput (const juce::String& identifier);
    bool openInput  (const juce::String& identifier);
    void closeAll();

    juce::String currentOutputId() const { return outputId; }
    juce::String currentInputId()  const { return inputId; }
    bool isConnected() const { return output != nullptr; }
    bool isInputOpen() const { return input != nullptr; }

    /** Diagnostics: counters are updated from the MIDI threads. */
    int  rxMessageCount() const { return rxAll.load(); }
    int  rxSysExCount()   const { return rxSysEx.load(); }
    int  txMessageCount() const { return txSent.load(); }

    /** Queue a message. gapMs is the minimum wait before the NEXT message goes out
        (-1 = use the default interval). */
    void send (const juce::MidiMessage& m, int gapMs = -1);

    /** Queue several messages as one burst. */
    void sendAll (const std::vector<juce::MidiMessage>& ms);

    /** Drop anything still waiting (e.g. when the user cancels a dump). */
    void clearQueue();

    int  pendingCount() const;

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Interval between queued messages, in milliseconds. */
    void setSendInterval (int ms) { sendIntervalMs = juce::jmax (1, ms); }

    /** Append a line to the MIDI log file (Documents/MC909-Editor-midi-log.txt). */
    void logLine (const juce::String& line);
    static juce::File logFile();

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    std::unique_ptr<juce::MidiOutput> output;
    std::unique_ptr<juce::MidiInput>  input;
    juce::String outputId, inputId;

    mutable juce::CriticalSection queueLock;
    struct Item { juce::MidiMessage msg; int gapMs; };
    std::deque<Item> queue;
    int sendIntervalMs = 5;
    juce::uint32 nextSendAt = 0;

    juce::CriticalSection logLock;
    int logLines = 0;

    std::atomic<int> rxAll { 0 }, rxSysEx { 0 }, txSent { 0 };

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHub)
};
