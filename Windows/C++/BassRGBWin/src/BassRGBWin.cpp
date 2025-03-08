#include <iostream>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <vector>
#include "kiss_fft.h"

#pragma comment(lib, "Avrt.lib")


constexpr auto FFT_SIZE = 1024;
constexpr auto BASS_HIGH = 250;
constexpr auto BASS_LOW = 20;


static void FilterBassFrequencies(kiss_fft_cpx* fftOutput, int sampleRate, int fftSize) {
    int numBins = fftSize / 2;
    for (int i = 0; i < numBins; ++i) {
        double frequency = (i * sampleRate) / (double)fftSize;
        if (frequency < BASS_LOW || frequency > BASS_HIGH) {
            fftOutput[i].r = 0.0;
            fftOutput[i].i = 0.0;
        }
    }
}

int main() {
    HRESULT hResult;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;

    CoInitialize(nullptr);

    hResult = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hResult)) {
        std::cerr << "Failed to create device enumerator!" << std::endl;
        return -1;
    }

    hResult = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hResult)) {
        std::cerr << "Failed to get default audio device!" << std::endl;
        return -1;
    }

    hResult = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hResult)) {
        std::cerr << "Failed to activate audio client!" << std::endl;
        return -1;
    }

    WAVEFORMATEX* pWaveFormat;
    hResult = pAudioClient->GetMixFormat(&pWaveFormat);
    if (FAILED(hResult)) {
        std::cerr << "Failed to get mix format!" << std::endl;
        return -1;
    }

    hResult = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, pWaveFormat, NULL);
    if (FAILED(hResult)) {
        std::cerr << "Failed to initialize audio client!" << std::endl;
        return -1;
    }

    hResult = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hResult)) {
        std::cerr << "Failed to get capture client!" << std::endl;
        return -1;
    }

    hResult = pAudioClient->Start();
    if (FAILED(hResult)) {
        std::cerr << "Failed to start audio capture!" << std::endl;
        return -1;
    }

    std::cout << "Capturing bass... Press Ctrl+C to stop." << std::endl;

    kiss_fft_cfg fftCfg = kiss_fft_alloc(FFT_SIZE, 0, nullptr, nullptr);
    std::vector<kiss_fft_cpx> fftOutput(FFT_SIZE);
    std::vector<kiss_fft_cpx> fftInput(FFT_SIZE);

    while (true) {
        UINT32 packetLength = 0;
        hResult = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hResult)) {
            std::cerr << "Failed to get packet size!" << std::endl;
            break;
        }
        // std::cout << "first packet length: " << packetLength << std::endl;

        while (packetLength > 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;

            hResult = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hResult)) {
                std::cerr << "Failed to get buffer!" << std::endl;
                break;
            }

            short* buffer = (short*)pData;
            UINT32 limit = (numFramesAvailable < FFT_SIZE) ? numFramesAvailable : FFT_SIZE;

            for (UINT32 i = 0; i < limit; i++) {
                fftInput[i].r = static_cast<float>(buffer[i]) / 32768.0f;
                fftInput[i].i = 0.0f;
            }

            kiss_fft(fftCfg, fftInput.data(), fftOutput.data());
            FilterBassFrequencies(fftOutput.data(), pWaveFormat->nSamplesPerSec, FFT_SIZE);

            for (int i = 0; i < FFT_SIZE / 2; ++i) {
                double frequency = (i * pWaveFormat->nSamplesPerSec) / (double)FFT_SIZE;
                double magnitude = sqrt(fftOutput[i].r * fftOutput[i].r + fftOutput[i].i * fftOutput[i].i);
                if (frequency <= BASS_HIGH && frequency >= BASS_LOW) {
                    std::cout << "Bin: " << i << " | Frequency: " << frequency << " Hz | Magnitude: " << magnitude << std::endl;
                }
            }

            hResult = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hResult)) {
                std::cerr << "Failed to release buffer!" << std::endl;
                break;
            }

            hResult = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hResult)) {
                std::cerr << "Failed to get next packet size!" << std::endl;
                break;
            }

            // std::cout << "next packet length: " << packetLength << std::endl;
        }

        Sleep(100);
    }

    kiss_fft_free(fftCfg);
    pAudioClient->Stop();
    pCaptureClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}
