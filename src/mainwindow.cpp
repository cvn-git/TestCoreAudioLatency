#include "mainwindow.h"
#include "coreaudioqt.h"

#include <nlohmann/json.hpp>

#include <QDebug>
#include <QMessageBox>

#include <sstream>
#include <fstream>
#include <mutex>
#include <array>
#include <filesystem>

#include "chirp.inc"


class LatencyTester : public CoreAudioQt
{
public:
    LatencyTester(AudioObjectID deviceId, double sampleRate, QObject *parent, const std::string& resultPath)
        : CoreAudioQt(deviceId, sampleRate, parent)
        , sampleRate_(sampleRate)
    {
        auto recordingFilename = std::filesystem::absolute(resultPath) / "recording.bin";
        file_.open(recordingFilename, std::ios::binary);
        if (!file_.is_open())
        {
            emit error("Cannot open recording file");
        }

        nlohmann::json json;
        json["sample_rate"] = sampleRate_;
        json["period"] = chirp_signal.size();
        std::ofstream jsonFile(std::filesystem::absolute(resultPath) / "config.json");
        jsonFile << json;

        qDebug() << "###############################################################";
        qDebug() << "Write recording to " << QString::fromStdString(recordingFilename);
        qDebug() << "Selected device ID: " << deviceId;
        qDebug() << "Selected device sample rate: " << sampleRate_;
    }

    void process(size_t numSamples, const float *inSamples, size_t inChannels, float *outSamples, size_t outChannels) override
    {
        std::scoped_lock<std::mutex> lock(mutex_);

        if ((inChannels != 2) || (outChannels != 2) || (inSamples == NULL) || (outSamples == NULL))
        {
            return;
        }

#if 0
        constexpr double twoPI = 6.283185307179586;
        const double step1 = twoPI * 440.0 / sampleRate_;
        const double step2 = twoPI * 1e3 / sampleRate_;
        float *ptr = outSamples;
        for (size_t k = 0; k < numSamples; k++)
        {
            *ptr++ = 1e-1 * std::sin(angle1_);
            *ptr++ = 1e-1 * std::sin(angle2_);
            angle1_ += step1;
            angle2_ += step2;
        }
        angle1_ = std::fmod(angle1_, twoPI);
        angle2_ = std::fmod(angle2_, twoPI);
#else
        float *dst = outSamples;
        const float *src = inSamples;
        for (size_t k = 0; k < numSamples; k++)
        {
            *dst++ = chirp_signal[pos_++];
            *dst++ = *src;
            src += 2;
            if (pos_ >= chirp_signal.size())
            {
                pos_ = 0;
            }
        }

#endif

        file_.write(reinterpret_cast<const char*>(inSamples), numSamples * inChannels * sizeof(float));
    }

private:
    std::ofstream file_;
    std::mutex mutex_;
    double sampleRate_{0};
    double angle1_{0};
    double angle2_{0};
    size_t pos_{0};
};


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{

    try
    {
        auto devices = getDevices();
        qDebug() << "Default input device ID:" << getDefaultInputDeviceID();
        qDebug() << "Default output device ID:" << getDefaultOutputDeviceID();
        qDebug() << devices.size() << " device(s) available:";
        const DeviceInfo *selectedDevice = nullptr;
        const auto selectedID = getDefaultInputDeviceID();
        for (const auto &dev : devices)
        {
            qDebug() << "-----------------------------------------------------";
            qDebug() << "ID:                " << dev.id;
            qDebug() << "Name:              " << dev.name;
            qDebug() << "Manufacturer:      " << dev.manufacturer;
            qDebug() << "Device UID:        " << dev.deviceUID;
            qDebug() << "Alive:             " << dev.alive;
            qDebug() << "Running:           " << dev.running;
            qDebug() << "Input channel(s):  " << dev.numInputs;
            qDebug() << "Output channel(s): " << dev.numOutputs;
            qDebug() << "Sample rate:       " << dev.sampleRate;

            std::ostringstream ss;
            for (auto range : dev.availSampleRates)
            {
                if (range.mMinimum != range.mMaximum)
                {
                    throw std::runtime_error("Invalid sample rate range");
                }
                ss << range.mMinimum << ",";
            }
            qDebug() << "Available sample rates:" << QString::fromStdString(ss.str());

            if (dev.id == selectedID)
            {
                selectedDevice = &dev;
            }
        }

        // Create tester
        tester = new LatencyTester(selectedDevice->id, 48e3, this, "../../../../TestCoreAudioLatency/python");
        connect(tester, &CoreAudioQt::error, this, &MainWindow::error);

        // Done
        tester->Start();
    }
    catch(const std::exception& e)
    {
        qDebug() << "EXCEPTION: " << e.what();
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::error(const QString& msg)
{
    QMessageBox::critical(this, "ERROR", msg);
    close();
}
