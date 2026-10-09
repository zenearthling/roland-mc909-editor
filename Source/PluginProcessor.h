#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MC909Map.h"
#include "MidiHub.h"

#include <map>

class MC909EditorProcessor : public juce::AudioProcessor,
                             private MidiHub::Listener,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    MC909EditorProcessor();
    ~MC909EditorProcessor() override;

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MC-909 Editor"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    // Editing model
    //==============================================================================

    /** Change one parameter and push a DT1 to the hardware. */
    void setParameterValue (const mc909::ParamDef& p, int rawValue, bool sendToDevice = true);

    /** Current raw value from the local mirror of the device state. */
    int getParameterValue (const mc909::ParamDef& p) const;

    /** Patch name lives at Patch Common 0x00..0x0B as 12 ASCII bytes. */
    juce::String getPatchName() const;
    void setPatchName (const juce::String& newName);

    /** Ask the device for one block, or for everything the editor shows. */
    void requestBlock (mc909::Block b, int gapMs = -1);
    void requestAll();

    /** Push the whole local edit buffer back to the device. */
    void sendAll();

    void setSelectedPart (int part);
    void setSelectedTone (int tone);
    int  getSelectedPart() const { return selectedPart; }
    int  getSelectedTone() const { return selectedTone; }

    /** Bitmask of tones that automation and Quick SysEx apply to. */
    void setToneMask (int mask) { toneMask = mask & 0x0F; }
    int  getToneMask() const    { return toneMask; }

    void setDeviceId (int id) { deviceId = (uint8_t) juce::jlimit (0x10, 0x7F, id); }
    int  getDeviceId() const  { return deviceId; }

    /** Audition a note on the device without leaving the editor. */
    void playTestNote (int noteNumber, int velocity, bool on);

    /** Probe every MIDI input for an MC-909 identity reply. */
    void detectDevice();

    MidiHub& midi() { return hub; }

    /** Diagnostics for the header: replies stored / replies we could not place / unreadable. */
    int rxStored() const    { return dt1Stored; }
    int rxUnplaced() const  { return dt1Unplaced; }
    int rxUnreadable() const { return sysexUnreadable; }
    juce::AudioProcessorValueTreeState& apvts() { return valueTree; }

    struct EditListener
    {
        virtual ~EditListener() = default;
        virtual void modelChanged() = 0;
        virtual void deviceDetected (const juce::String& inputId) {}
    };

    void addEditListener (EditListener* l)    { editListeners.add (l); }
    void removeEditListener (EditListener* l) { editListeners.remove (l); }

private:
    //==============================================================================
    void sysExReceived (const juce::MidiMessage&) override;
    void parameterChanged (const juce::String& paramId, float newValue) override;

    juce::String blockKey (mc909::Block b) const;
    std::vector<uint8_t>& blockData (mc909::Block b);
    const std::vector<uint8_t>* findBlock (mc909::Block b) const;

    static int readValue (const std::vector<uint8_t>& data, uint32_t offset, int numBytes);

    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

    MidiHub hub;
    int dt1Stored = 0, dt1Unplaced = 0, sysexUnreadable = 0;
    juce::AudioProcessorValueTreeState valueTree;

    std::map<juce::String, std::vector<uint8_t>> blocks;

    int     selectedPart = 0;
    int     selectedTone = 0;
    int     toneMask     = mc909::quick::kAllTones;
    uint8_t deviceId     = 0x10;

    juce::ListenerList<EditListener> editListeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC909EditorProcessor)
};
