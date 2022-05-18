#include "coreaudioqt.h"

#include <QDebug>


void setCAProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& addr, const void *data, UInt32 dataSize)
{
    if (dataSize == 0)
    {
        if (AudioObjectGetPropertyDataSize(objectID, &addr, 0, nullptr, &dataSize) != 0)
        {
            throw std::runtime_error("AudioObjectGetPropertyDataSize() failed");
        }
    }

    if (AudioObjectSetPropertyData(objectID, &addr, 0, NULL, dataSize, data) != 0)
    {
        throw std::runtime_error("AudioObjectSetPropertyData() failed");
    }
}


std::vector<UInt32> getIOProcStreamUsage(AudioObjectID deviceID, AudioObjectPropertyScope scope, AudioDeviceIOProcID procID)
{
    const AudioObjectPropertyAddress addr{kAudioDevicePropertyIOProcStreamUsage, scope, kAudioObjectPropertyElementMain};
    auto usage = getCAProperty<AudioHardwareIOProcStreamUsage>(deviceID, addr, [procID](AudioHardwareIOProcStreamUsage& usage){
        usage.mIOProc = reinterpret_cast<void*>(procID);
    });
    std::vector<UInt32> result(usage->mNumberStreams);
    for (size_t k = 0; k < result.size(); k++)
    {
        result[k] = usage->mStreamIsOn[k];
    }
    return result;
}


void setIOProcStreamUsage(AudioObjectID deviceID, AudioObjectPropertyScope scope, AudioDeviceIOProcID procID, const std::vector<UInt32>& usages)
{
    const AudioObjectPropertyAddress addr{kAudioDevicePropertyIOProcStreamUsage, scope, kAudioObjectPropertyElementMain};
    auto usage = getCAProperty<AudioHardwareIOProcStreamUsage>(deviceID, addr, [procID](AudioHardwareIOProcStreamUsage& usage){
        usage.mIOProc = reinterpret_cast<void*>(procID);
    });

    if (usages.size() != usage->mNumberStreams)
    {
        throw std::invalid_argument("setIOProcStreamUsage(): Invalid number of streams");
    }
    for (size_t k = 0; k < usages.size(); k++)
    {
        usage->mStreamIsOn[k] = usages[k];
    }

    setCAProperty(deviceID, addr, usage.get());
}


std::vector<DeviceInfo> getDevices()
{
    std::vector<AudioObjectID> IDs;
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    getCAProperties(kAudioObjectSystemObject, addr, IDs);

    size_t numDevices = IDs.size();
    std::vector<DeviceInfo> devices(numDevices);
    for (size_t devIdx = 0; devIdx < numDevices; devIdx++)
    {
        auto &dev = devices[devIdx];
        auto id = IDs[devIdx];

        dev.id = id;

        addr.mScope = kAudioObjectPropertyScopeGlobal;

        addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
        getCAProperty(id, addr, dev.name);

        addr.mSelector = kAudioDevicePropertyDeviceManufacturerCFString;
        getCAProperty(id, addr, dev.manufacturer);

        addr.mSelector = kAudioDevicePropertyDeviceUID;
        getCAProperty(id, addr, dev.deviceUID);

        addr.mSelector = kAudioDevicePropertyDeviceIsAlive;
        getCAProperty(id, addr, dev.alive);

        addr.mSelector = kAudioDevicePropertyDeviceIsRunning;
        getCAProperty(id, addr, dev.running);

        addr.mSelector = kAudioClockDevicePropertyNominalSampleRate;
        getCAProperty(id, addr, dev.sampleRate);

        addr.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
        getCAProperties(id, addr, dev.availSampleRates);

        addr.mSelector = kAudioDevicePropertyStreamConfiguration;
        addr.mScope = kAudioObjectPropertyScopeOutput;
        auto list = getCAProperty<AudioBufferList>(id, addr);
        if (list->mNumberBuffers == 1)
        {
            dev.numOutputs = list->mBuffers[0].mNumberChannels;
        }

        addr.mScope = kAudioObjectPropertyScopeInput;
        list = getCAProperty<AudioBufferList>(id, addr);
        if (list->mNumberBuffers == 1)
        {
            dev.numInputs = list->mBuffers[0].mNumberChannels;
        }
    }

    return devices;
}

AudioObjectID getDefaultInputDeviceID()
{
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectID id;
    getCAProperty(kAudioObjectSystemObject, addr, id);
    return id;
}

AudioObjectID getDefaultOutputDeviceID()
{
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectID id;
    getCAProperty(kAudioObjectSystemObject, addr, id);
    return id;
}


