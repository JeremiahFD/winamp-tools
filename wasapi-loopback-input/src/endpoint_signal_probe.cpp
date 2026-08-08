#include <algorithm>
#include <chrono>
#include <cwchar>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

std::wstring DeviceId(IMMDeviceEnumerator* enumerator, ERole role) {
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, role, &device))) {
        return {};
    }

    wchar_t* id = nullptr;
    if (FAILED(device->GetId(&id))) {
        return {};
    }
    const std::wstring result = id;
    CoTaskMemFree(id);
    return result;
}

std::wstring DeviceName(IMMDevice* device) {
    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &properties))) {
        return L"Unknown playback device";
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT result = properties->GetValue(PKEY_Device_FriendlyName, &value);
    std::wstring name = L"Unknown playback device";
    if (SUCCEEDED(result) && value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        name = value.pwszVal;
    }
    PropVariantClear(&value);
    return name;
}

std::wstring RoleLabels(const std::wstring& id,
                        const std::wstring& console,
                        const std::wstring& multimedia,
                        const std::wstring& communications) {
    std::wstring labels;
    if (id == console) {
        labels += L"Console ";
    }
    if (id == multimedia) {
        labels += L"Multimedia ";
    }
    if (id == communications) {
        labels += L"Communications";
    }
    while (!labels.empty() && labels.back() == L' ') {
        labels.pop_back();
    }
    return labels.empty() ? L"none" : labels;
}

struct EndpointRow {
    std::wstring name;
    std::wstring roles;
    ComPtr<IAudioMeterInformation> meter;
    float peak = 0.0F;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
    int seconds = 5;
    if (argc > 1) {
        seconds = std::clamp(_wtoi(argv[1]), 1, 60);
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult)) {
        std::wcerr << L"Unable to initialize COM. HRESULT=0x" << std::hex
                   << static_cast<unsigned long>(comResult) << L"\n";
        return 1;
    }

    int exitCode = 0;
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                          IID_PPV_ARGS(&enumerator));
        if (FAILED(result)) {
            std::wcerr << L"Unable to create the audio endpoint enumerator. HRESULT=0x"
                       << std::hex << static_cast<unsigned long>(result) << L"\n";
            exitCode = 2;
        } else {
            const std::wstring console = DeviceId(enumerator.Get(), eConsole);
            const std::wstring multimedia = DeviceId(enumerator.Get(), eMultimedia);
            const std::wstring communications = DeviceId(enumerator.Get(), eCommunications);

            ComPtr<IMMDeviceCollection> devices;
            result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
            if (FAILED(result)) {
                std::wcerr << L"Unable to enumerate active playback endpoints. HRESULT=0x"
                           << std::hex << static_cast<unsigned long>(result) << L"\n";
                exitCode = 3;
            } else {
                UINT count = 0;
                devices->GetCount(&count);
                std::vector<EndpointRow> rows;
                rows.reserve(count);

                for (UINT index = 0; index < count; ++index) {
                    ComPtr<IMMDevice> device;
                    if (FAILED(devices->Item(index, &device))) {
                        continue;
                    }

                    wchar_t* rawId = nullptr;
                    if (FAILED(device->GetId(&rawId))) {
                        continue;
                    }
                    const std::wstring id = rawId;
                    CoTaskMemFree(rawId);

                    EndpointRow row;
                    row.name = DeviceName(device.Get());
                    row.roles = RoleLabels(id, console, multimedia, communications);
                    result = device->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL,
                                              nullptr,
                                              reinterpret_cast<void**>(row.meter.GetAddressOf()));
                    if (SUCCEEDED(result)) {
                        rows.push_back(std::move(row));
                    }
                }

                std::wcout << L"Watching " << rows.size() << L" active playback endpoint(s) for "
                           << seconds << L" second(s)...\n";
                const int iterations = seconds * 20;
                for (int iteration = 0; iteration < iterations; ++iteration) {
                    for (auto& row : rows) {
                        float peak = 0.0F;
                        if (SUCCEEDED(row.meter->GetPeakValue(&peak))) {
                            row.peak = std::max(row.peak, peak);
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                std::wcout << std::fixed << std::setprecision(6);
                for (const auto& row : rows) {
                    std::wcout << L"Device: " << row.name << L"\n"
                               << L"Default roles: " << row.roles << L"\n"
                               << L"Maximum live peak: " << row.peak << L"\n"
                               << L"Signal: " << (row.peak >= 0.001F ? L"YES" : L"NO / SILENT")
                               << L"\n\n";
                }
            }
        }
    }

    CoUninitialize();
    return exitCode;
}
