#ifndef COREAUDIOQT_H
#define COREAUDIOQT_H

#include <AudioHardware.h>

#include <QString>
#include <QObject>

#include <vector>
#include <exception>
#include <memory>
#include <functional>


/**
 * @brief Read a vector of constant-length property elements
 * @param objectID
 * @param addr
 * @param data
 */
template<typename T>
inline void getCAProperties(AudioObjectID objectID, const AudioObjectPropertyAddress& addr, std::vector<T> &data)
{
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(objectID, &addr, 0, nullptr, &dataSize) != 0)
    {
        throw std::runtime_error("AudioObjectGetPropertyDataSize() failed");
    }
    size_t numElements = dataSize / sizeof(T);
    if ((numElements * sizeof(T)) != dataSize)
    {
        throw std::runtime_error("Invalid number of elements returned from AudioObjectGetPropertyDataSize()");
    }
    if (numElements == 0)
    {
        data.clear();
        return;
    }

    data.resize(numElements);
    if (AudioObjectGetPropertyData(objectID, &addr, 0,nullptr, &dataSize, data.data()) != 0)
    {
        throw std::runtime_error("AudioObjectGetPropertyData() failed");
    }
    if ((numElements * sizeof(T)) != dataSize)
    {
        throw std::runtime_error("Invalid number of elements returned from AudioObjectGetPropertyData()");
    }
}

/**
 * @brief Read a variable-length property element
 * @param objectID
 * @param addr
 * @return
 */
template<typename T>
inline std::unique_ptr<T, void (*)(void*)> getCAProperty(AudioObjectID objectID,
                                                         const AudioObjectPropertyAddress& addr,
                                                         std::function<void(T&)> fillFunc = std::function<void(T&)>())
{
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(objectID, &addr, 0, nullptr, &dataSize) != 0)
    {
        throw std::runtime_error("AudioObjectGetPropertyDataSize() failed");
    }
    if (dataSize == 0)
    {
        throw std::runtime_error("AudioObjectGetPropertyDataSize() returns zero size");
    }

    auto rawPtr = reinterpret_cast<T*>(::malloc(dataSize));
    if (rawPtr == NULL)
    {
        throw std::bad_alloc();
    }
    std::unique_ptr<T, void (*)(void*)> ptr(rawPtr, ::free);

    if (fillFunc)
    {
        fillFunc(*ptr);
    }

    if (AudioObjectGetPropertyData(objectID, &addr, 0, nullptr, &dataSize, ptr.get()) != 0)
    {
        throw std::runtime_error("AudioObjectGetPropertyData() failed");
    }

    return ptr;
}

/**
 * @brief Read a constant-length property element
 * @param objectID
 * @param addr
 * @param data
 */
template<typename T>
inline void getCAProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& addr, T &data)
{
    auto dataSize = static_cast<UInt32>(sizeof(T));
    if (AudioObjectGetPropertyData(objectID, &addr, 0,nullptr, &dataSize, &data) != 0)
    {
        throw std::runtime_error("AudioObjectGetPropertyData() failed");
    }
    if (dataSize != static_cast<UInt32>(sizeof(T)))
    {
        throw std::runtime_error("Invalid number of elements returned from AudioObjectGetPropertyData()");
    }
}

/**
 * @brief Read a property element in string type
 * @param objectID
 * @param addr
 * @param data
 */
template<>
inline void getCAProperty<QString>(AudioObjectID objectID, const AudioObjectPropertyAddress& addr, QString &data)
{
    CFStringRef str = NULL;
    getCAProperty(objectID, addr, str);
    data = QString::fromCFString(str);
    if (str != NULL)
    {
        CFRelease(str);
    }
}

void setCAProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& addr, const void *data, UInt32 dataSize = 0);

template<typename T>
inline void setCAProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& addr, const T& data)
{
    setCAProperty(objectID, addr, &data, sizeof(data));
}

std::vector<UInt32> getIOProcStreamUsage(AudioObjectID deviceID, AudioObjectPropertyScope scope, AudioDeviceIOProcID procID);

void setIOProcStreamUsage(AudioObjectID deviceID, AudioObjectPropertyScope scope, AudioDeviceIOProcID procID, const std::vector<UInt32>& usages);


struct DeviceInfo
{
    AudioObjectID id{0};
    QString name;
    QString manufacturer;
    QString deviceUID;
    UInt32 alive{0};
    UInt32 running{0};
    UInt32 numInputs{0};
    UInt32 numOutputs{0};
    Float64 sampleRate{0};
    std::vector<AudioValueRange> availSampleRates;
};

std::vector<DeviceInfo> getDevices();

AudioObjectID getDefaultInputDeviceID();

AudioObjectID getDefaultOutputDeviceID();


class CoreAudioQt : public QObject
{
    Q_OBJECT
public:
    CoreAudioQt(AudioObjectID deviceID, QObject *parent = nullptr);
    virtual ~CoreAudioQt();
    void Start();
    void Stop();
    virtual void process(size_t numSamples, const float *inSamples, size_t inChannels, float *outSamples, size_t outChannels) = 0;

signals:
    void error(const QString &msg);

private:
    const AudioObjectID deviceID_;
    AudioDeviceIOProcID procID_{nullptr};
};

#endif // COREAUDIOQT_H
