#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <deque>

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

    /** Queue a message. Bulk transfers are paced automatically. */
    void send (const juce::MidiMessage& m);

    /** Queue several messages as one burst. */
    void sendAll (const std::vector<juce::MidiMessage>& ms);

    /** Drop anything still waiting (e.g. when the user cancels a dump). */
    void clearQueue();

    int  pendingCount() const;

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Interval between queued messages, in milliseconds. */
    void setSendInterval (int ms) { sendIntervalMs = juce::jmax (1, ms); startTimer (sendIntervalMs); }

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    std::unique_ptr<juce::MidiOutput> output;
    std::unique_ptr<juce::MidiInput>  input;
    juce::String outputId, inputId;

    mutable juce::CriticalSection queueLock;
    std::deque<juce::MidiMessage> queue;
    int sendIntervalMs = 5;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHub)
};
