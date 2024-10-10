#include <QApplication>
#include <QWidget>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QIODevice>
#include <QPushButton>
#include <QVBoxLayout>
#include <QByteArray>
#include <QDebug>

#include <SoundTouch.h>
using namespace soundtouch;

// Custom QIODevice for audio processing with SoundTouch
class AudioProcessor : public QIODevice {
    Q_OBJECT
public:
    AudioProcessor(QAudioFormat format, QObject* parent = nullptr)
        : QIODevice(parent), format(format), soundTouch()
    {
        // Configure SoundTouch
        soundTouch.setSampleRate(format.sampleRate());
        soundTouch.setChannels(format.channelCount());
        soundTouch.setPitch(0.8f); // Lower pitch by factor
        soundTouch.setTempo(1.0f); // Normal tempo

        open(QIODevice::ReadWrite);
    }

    // Start the device
    void startProcessing() {
        open(QIODevice::ReadWrite);
    }

    // Stop the device
    void stopProcessing() {
        soundTouch.flush();
        close();
    }

    // Implement readData to provide processed audio to QAudioOutput
    qint64 readData(char* data, qint64 maxlen) override {
        int16_t buffer[4096];
        int numSamples = soundTouch.receiveSamples(buffer, maxlen / (2 * format.channelCount()));

        if (numSamples > 0) {
            QByteArray processedData(reinterpret_cast<char*>(buffer), numSamples * 2 * format.channelCount());
            qint64 bytesToRead = qMin(maxlen, static_cast<qint64>(processedData.size()));
            memcpy(data, processedData.constData(), bytesToRead);
            return bytesToRead;
        }

        return 0;
    }

    // Implement writeData to receive audio from QAudioInput
    qint64 writeData(const char* data, qint64 len) override {
        soundTouch.putSamples(reinterpret_cast<const SAMPLETYPE*>(data), len / (2 * format.channelCount()));
        return len;
    }

protected:
    qint64 bytesAvailable() const override {
        return QIODevice::bytesAvailable() + soundTouch.numSamples();
    }

private:
    QAudioFormat format;
    SoundTouch soundTouch;
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
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleSize(16);
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

        qDebug() << "Input Device:" << inputInfo.deviceName();
        qDebug() << "Output Device:" << outputInfo.deviceName();

        // Initialize Audio Processor
        processor = new AudioProcessor(format, this);
        qDebug() << "AudioProcessor initialized.";

        // Initialize Audio Input
        audioInput = new QAudioInput(inputInfo, format, this);
        audioInput->setBufferSize(4096);
        connect(audioInput, &QAudioInput::stateChanged, this, [](QAudio::State newState){
            qDebug() << "AudioInput State Changed:" << newState;
        });

        // Initialize Audio Output
        audioOutput = new QAudioOutput(outputInfo, format, this);
        audioOutput->setBufferSize(4096);
        connect(audioOutput, &QAudioOutput::stateChanged, this, [](QAudio::State newState){
            qDebug() << "AudioOutput State Changed:" << newState;
        });

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
            qDebug() << "AudioProcessor started.";
        }

        audioInput->start(processor);
        audioOutput->start(processor);
        qDebug() << "Audio Input and Output started.";
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