namespace {

OSStatus audioIOProc(AudioObjectID inDevice,
                    const AudioTimeStamp* inNow,
                    const AudioBufferList* inInputData,
                    const AudioTimeStamp* inInputTime,
                    AudioBufferList* outOutputData,
                    const AudioTimeStamp* inOutputTime,
                    void* __nullable inClientData)
{
    if (inClientData == nullptr)
    {
        return noErr;
    }
    auto hdl = reinterpret_cast<CoreAudioQt*>(inClientData);

    const float *inSamples = nullptr;
    float *outSamples = nullptr;
    size_t numInputs = 0;
    size_t numOutputs = 0;
    size_t numSamples = 0;

    if (inInputData->mNumberBuffers == 1)
    {
        const auto &buffer = inInputData->mBuffers[0];
        numInputs = buffer.mNumberChannels;

        numSamples = buffer.mDataByteSize / (sizeof(float) * numInputs);
        if ((numSamples == 0) || ((numSamples * sizeof(float) * numInputs) != buffer.mDataByteSize))
        {
            emit hdl->error("Invalid number of input samples");
            return noErr;
        }

        inSamples = reinterpret_cast<const float*>(buffer.mData);
        if (inSamples == NULL)
        {
            emit hdl->error("Input streaming is disabled");
            return noErr;
        }
    }

    if (outOutputData->mNumberBuffers == 1)
    {
        auto &buffer = outOutputData->mBuffers[0];
        numOutputs = buffer.mNumberChannels;

        if (numSamples == 0)
        {
            numSamples = buffer.mDataByteSize / (sizeof(float) * numOutputs);
        }
        if ((numSamples == 0) || ((numSamples * sizeof(float) * numOutputs) != buffer.mDataByteSize))
        {
            emit hdl->error("Invalid number of output samples");
            return noErr;
        }

        outSamples = reinterpret_cast<float*>(buffer.mData);
        if (outSamples == NULL)
        {
            emit hdl->error("Output streaming is disabled");
            return noErr;
        }
    }

    hdl->process(numSamples, inSamples, numInputs, outSamples, numOutputs);

    return noErr;
}

void printStreamConfig(const AudioStreamBasicDescription& desc)
{
    qDebug() << "Sample rate: " << desc.mSampleRate;
    auto id = reinterpret_cast<const char*>(&desc.mFormatID);
    qDebug() << "Format ID: " << id[0] << id[1] << id[2] << id[3];
    qDebug() << "Format flags: " << desc.mFormatFlags;
    qDebug() << "Bytes per packet: " << desc.mBytesPerPacket;
    qDebug() << "Frames per packet: " << desc.mFramesPerPacket;
    qDebug() << "Bytes per frame: " << desc.mBytesPerFrame;
    qDebug() << "Channels per frame: " << desc.mChannelsPerFrame;
    qDebug() << "Bits per channel: " << desc.mBitsPerChannel;
}

}   // anonymous namespace

CoreAudioQt::CoreAudioQt(AudioObjectID deviceID, double sampleRate, QObject *parent)
    : QObject(parent)
    , deviceID_{deviceID}
    , sampleRate_{sampleRate}
{
}

CoreAudioQt::~CoreAudioQt()
{
    Stop();
}

void CoreAudioQt::Start()
{
    if (procID_ != nullptr)
    {
        emit error("Audio processing already started");
        return;
    }

    AudioObjectPropertyAddress addr{kAudioClockDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    setCAProperty(deviceID_, addr, sampleRate_);

    Float64 sampleRate = 0;
    getCAProperty(deviceID_, addr, sampleRate);
    if (std::abs(sampleRate - sampleRate_) > 1e-3)
    {
        qDebug() << "Sample rate: " << sampleRate;
        emit error("Cannot set the sample rate");
        return;
    }

#if 1
    addr.mSelector = kAudioDevicePropertyBufferFrameSize;
    UInt32 bufSize = 32;
    setCAProperty(deviceID_, addr, bufSize);

    bufSize = 0;
    addr.mScope = kAudioObjectPropertyScopeInput;
    getCAProperty(deviceID_, addr, bufSize);
    qDebug() << "Input buffer size: " << bufSize;

    bufSize = 0;
    addr.mScope = kAudioObjectPropertyScopeOutput;
    getCAProperty(deviceID_, addr, bufSize);
    qDebug() << "Output buffer size: " << bufSize;

    UInt32 latency = 0;
    addr.mSelector = kAudioDevicePropertyLatency;
    addr.mScope = kAudioObjectPropertyScopeInput;
    getCAProperty(deviceID_, addr, latency);
    qDebug() << "Input device latency: " << latency;

    latency = 0;
    addr.mScope = kAudioObjectPropertyScopeOutput;
    getCAProperty(deviceID_, addr, latency);
    qDebug() << "Output device latency: " << latency;

    addr.mSelector = kAudioDevicePropertyStreams;
    addr.mScope = kAudioObjectPropertyScopeInput;
    std::vector<UInt32> inStreamIDs;
    getCAProperties(deviceID_, addr, inStreamIDs);

    addr.mScope = kAudioObjectPropertyScopeOutput;
    std::vector<UInt32> outStreamIDs;
    getCAProperties(deviceID_, addr, outStreamIDs);

    if ((inStreamIDs.size() != 1) || (outStreamIDs.size() != 1))
        throw std::runtime_error("Invalid number of input/output streams");

    addr.mSelector = kAudioStreamPropertyLatency;
    addr.mScope = kAudioObjectPropertyScopeGlobal;
    UInt32 inLatency = 0;
    getCAProperty(inStreamIDs[0], addr, inLatency);
    qDebug() << "Input stream latency: " << inLatency;

    UInt32 outLatency = 0;
    getCAProperty(outStreamIDs[0], addr, outLatency);
    qDebug() << "Output stream latency: " << outLatency;
#endif

    if (AudioDeviceCreateIOProcID(deviceID_, audioIOProc, this, &procID_) != noErr)
    {
        emit error("AudioDeviceCreateIOProcID() failed");
        return;
    }

    if (AudioDeviceStart(deviceID_, procID_) != noErr)
    {
        emit error("AudioDeviceStart() failed");
        return;
    }
}

void CoreAudioQt::Stop()
{
    if (procID_ != nullptr)
    {
        AudioDeviceStop(deviceID_, nullptr);
        AudioDeviceDestroyIOProcID(deviceID_, procID_);
        procID_ = nullptr;
    }
}
