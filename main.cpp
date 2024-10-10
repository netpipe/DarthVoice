#include <QApplication>
#include <QWidget>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QIODevice>
#include <QPushButton>
#include <QVBoxLayout>
#include <QByteArray>
#include <QBuffer>
#include <QTimer>
#include <cmath>
#include <vector>
#include <QDebug>

// Constants for pitch shifting
const double PI = 3.14159265358979323846;
const int SAMPLE_RATE = 44100; // 44.1 kHz
const int CHANNELS = 1;        // Mono
const int SAMPLE_SIZE = 16;    // 16 bits per sample

// Simple Low-Pass Filter Implementation
class LowPassFilter {
public:
    LowPassFilter(double cutoffFrequency, double sampleRate) {
        double RC = 1.0 / (2 * PI * cutoffFrequency);
        alpha = 1.0 / (RC * sampleRate + 1.0);
        prev = 0.0;
    }

    double process(double input) {
        double output = prev + (alpha * (input - prev));
        prev = output;
        return output;
    }

private:
    double alpha;
    double prev;
};

// Simple Pitch Shifting by Resampling (Not high quality)
class PitchShifter {
public:
    PitchShifter(double pitchFactor) : factor(pitchFactor), phase(0.0) {}

    // Simple resampling without windowing
    double process(double input) {
        buffer.push_back(input);
        if (buffer.size() > static_cast<size_t>(1.0 / factor * SAMPLE_RATE)) {
            double output = buffer.front();
            buffer.erase(buffer.begin());
            return output;
        }
        return 0.0; // Silence until buffer is filled
    }

private:
    double factor; // Pitch factor (>1 higher pitch, <1 lower pitch)
    double phase;
    std::vector<double> buffer;
};

// Custom QIODevice for audio processing
class AudioProcessor : public QIODevice {
    Q_OBJECT
public:
    AudioProcessor(QAudioFormat format, QObject* parent = nullptr)
        : QIODevice(parent), format(format), filter(300.0, SAMPLE_RATE),
          shifter(0.8) // Lower pitch by factor of 0.8
    {
        open(QIODevice::ReadWrite);
    }

    // Start the device
    void startProcessing() {
        open(QIODevice::ReadWrite);
    }

    // Stop the device
    void stopProcessing() {
        close();
    }

    // Implement readData to provide processed audio to QAudioOutput
    qint64 readData(char* data, qint64 maxlen) override {
        if (outputBuffer.isEmpty())
            return 0;

        qint64 bytesToRead = qMin(maxlen, static_cast<qint64>(outputBuffer.size()));
        memcpy(data, outputBuffer.constData(), bytesToRead);
        outputBuffer.remove(0, bytesToRead);
        return bytesToRead;
    }

    // Implement writeData to receive audio from QAudioInput
    qint64 writeData(const char* data, qint64 len) override {
        const qint16* samples = reinterpret_cast<const qint16*>(data);
        int sampleCount = len / 2; // 16-bit audio

        QByteArray processedData;
        processedData.reserve(len);

        for (int i = 0; i < sampleCount; ++i) {
            // Convert to double
            double sample = samples[i] / 32768.0;

            // Apply pitch shifting
            double pitchedSample = shifter.process(sample);

            // Apply low-pass filter
            double filteredSample = filter.process(pitchedSample);

            // Convert back to 16-bit
            qint16 outputSample = static_cast<qint16>(filteredSample * 32767.0);
            processedData.append(reinterpret_cast<char*>(&outputSample), 2);
        }

        // Append processed data to output buffer
        outputBuffer.append(processedData);
        return len;
    }

protected:
    qint64 bytesAvailable() const override {
        return QIODevice::bytesAvailable() + outputBuffer.size();
    }

private:
    QAudioFormat format;
    LowPassFilter filter;
    PitchShifter shifter;
    QByteArray outputBuffer;
};

// Main Application Window
class VoiceChanger : public QWidget {
    Q_OBJECT
public:
    VoiceChanger(QWidget* parent = nullptr) : QWidget(parent) {
        // Set up UI
        QVBoxLayout* layout = new QVBoxLayout(this);
        QPushButton* startButton = new QPushButton("Start Voice Changer", this);
        QPushButton* stopButton = new QPushButton("Stop Voice Changer", this);
        layout->addWidget(startButton);
        layout->addWidget(stopButton);
        setLayout(layout);

        // Setup Audio Format
        QAudioFormat format;
        format.setSampleRate(SAMPLE_RATE);
        format.setChannelCount(CHANNELS);
        format.setSampleSize(SAMPLE_SIZE);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        // Check if format is supported
        QAudioDeviceInfo inputInfo = QAudioDeviceInfo::defaultInputDevice();
        if (!inputInfo.isFormatSupported(format)) {
            qWarning() << "Default format not supported, trying to use the nearest.";
            format = inputInfo.nearestFormat(format);
        }

        QAudioDeviceInfo outputInfo = QAudioDeviceInfo::defaultOutputDevice();
        if (!outputInfo.isFormatSupported(format)) {
            qWarning() << "Default format not supported, trying to use the nearest.";
            format = outputInfo.nearestFormat(format);
        }

        // Initialize Audio Processor
        processor = new AudioProcessor(format, this);

        // Initialize Audio Input
        audioInput = new QAudioInput(inputInfo, format, this);
        audioInput->setBufferSize(4096);

        // Initialize Audio Output
        audioOutput = new QAudioOutput(outputInfo, format, this);
        audioOutput->setBufferSize(4096);

        // Connect Buttons
        connect(startButton, &QPushButton::clicked, this, &VoiceChanger::startProcessing);
        connect(stopButton, &QPushButton::clicked, this, &VoiceChanger::stopProcessing);
    }

    ~VoiceChanger() {
        stopProcessing();
    }

private slots:
    void startProcessing() {
        if (!processor->isOpen()) {
            processor->startProcessing();
        }

        audioInput->start(processor);
        audioOutput->start(processor);
        qDebug() << "Voice Changer Started.";
    }

    void stopProcessing() {
        audioInput->stop();
        audioOutput->stop();
        processor->stopProcessing();
        qDebug() << "Voice Changer Stopped.";
    }

private:
    QAudioInput* audioInput;
    QAudioOutput* audioOutput;
    AudioProcessor* processor;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    VoiceChanger window;
    window.setWindowTitle("Darth Vader Voice Changer");
    window.resize(300, 100);
    window.show();

    return app.exec();
}

#include "main.moc"
