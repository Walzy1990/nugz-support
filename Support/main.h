#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <intrin.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#include <msi.h>
#pragma comment(lib, "msi.lib")
#include <netfw.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wscapi.lib")
#include <wscapi.h>
#include <tlhelp32.h>
#include <cstdlib>
#include <fstream>
#include <conio.h>
#include <shellapi.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "winhttp.lib")
#include <winhttp.h>
#include <sstream>
#include <filesystem>
#include <ranges>
#include <algorithm>
#include <cctype>
#include <utility>
#include <cstring>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

typedef LONG NTSTATUS;
typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
using namespace std;

int RunDashboard();

void SColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void RColor() {
    SColor(7);
}

void ClearScreen() {
    system("cls");
}

void FlushInputBuffer() {
    Sleep(80); // Let any queued keypresses arrive
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    while (_kbhit()) _getch(); // Catch any stragglers
}

void PrintHeader() {
    SColor(2);
    std::cout << R"(
  ____           _____ _____ _____       __      __
 |  _ \   /\    / ____|_   _/ ____|     /\ \    / /
 | |_) | /  \  | (___   | || |         /  \ \  / / 
 |  _ < / /\ \  \___ \  | || |        / /\ \ \/ /  
 | |_) / ____ \ ____) |_| || |____   / ____ \  /   
 |____/_/    \_\_____/|_____\_____| /_/    \_\/    
                                                   
                                                   

)";
    RColor();
}

bool SaveConsoleOutputToFile(const char* filename) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (filename == nullptr || !GetConsoleScreenBufferInfo(hConsole, &info)) return false;

    DWORD bufferSize = info.dwSize.X * info.dwSize.Y;
    CHAR_INFO* buffer = new CHAR_INFO[bufferSize];
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT readRegion = { 0, 0, info.dwSize.X - 1, info.dwSize.Y - 1 };

    if (!ReadConsoleOutput(hConsole, buffer, info.dwSize, bufferCoord, &readRegion)) {
        delete[] buffer;
        return false;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        delete[] buffer;
        return false;
    }

    for (int y = 0; y < info.dwSize.Y; ++y) {
        for (int x = 0; x < info.dwSize.X; ++x) {
            CHAR_INFO ci = buffer[y * info.dwSize.X + x];
            file << (char)ci.Char.AsciiChar;
        }
        file << '\n';
    }

    file.close();
    bool succeeded = file.good();
    delete[] buffer;
    return succeeded;
}

void DeleteFileIfExists(const std::string& filename) {
    // Versuche, die Datei zu löschen
    if (remove(filename.c_str()) == 0) {
       // std::cout << "Datei '" << filename << "' wurde gelöscht." << std::endl;
    }
    else {
        // remove() gibt 0 zurück, wenn die Datei gelöscht wurde, sonst -1
        // Es wird NICHT unterschieden, ob die Datei nicht existiert oder ein anderer Fehler vorliegt
        //std::cout << "Datei '" << filename << "' konnte nicht gelöscht werden (existiert nicht oder Fehler)." << std::endl;
    }
}

std::string NarrowWideString(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (requiredSize <= 1) {
        return "";
    }

    std::string output(requiredSize, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &output[0], requiredSize, nullptr, nullptr);
    if (!output.empty() && output.back() == '\0') {
        output.pop_back();
    }
    return output;
}

std::string TrimString(const std::string& input) {
    size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
        ++begin;
    }

    size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }

    return input.substr(begin, end - begin);
}

std::string ToLowerCopy(std::string input) {
    std::transform(input.begin(), input.end(), input.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
        });
    return input;
}

bool IsLikelyPlaceholderValue(const std::string& value) {
    std::string normalized = ToLowerCopy(TrimString(value));
    if (normalized.empty()) return true;

    static const std::vector<std::string> placeholders = {
        "to be filled by o.e.m.",
        "to be filled by oem",
        "default string",
        "default",
        "none",
        "n/a",
        "unknown",
        "null",
        "system serial number"
    };

    for (const auto& placeholder : placeholders) {
        if (normalized == placeholder) {
            return true;
        }
    }

    return false;
}

std::string VariantToString(VARIANT& variantValue) {
    VARIANT converted;
    VariantInit(&converted);

    if (variantValue.vt == VT_NULL || variantValue.vt == VT_EMPTY) {
        return "";
    }

    if (variantValue.vt == VT_BSTR && variantValue.bstrVal != nullptr) {
        return TrimString(NarrowWideString(std::wstring(variantValue.bstrVal)));
    }

    HRESULT hr = VariantChangeType(&converted, &variantValue, 0, VT_BSTR);
    if (FAILED(hr) || converted.vt != VT_BSTR || converted.bstrVal == nullptr) {
        VariantClear(&converted);
        return "";
    }

    std::string output = TrimString(NarrowWideString(std::wstring(converted.bstrVal)));
    VariantClear(&converted);
    return output;
}

std::string FormatMacAddress(const BYTE* address, ULONG length) {
    if (address == nullptr || length == 0) {
        return "";
    }

    std::ostringstream macBuilder;
    macBuilder << std::uppercase << std::hex;
    for (ULONG i = 0; i < length; ++i) {
        if (i != 0) {
            macBuilder << "-";
        }
        macBuilder.width(2);
        macBuilder.fill('0');
        macBuilder << static_cast<int>(address[i]);
    }

    return macBuilder.str();
}

std::string ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valueName) {
    HKEY keyHandle = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ | KEY_WOW64_64KEY, &keyHandle) != ERROR_SUCCESS) {
        return "";
    }

    DWORD valueType = 0;
    DWORD dataSize = 0;
    LONG result = RegQueryValueExW(keyHandle, valueName, nullptr, &valueType, nullptr, &dataSize);
    if (result != ERROR_SUCCESS || (valueType != REG_SZ && valueType != REG_EXPAND_SZ) || dataSize == 0) {
        RegCloseKey(keyHandle);
        return "";
    }

    std::wstring rawValue(dataSize / sizeof(wchar_t), L'\0');
    result = RegQueryValueExW(keyHandle, valueName, nullptr, &valueType, reinterpret_cast<LPBYTE>(&rawValue[0]), &dataSize);
    RegCloseKey(keyHandle);
    if (result != ERROR_SUCCESS) {
        return "";
    }

    if (!rawValue.empty() && rawValue.back() == L'\0') {
        rawValue.pop_back();
    }

    return TrimString(NarrowWideString(rawValue));
}

void PrintIdentifierBlock(const std::string& label, const std::vector<std::pair<std::string, std::string>>& entries) {
    std::cout << "[+] " << label << ":" << std::endl;

    bool hasOutput = false;
    for (const auto& entry : entries) {
        if (IsLikelyPlaceholderValue(entry.second)) {
            continue;
        }

        SColor(15);
        std::cout << "    - " << entry.first << ": " << entry.second << std::endl;
        RColor();
        hasOutput = true;
    }

    if (!hasOutput) {
        SColor(8);
        std::cout << "    - Not available" << std::endl;
        RColor();
    }
}

void WriteIdentifierBlockToFile(std::ofstream& outputFile, const std::string& label, const std::vector<std::pair<std::string, std::string>>& entries) {
    outputFile << "[+] " << label << ":" << std::endl;

    bool hasOutput = false;
    for (const auto& entry : entries) {
        if (IsLikelyPlaceholderValue(entry.second)) {
            continue;
        }

        outputFile << "    - " << entry.first << ": " << entry.second << std::endl;
        hasOutput = true;
    }

    if (!hasOutput) {
        outputFile << "    - Not available" << std::endl;
    }

    outputFile << std::endl;
}

std::vector<std::pair<std::string, std::string>> GatherWmiIdentifiers(
    const std::wstring& wmiNamespace,
    const std::wstring& className,
    const std::vector<std::wstring>& properties,
    const std::wstring& whereClause = L""
) {
    std::vector<std::pair<std::string, std::string>> identifiers;

    if (properties.empty()) {
        return identifiers;
    }

    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hres);
    if (FAILED(hres) && hres != RPC_E_CHANGED_MODE) {
        return identifiers;
    }

    IWbemLocator* locator = nullptr;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&locator);
    if (FAILED(hres)) {
        if (comInitializedHere) CoUninitialize();
        return identifiers;
    }

    IWbemServices* services = nullptr;
    hres = locator->ConnectServer(_bstr_t(wmiNamespace.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    if (FAILED(hres)) {
        locator->Release();
        if (comInitializedHere) CoUninitialize();
        return identifiers;
    }

    hres = CoSetProxyBlanket(
        services,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE
    );
    if (FAILED(hres)) {
        services->Release();
        locator->Release();
        if (comInitializedHere) CoUninitialize();
        return identifiers;
    }

    std::wstring selectClause;
    for (size_t i = 0; i < properties.size(); ++i) {
        selectClause += properties[i];
        if (i + 1 < properties.size()) {
            selectClause += L",";
        }
    }

    std::wstring query = L"SELECT " + selectClause + L" FROM " + className;
    if (!whereClause.empty()) {
        query += L" WHERE " + whereClause;
    }

    IEnumWbemClassObject* enumerator = nullptr;
    hres = services->ExecQuery(
        bstr_t("WQL"),
        bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &enumerator
    );

    if (SUCCEEDED(hres) && enumerator != nullptr) {
        IWbemClassObject* classObject = nullptr;
        ULONG returnedCount = 0;

        while (enumerator->Next(WBEM_INFINITE, 1, &classObject, &returnedCount) == S_OK && returnedCount > 0) {
            for (const auto& propertyName : properties) {
                VARIANT value;
                VariantInit(&value);

                HRESULT propertyRead = classObject->Get(propertyName.c_str(), 0, &value, nullptr, nullptr);
                if (SUCCEEDED(propertyRead)) {
                    std::string valueText = VariantToString(value);
                    if (!IsLikelyPlaceholderValue(valueText)) {
                        identifiers.emplace_back(NarrowWideString(propertyName), valueText);
                    }
                }

                VariantClear(&value);
            }

            classObject->Release();
        }

        enumerator->Release();
    }

    services->Release();
    locator->Release();
    if (comInitializedHere) CoUninitialize();

    std::sort(identifiers.begin(), identifiers.end());
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
    return identifiers;
}

std::vector<std::pair<std::string, std::string>> GatherIfTableIdentifiers() {
    std::vector<std::pair<std::string, std::string>> identifiers;

    ULONG tableSize = 0;
    if (GetIfTable(nullptr, &tableSize, FALSE) != ERROR_INSUFFICIENT_BUFFER || tableSize == 0) {
        return identifiers;
    }

    std::vector<BYTE> tableBuffer(tableSize);
    PMIB_IFTABLE ifTable = reinterpret_cast<PMIB_IFTABLE>(tableBuffer.data());
    if (GetIfTable(ifTable, &tableSize, FALSE) != NO_ERROR) {
        return identifiers;
    }

    for (DWORD i = 0; i < ifTable->dwNumEntries; ++i) {
        const MIB_IFROW& row = ifTable->table[i];
        std::string mac = FormatMacAddress(row.bPhysAddr, row.dwPhysAddrLen);
        if (!IsLikelyPlaceholderValue(mac)) {
            identifiers.emplace_back("MAC", mac);
        }
    }

    std::sort(identifiers.begin(), identifiers.end());
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
    return identifiers;
}

std::vector<std::pair<std::string, std::string>> GatherAdapterInfoIdentifiers() {
    std::vector<std::pair<std::string, std::string>> identifiers;

    ULONG bufferSize = 0;
    if (GetAdaptersInfo(nullptr, &bufferSize) != ERROR_BUFFER_OVERFLOW || bufferSize == 0) {
        return identifiers;
    }

    std::vector<BYTE> adapterBuffer(bufferSize);
    PIP_ADAPTER_INFO adapters = reinterpret_cast<PIP_ADAPTER_INFO>(adapterBuffer.data());
    if (GetAdaptersInfo(adapters, &bufferSize) != NO_ERROR) {
        return identifiers;
    }

    for (PIP_ADAPTER_INFO current = adapters; current != nullptr; current = current->Next) {
        std::string mac = FormatMacAddress(current->Address, current->AddressLength);
        if (!IsLikelyPlaceholderValue(mac)) {
            identifiers.emplace_back("MAC", mac);
        }
        std::string adapterName = TrimString(current->AdapterName);
        if (!IsLikelyPlaceholderValue(adapterName)) {
            identifiers.emplace_back("AdapterName", adapterName);
        }
    }

    std::sort(identifiers.begin(), identifiers.end());
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
    return identifiers;
}

std::vector<std::pair<std::string, std::string>> GatherRegistryNetworkIdentifiers() {
    std::vector<std::pair<std::string, std::string>> identifiers;
    const wchar_t* basePath = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}";

    HKEY baseKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, basePath, 0, KEY_READ | KEY_WOW64_64KEY, &baseKey) != ERROR_SUCCESS) {
        return identifiers;
    }

    DWORD index = 0;
    wchar_t subKeyName[256];
    DWORD subKeyNameLength = _countof(subKeyName);

    while (RegEnumKeyExW(baseKey, index, subKeyName, &subKeyNameLength, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        HKEY adapterKey = nullptr;
        if (RegOpenKeyExW(baseKey, subKeyName, 0, KEY_READ | KEY_WOW64_64KEY, &adapterKey) == ERROR_SUCCESS) {
            auto readLocalValue = [&](const wchar_t* valueName) -> std::string {
                DWORD valueType = 0;
                DWORD dataSize = 0;
                LONG query = RegQueryValueExW(adapterKey, valueName, nullptr, &valueType, nullptr, &dataSize);
                if (query != ERROR_SUCCESS || (valueType != REG_SZ && valueType != REG_EXPAND_SZ) || dataSize == 0) {
                    return "";
                }

                std::wstring value(dataSize / sizeof(wchar_t), L'\0');
                query = RegQueryValueExW(adapterKey, valueName, nullptr, &valueType, reinterpret_cast<LPBYTE>(&value[0]), &dataSize);
                if (query != ERROR_SUCCESS) {
                    return "";
                }

                if (!value.empty() && value.back() == L'\0') {
                    value.pop_back();
                }

                return TrimString(NarrowWideString(value));
            };

            std::string netCfgId = readLocalValue(L"NetCfgInstanceId");
            if (!IsLikelyPlaceholderValue(netCfgId)) {
                identifiers.emplace_back("NetCfgInstanceId", netCfgId);
            }

            std::string networkAddress = readLocalValue(L"NetworkAddress");
            if (!IsLikelyPlaceholderValue(networkAddress)) {
                identifiers.emplace_back("NetworkAddress", networkAddress);
            }

            std::string driverDesc = readLocalValue(L"DriverDesc");
            if (!IsLikelyPlaceholderValue(driverDesc)) {
                identifiers.emplace_back("DriverDesc", driverDesc);
            }

            RegCloseKey(adapterKey);
        }

        ++index;
        subKeyNameLength = _countof(subKeyName);
    }

    RegCloseKey(baseKey);

    std::sort(identifiers.begin(), identifiers.end());
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
    return identifiers;
}

std::vector<std::pair<std::string, std::string>> GatherVolumeIdentifiers() {
    std::vector<std::pair<std::string, std::string>> identifiers;

    wchar_t volumeName[MAX_PATH] = { 0 };
    HANDLE volumeHandle = FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName));
    if (volumeHandle == INVALID_HANDLE_VALUE) {
        return identifiers;
    }

    do {
        std::string volumeGuid = TrimString(NarrowWideString(std::wstring(volumeName)));
        if (!IsLikelyPlaceholderValue(volumeGuid)) {
            identifiers.emplace_back("VolumeGuid", volumeGuid);
        }
    } while (FindNextVolumeW(volumeHandle, volumeName, ARRAYSIZE(volumeName)));

    FindVolumeClose(volumeHandle);
    std::sort(identifiers.begin(), identifiers.end());
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
    return identifiers;
}

std::vector<std::pair<std::string, std::string>> GatherCoreRegistryIdentifiers() {
    std::vector<std::pair<std::string, std::string>> registryIdentifiers;
    registryIdentifiers.emplace_back(
        "MachineGuid",
        ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid")
    );
    registryIdentifiers.emplace_back(
        "HwProfileGuid",
        ReadRegistryString(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\IDConfigDB\\Hardware Profiles\\0001", L"HwProfileGuid")
    );
    registryIdentifiers.emplace_back(
        "ComputerHardwareId",
        ReadRegistryString(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SystemInformation", L"ComputerHardwareId")
    );

    std::sort(registryIdentifiers.begin(), registryIdentifiers.end());
    registryIdentifiers.erase(std::unique(registryIdentifiers.begin(), registryIdentifiers.end()), registryIdentifiers.end());
    return registryIdentifiers;
}

std::vector<std::pair<std::string, std::string>> GatherTpmRegistryIdentifiers() {
    std::vector<std::pair<std::string, std::string>> identifiers;

    identifiers.emplace_back(
        "TpmDeviceIdentifier",
        ReadRegistryString(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\TPM\\WMI", L"DeviceIdentifier")
    );
    identifiers.emplace_back(
        "TpmInstanceName",
        ReadRegistryString(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\TPM\\WMI", L"InstanceName")
    );
    identifiers.emplace_back(
        "Endorsement",
        ReadRegistryString(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\IntegrityServices", L"Endorsement")
    );

    std::sort(identifiers.begin(), identifiers.end());
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
    return identifiers;
}

void PrintWmiIdentifiers(
    const std::string& label,
    const std::wstring& wmiNamespace,
    const std::wstring& className,
    const std::vector<std::wstring>& properties,
    const std::wstring& whereClause = L""
) {
    std::vector<std::pair<std::string, std::string>> identifiers = GatherWmiIdentifiers(
        wmiNamespace,
        className,
        properties,
        whereClause
    );
    PrintIdentifierBlock(label, identifiers);
}

void PrintHWIDCheckerReport() {
    SColor(8);
    std::cout << " ==== HWID Checker ====" << std::endl;
    std::cout << " \n Common hardware identifiers" << std::endl;
    RColor();

    SColor(8);
    std::cout << " retrieving ndis reserved / adapters" << std::endl;
    RColor();
    PrintIdentifierBlock("NDIS / IF Table", GatherIfTableIdentifiers());

    SColor(8);
    std::cout << " retrieving iftable" << std::endl;
    RColor();
    PrintIdentifierBlock("IF Table", GatherIfTableIdentifiers());

    SColor(8);
    std::cout << " retrieving adapter info" << std::endl;
    RColor();
    PrintIdentifierBlock("Adapter Info", GatherAdapterInfoIdentifiers());

    SColor(8);
    std::cout << " retrieving wmic" << std::endl;
    RColor();

    PrintWmiIdentifiers(
        "SMBIOS / System Product",
        L"ROOT\\CIMV2",
        L"Win32_ComputerSystemProduct",
        { L"UUID", L"IdentifyingNumber", L"Vendor", L"Name" }
    );

    PrintWmiIdentifiers(
        "Motherboard",
        L"ROOT\\CIMV2",
        L"Win32_BaseBoard",
        { L"SerialNumber", L"Manufacturer", L"Product", L"Version" }
    );

    PrintWmiIdentifiers(
        "BIOS / SMBIOS",
        L"ROOT\\CIMV2",
        L"Win32_BIOS",
        { L"SerialNumber", L"SMBIOSBIOSVersion", L"Manufacturer", L"ReleaseDate" }
    );

    PrintWmiIdentifiers(
        "CPU",
        L"ROOT\\CIMV2",
        L"Win32_Processor",
        { L"ProcessorId", L"Name", L"Manufacturer" }
    );

    PrintWmiIdentifiers(
        "Physical Memory",
        L"ROOT\\CIMV2",
        L"Win32_PhysicalMemory",
        { L"SerialNumber", L"PartNumber", L"Manufacturer" }
    );

    PrintWmiIdentifiers(
        "TPM",
        L"ROOT\\CIMv2\\Security\\MicrosoftTpm",
        L"Win32_Tpm",
        {
            L"ManufacturerId",
            L"ManufacturerIdTxt",
            L"ManufacturerVersion",
            L"ManufacturerVersionInfo",
            L"ManufacturerVersionFull20",
            L"SpecVersion",
            L"PhysicalPresenceVersionInfo",
            L"InstanceId",
            L"IsEnabled_InitialValue",
            L"IsActivated_InitialValue",
            L"IsOwned_InitialValue"
        }
    );

    PrintWmiIdentifiers(
        "TPM Device (PnP)",
        L"ROOT\\CIMV2",
        L"Win32_PnPEntity",
        { L"DeviceID", L"PNPDeviceID", L"HardwareID", L"Name", L"ClassGuid" },
        L"PNPClass = 'SecurityDevices'"
    );

    PrintWmiIdentifiers(
        "Disk Drives",
        L"ROOT\\CIMV2",
        L"Win32_DiskDrive",
        { L"SerialNumber", L"Model", L"PNPDeviceID", L"DeviceID" }
    );

    PrintWmiIdentifiers(
        "Physical Media",
        L"ROOT\\CIMV2",
        L"Win32_PhysicalMedia",
        { L"SerialNumber", L"Tag" }
    );

    PrintWmiIdentifiers(
        "Monitors",
        L"ROOT\\CIMV2",
        L"Win32_DesktopMonitor",
        { L"PNPDeviceID", L"MonitorManufacturer", L"Name" }
    );

    PrintWmiIdentifiers(
        "Network Adapters",
        L"ROOT\\CIMV2",
        L"Win32_NetworkAdapter",
        { L"MACAddress", L"PNPDeviceID", L"GUID", L"Name" },
        L"PhysicalAdapter = TRUE"
    );

    PrintWmiIdentifiers(
        "GPU",
        L"ROOT\\CIMV2",
        L"Win32_VideoController",
        { L"PNPDeviceID", L"Name", L"DriverVersion" }
    );

    PrintWmiIdentifiers(
        "System Enclosure",
        L"ROOT\\CIMV2",
        L"Win32_SystemEnclosure",
        { L"SerialNumber", L"SMBIOSAssetTag", L"Manufacturer" }
    );

    PrintWmiIdentifiers(
        "Operating System",
        L"ROOT\\CIMV2",
        L"Win32_OperatingSystem",
        { L"SerialNumber", L"CSName" }
    );

    PrintWmiIdentifiers(
        "Logical Disks",
        L"ROOT\\CIMV2",
        L"Win32_LogicalDisk",
        { L"DeviceID", L"VolumeSerialNumber", L"VolumeName" }
    );

    PrintWmiIdentifiers(
        "Network Adapter Config",
        L"ROOT\\CIMV2",
        L"Win32_NetworkAdapterConfiguration",
        { L"MACAddress", L"SettingID", L"ServiceName" },
        L"IPEnabled = TRUE"
    );

    PrintWmiIdentifiers(
        "Computer System",
        L"ROOT\\CIMV2",
        L"Win32_ComputerSystem",
        { L"Model", L"Manufacturer", L"Name" }
    );

    SColor(8);
    std::cout << " retrieving disk / volumes" << std::endl;
    RColor();
    PrintIdentifierBlock("Storage Volumes", GatherVolumeIdentifiers());

    SColor(8);
    std::cout << " retrieving registry" << std::endl;
    RColor();
    PrintIdentifierBlock("Registry", GatherCoreRegistryIdentifiers());
    PrintIdentifierBlock("Registry Network Adapters", GatherRegistryNetworkIdentifiers());
    PrintIdentifierBlock("Registry TPM", GatherTpmRegistryIdentifiers());
}

bool SaveHWIDSerialsToFile(const std::string& filename) {
    std::ofstream outputFile(filename);
    if (!outputFile.is_open()) {
        return false;
    }

    outputFile << "HWID Serial Export" << std::endl;
    outputFile << "==================" << std::endl << std::endl;

    WriteIdentifierBlockToFile(outputFile, "NDIS / IF Table", GatherIfTableIdentifiers());
    WriteIdentifierBlockToFile(outputFile, "IF Table", GatherIfTableIdentifiers());
    WriteIdentifierBlockToFile(outputFile, "Adapter Info", GatherAdapterInfoIdentifiers());

    WriteIdentifierBlockToFile(
        outputFile,
        "SMBIOS / System Product",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_ComputerSystemProduct", { L"UUID", L"IdentifyingNumber", L"Vendor", L"Name" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Motherboard",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_BaseBoard", { L"SerialNumber", L"Manufacturer", L"Product", L"Version" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "BIOS / SMBIOS",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_BIOS", { L"SerialNumber", L"SMBIOSBIOSVersion", L"Manufacturer", L"ReleaseDate" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "CPU",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_Processor", { L"ProcessorId", L"Name", L"Manufacturer" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Physical Memory",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_PhysicalMemory", { L"SerialNumber", L"PartNumber", L"Manufacturer" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "TPM",
        GatherWmiIdentifiers(
            L"ROOT\\CIMv2\\Security\\MicrosoftTpm",
            L"Win32_Tpm",
            {
                L"ManufacturerId",
                L"ManufacturerIdTxt",
                L"ManufacturerVersion",
                L"ManufacturerVersionInfo",
                L"ManufacturerVersionFull20",
                L"SpecVersion",
                L"PhysicalPresenceVersionInfo",
                L"InstanceId",
                L"IsEnabled_InitialValue",
                L"IsActivated_InitialValue",
                L"IsOwned_InitialValue"
            }
        )
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "TPM Device (PnP)",
        GatherWmiIdentifiers(
            L"ROOT\\CIMV2",
            L"Win32_PnPEntity",
            { L"DeviceID", L"PNPDeviceID", L"HardwareID", L"Name", L"ClassGuid" },
            L"PNPClass = 'SecurityDevices'"
        )
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Disk Drives",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_DiskDrive", { L"SerialNumber", L"Model", L"PNPDeviceID", L"DeviceID" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Physical Media",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_PhysicalMedia", { L"SerialNumber", L"Tag" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Monitors",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_DesktopMonitor", { L"PNPDeviceID", L"MonitorManufacturer", L"Name" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Network Adapters",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_NetworkAdapter", { L"MACAddress", L"PNPDeviceID", L"GUID", L"Name" }, L"PhysicalAdapter = TRUE")
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "GPU",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_VideoController", { L"PNPDeviceID", L"Name", L"DriverVersion" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "System Enclosure",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_SystemEnclosure", { L"SerialNumber", L"SMBIOSAssetTag", L"Manufacturer" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Operating System",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_OperatingSystem", { L"SerialNumber", L"CSName" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Logical Disks",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_LogicalDisk", { L"DeviceID", L"VolumeSerialNumber", L"VolumeName" })
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Network Adapter Config",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_NetworkAdapterConfiguration", { L"MACAddress", L"SettingID", L"ServiceName" }, L"IPEnabled = TRUE")
    );

    WriteIdentifierBlockToFile(
        outputFile,
        "Computer System",
        GatherWmiIdentifiers(L"ROOT\\CIMV2", L"Win32_ComputerSystem", { L"Model", L"Manufacturer", L"Name" })
    );

    WriteIdentifierBlockToFile(outputFile, "Storage Volumes", GatherVolumeIdentifiers());

    WriteIdentifierBlockToFile(outputFile, "Registry", GatherCoreRegistryIdentifiers());
    WriteIdentifierBlockToFile(outputFile, "Registry Network Adapters", GatherRegistryNetworkIdentifiers());
    WriteIdentifierBlockToFile(outputFile, "Registry TPM", GatherTpmRegistryIdentifiers());

    outputFile.close();
    return true;
}

std::string GetWindowsReleaseLabel(DWORD build) {
    // Windows 11 Release-Label anhand der Build-Nummer
    if (build >= 26100) return "25H2";
    if (build >= 26000) return "24H2";
    if (build >= 22631) return "23H2";
    if (build >= 22621) return "22H2";
    if (build >= 22000) return "21H2";
    // Windows 10 Release-Label anhand der Build-Nummer
    if (build >= 19045) return "22H2";
    if (build >= 19044) return "21H2";
    if (build >= 19043) return "21H1";
    if (build >= 19042) return "20H2";
    if (build >= 19041) return "2004";
    if (build >= 18363) return "1909";
    if (build >= 18362) return "1903";
    if (build >= 17763) return "1809";
    // Weitere Releases könnten hier eingefügt werden
    return "Unknown";
}

std::string GetWindowsMainVersion(DWORD major, DWORD build) {
    if (major == 10 && build >= 22000) {
        // Windows 11 meldet sich oft noch als Windows 10, daher hier Build-Nummer prüfen
        return "Windows 11";
    }
    else if (major == 10) {
        return "Windows 10";
    }
    return "Older Windows";
}

void PrintWindowsVersionInfo() {
    HMODULE hMod = GetModuleHandleA("ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion != nullptr) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (RtlGetVersion(&rovi) == 0) {
                std::string version = GetWindowsMainVersion(rovi.dwMajorVersion, rovi.dwBuildNumber);
                std::string release = GetWindowsReleaseLabel(rovi.dwBuildNumber);
                std::cout << "[+] Windows Version: ";
                SColor(15);
                std::cout << version << " " << release;
                std::cout << " (Build " << rovi.dwBuildNumber << ")" << std::endl;
                RColor();
            }
        }
    }
}

void PrintCPUNameWMI() {
    HRESULT hres;

    // Step 1: COM initialisieren
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "Failed to initialize COM library. Error code: 0x" << std::hex << hres << std::endl;
        return;
    }


    // Step 3: WMI-Verbindung
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        CoUninitialize();
        std::cerr << "Failed to create IWbemLocator object. Error code: 0x" << std::hex << hres << std::endl;
        return;
    }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        std::cerr << "Could not connect to WMI. Error code: 0x" << std::hex << hres << std::endl;
        return;
    }

    // Step 4: Sicherheitseinstellungen für den Proxy
    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        std::cerr << "Could not set proxy blanket. Error code: 0x" << std::hex << hres << std::endl;
        return;
    }

    // Step 5: WMI-Abfrage
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT Name FROM Win32_Processor"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        std::cerr << "Query failed. Error code: 0x" << std::hex << hres << std::endl;
        return;
    }

    // Step 6: Ergebnisse auslesen
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    std::cout << "[+] CPU: ";
    SColor(15);
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        VARIANT vtProp;
        hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres)) {
            std::wcout << vtProp.bstrVal << std::endl;
            RColor();
            VariantClear(&vtProp);
        }
        pclsObj->Release();
    }

    // Step 7: Aufräumen
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();
}

void PrintGPUName() {
    DISPLAY_DEVICE device;
    device.cb = sizeof(DISPLAY_DEVICE);

    std::cout << "[+] GPU: ";
    SColor(15);

    for (DWORD i = 0; EnumDisplayDevices(NULL, i, &device, 0); i++) {
        if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
            std::wcout << device.DeviceString << std::endl;
            RColor();
            return;
        }
    }

    if (EnumDisplayDevices(NULL, 0, &device, 0)) {
        std::wcout << device.DeviceString << std::endl;
    }
    else {
        std::cout << "Unknown" << std::endl;
    }
    RColor();
}

void PrintMotherboardInfo() {
    HRESULT hres;

    // COM initialisieren
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return;


    // WMI-Verbindung
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        CoUninitialize();
        return;
    }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // Sicherheitseinstellungen für den Proxy
    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // WMI-Abfrage (nur Manufacturer und Product)
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT Manufacturer,Product FROM Win32_BaseBoard"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // Ergebnisse auslesen
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    std::cout << "[+] Motherboard: ";
    SColor(15);
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        VARIANT vtManufacturer, vtProduct;
        VariantInit(&vtManufacturer);
        VariantInit(&vtProduct);

        hres = pclsObj->Get(L"Manufacturer", 0, &vtManufacturer, 0, 0);
        hres = pclsObj->Get(L"Product", 0, &vtProduct, 0, 0);

        std::wcout << (vtManufacturer.vt == VT_BSTR ? vtManufacturer.bstrVal : L"Unknown") << L" ";
        std::wcout << (vtProduct.vt == VT_BSTR ? vtProduct.bstrVal : L"Unknown") << std::endl;
        RColor();

        VariantClear(&vtManufacturer);
        VariantClear(&vtProduct);
        pclsObj->Release();
    }

    // Aufräumen
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();
}

void CheckUACStatus() {
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(DWORD);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, L"EnableLUA", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            std::cout << "[+] UAC (User Account Control): ";
            if (value == 1) {
                SColor(10); // Green
                std::cout << "Enabled";
            }
            else {
                SColor(12); // Red
                std::cout << "Disabled";
            }
            SColor(7); // Reset
            std::cout << std::endl;
        }
        else {
            std::cout << "[+] UAC: Unknown" << std::endl;
        }
        RegCloseKey(hKey);
    }
    else {
        std::cout << "[+] UAC: Unknown" << std::endl;
    }
}

void CheckVCRedistInstalled() {
    DWORD index = 0;
    TCHAR productCode[39] = { 0 };
    bool vcRedistFound = false;

    while (MsiEnumProducts(index, productCode) == ERROR_SUCCESS) {
        TCHAR productName[256] = { 0 };
        DWORD productNameLen = 256;

        if (MsiGetProductInfo(productCode, INSTALLPROPERTY_INSTALLEDPRODUCTNAME, productName, &productNameLen) == ERROR_SUCCESS) {
            std::wstring name(productName);

            // Prüfe, ob es sich um Visual C++ 2015–2022 handelt
            if (name.find(L"Microsoft Visual C++") != std::wstring::npos &&
                (name.find(L"2015") != std::wstring::npos ||
                    name.find(L"2017") != std::wstring::npos ||
                    name.find(L"2019") != std::wstring::npos ||
                    name.find(L"2022") != std::wstring::npos ||
                    name.find(L"2015-2022") != std::wstring::npos ||
                    name.find(L"2015–2022") != std::wstring::npos)) {
                vcRedistFound = true;
                break;
            }
        }
        index++;
    }

    std::cout << "[+] Visual C++ Redistributable 2015 - 2022: ";
    if (vcRedistFound) {
        SColor(10); // Green
        std::cout << "Installed";
    }
    else {
        SColor(12); // Red
        std::cout << "Not installed";
    }
    SColor(7); // Reset
    std::cout << std::endl;
}

void CheckWindowsDefenderStatus() {
    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (scManager == NULL) {
        std::cout << "[+] Windows Defender: Error (could not open service manager)" << std::endl;
        return;
    }

    SC_HANDLE scService = OpenService(scManager, L"WinDefend", SERVICE_QUERY_STATUS);
    if (scService == NULL) {
        CloseServiceHandle(scManager);
        std::cout << "[+] Windows Defender: Error (could not open service)" << std::endl;
        return;
    }

    SERVICE_STATUS_PROCESS status;
    DWORD bytesNeeded;
    if (!QueryServiceStatusEx(scService, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &bytesNeeded)) {
        CloseServiceHandle(scService);
        CloseServiceHandle(scManager);
        std::cout << "[+] Windows Defender: Error (could not query status)" << std::endl;
        return;
    }

    std::cout << "[+] Windows Defender: ";
    if (status.dwCurrentState == SERVICE_RUNNING) {
        SColor(12); // Red
        std::cout << "Enabled";
    }
    else {
        SColor(10); // Green
        std::cout << "Disabled";
    }
    SColor(7); // Reset
    std::cout << std::endl;

    CloseServiceHandle(scService);
    CloseServiceHandle(scManager);
}

void CheckWindowsFirewallStatus() {
    HRESULT hr = S_OK;
    INetFwMgr* fwMgr = NULL;
    INetFwPolicy* fwPolicy = NULL;
    INetFwProfile* fwProfile = NULL;
    VARIANT_BOOL fwEnabled;

    // COM initialisieren
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cout << "[+] Windows Firewall: Error (COM init failed)" << std::endl;
        return;
    }

    // Firewall Manager erstellen
    hr = CoCreateInstance(__uuidof(NetFwMgr), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwMgr), (void**)&fwMgr);
    if (FAILED(hr) || fwMgr == NULL) {
        CoUninitialize();
        std::cout << "[+] Windows Firewall: Error (could not create firewall manager)" << std::endl;
        return;
    }

    // Firewall Policy holen
    hr = fwMgr->get_LocalPolicy(&fwPolicy);
    if (FAILED(hr) || fwPolicy == NULL) {
        fwMgr->Release();
        CoUninitialize();
        std::cout << "[+] Windows Firewall: Error (could not get firewall policy)" << std::endl;
        return;
    }

    // Aktuelles Profil holen (normalerweise das Standardprofil)
    hr = fwPolicy->get_CurrentProfile(&fwProfile);
    if (FAILED(hr) || fwProfile == NULL) {
        fwPolicy->Release();
        fwMgr->Release();
        CoUninitialize();
        std::cout << "[+] Windows Firewall: Error (could not get firewall profile)" << std::endl;
        return;
    }

    // Firewall-Status abfragen
    hr = fwProfile->get_FirewallEnabled(&fwEnabled);
    if (FAILED(hr)) {
        fwProfile->Release();
        fwPolicy->Release();
        fwMgr->Release();
        CoUninitialize();
        std::cout << "[+] Windows Firewall: Error (could not get firewall status)" << std::endl;
        return;
    }

    std::cout << "[+] Windows Firewall: ";
    if (fwEnabled == VARIANT_TRUE) {
        SColor(12); // Rot = aktiviert
        std::cout << "Enabled";
    }
    else {
        SColor(10); // Grün = deaktiviert
        std::cout << "Disabled";
    }
    SColor(7); // Reset
    std::cout << std::endl;

    // Aufräumen
    fwProfile->Release();
    fwPolicy->Release();
    fwMgr->Release();
    CoUninitialize();
}

void DetectAntivirus() {
    HKEY hKey;
    DWORD index = 0;
    char subkeyName[1024];
    DWORD subkeyNameSize = sizeof(subkeyName);
    bool found = false;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        while (RegEnumKeyExA(hKey, index, subkeyName, &subkeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            HKEY hSubKey;
            char displayName[1024];
            DWORD displayNameSize = sizeof(displayName);
            if (RegOpenKeyExA(hKey, subkeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                if (RegQueryValueExA(hSubKey, "DisplayName", NULL, NULL, (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS) {
                    string name(displayName);
                    if (name.find("antivirus") != string::npos || name.find("AntiVirus") != string::npos ||
                        name.find("Security") != string::npos || name.find("Kaspersky") != string::npos ||
                        name.find("Norton") != string::npos || name.find("Avast") != string::npos ||
                        name.find("AVG") != string::npos || name.find("Bitdefender") != string::npos ||
                        name.find("McAfee") != string::npos || name.find("ESET") != string::npos) {
                        found = true;
                        break;
                    }
                }
                RegCloseKey(hSubKey);
            }
            subkeyNameSize = sizeof(subkeyName);
            index++;
        }
        RegCloseKey(hKey);
    }

    cout << "[+] Third-Party-Antivirus: ";
    if (found) {
        SColor(12); // Rot (FOREGROUND_RED)
        cout << "Installed";
    }
    else {
        SColor(10); // Grün (FOREGROUND_GREEN)
        cout << "Not Installed";
    }
    RColor();
    wcout << endl;
}

bool IsAntiCheatRunning(const wstring& processName) {
    HANDLE hProcessSnap;
    PROCESSENTRY32W pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return false;
    }
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    if (!Process32FirstW(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return false;
    }
    do {
        wstring exeName = pe32.szExeFile;
        if (exeName.find(processName) != wstring::npos) {
            CloseHandle(hProcessSnap);
            return true;
        }
    } while (Process32NextW(hProcessSnap, &pe32));
    CloseHandle(hProcessSnap);
    return false;
}

void DetectAntiCheat() {
    vector<wstring> antiCheatProcesses = {
        L"EasyAntiCheat.exe",
        L"EasyAntiCheat_launcher.exe",
        L"BEService.exe",
        L"BEService_x64.exe",
        L"vgtray.exe",
        L"faceit_client.exe"
    };

    bool found = false;
    for (const auto& process : antiCheatProcesses) {
        if (IsAntiCheatRunning(process)) {
            found = true;
            break;
        }
    }

    wcout << L"[+] Anti-Cheat: ";
    if (found) {
        SColor(12); // Rot
        wcout << L"Installed";
    }
    else {
        SColor(10); // Grün
        wcout << L"Not Installed";
    }
    RColor();
    wcout << endl;
}

bool IsFastStartupEnabled() {
    HKEY hKey;
    DWORD value = 0;
    DWORD dataSize = sizeof(value);

    // Fast Startup ist im Schlüssel HiberbootEnabled unter HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Power
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false; // Schlüssel nicht gefunden, Standardwert ist nicht aktiviert
    }

    if (RegQueryValueExA(hKey, "HiberbootEnabled", NULL, NULL, (LPBYTE)&value, &dataSize) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false; // Wert nicht gefunden, Standardwert ist nicht aktiviert
    }
    RegCloseKey(hKey);

    return value != 0;
}

void DetectFastStartup() {
    bool enabled = IsFastStartupEnabled();

    cout << "[+] Fast Startup: ";
    if (enabled) {
        SColor(12); // Rot
        cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        cout << "Disabled";
    }
    RColor();
    cout << endl;
}

void DetectSecureBootWMI() {
    HKEY hKey;
    DWORD value = 0, size = sizeof(DWORD);
    bool secureBootEnabled = false;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"UEFISecureBootEnabled", nullptr, nullptr, (LPBYTE)&value, &size) == ERROR_SUCCESS && value == 1) {
            secureBootEnabled = true;
        }
        RegCloseKey(hKey);
    }

    cout << "[+] Secure Boot: ";
    if (secureBootEnabled) {
        SColor(12); // Rot
        cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        cout << "Disabled";
    }
    RColor();
    cout << endl;
}

void DetectVbsWmi() {
    HRESULT hres;
    bool vbsEnabled = false;

    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        cout << "[+] VBS: Failed to initialize COM" << endl;
        return;
    }


    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        cout << "[+] VBS: Failed to create WMI locator" << endl;
        CoUninitialize();
        return;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\Microsoft\\Windows\\DeviceGuard"), // WMI namespace für DeviceGuard/VBS
        NULL,                                             // User name
        NULL,                                             // User password
        0,                                                // Locale
        0,                                                // Security flags
        0,                                                // Authority
        0,                                                // Context object
        &pSvc
    );
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        cout << "[+] VBS: Failed to connect to WMI" << endl;
        return;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] VBS: Failed to set proxy blanket" << endl;
        return;
    }

    // Use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT VirtualizationBasedSecurityStatus FROM Win32_DeviceGuard"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] VBS: Failed to execute WMI query" << endl;
        return;
    }

    // Get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;

        VARIANT vtProp;
        VariantInit(&vtProp);

        // Get the value of the VirtualizationBasedSecurityStatus property
        hres = pclsObj->Get(L"VirtualizationBasedSecurityStatus", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_I4) {
            // Laut Microsoft-Doku: 2 = VBS aktiviert und laufend
            if (vtProp.intVal == 2) {
                vbsEnabled = true;
            }
        }
        VariantClear(&vtProp);
        pclsObj->Release();
    }

    // Cleanup
    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();

    // Output in one line with color
    cout << "[+] Virtualization Based Security: ";
    if (vbsEnabled) {
        SColor(12); // Rot
        cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        cout << "Disabled";
    }
    RColor();
    cout << endl;
}

void DetectVirtualizationWmi() {
    HRESULT hres;
    bool virtualizationEnabled = false;

    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        cout << "[+] Virtualization: Failed to initialize COM" << endl;
        return;
    }


    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        cout << "[+] Virtualization: Failed to create WMI locator" << endl;
        CoUninitialize();
        return;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // Standard namespace
        NULL,                    // User name
        NULL,                    // User password
        0,                       // Locale
        0,                       // Security flags
        0,                       // Authority
        0,                       // Context object
        &pSvc
    );
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        cout << "[+] Virtualization: Failed to connect to WMI" << endl;
        return;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] Virtualization: Failed to set proxy blanket" << endl;
        return;
    }

    // Use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT VirtualizationFirmwareEnabled FROM Win32_Processor"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] Virtualization: Failed to execute WMI query" << endl;
        return;
    }

    // Get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;

        VARIANT vtProp;
        VariantInit(&vtProp);

        // Get the value of the VirtualizationFirmwareEnabled property
        hres = pclsObj->Get(L"VirtualizationFirmwareEnabled", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BOOL) {
            virtualizationEnabled = (vtProp.boolVal != VARIANT_FALSE);
        }
        VariantClear(&vtProp);
        pclsObj->Release();
    }

    // Cleanup
    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();

    // Output in one line with color
    cout << "[+] Virtualization: ";
    if (virtualizationEnabled) {
        SColor(10); // Grün (Virtualisierung aktiv = gut)
        cout << "Enabled";
    }
    else {
        SColor(12); // Rot (Virtualisierung deaktiviert = Warnung)
        cout << "Disabled";
    }
    RColor();
    cout << endl;
}

void DetectTpmWmi() {
    HRESULT hres;
    bool tpmEnabled = false;

    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        cout << "[+] TPM: Failed to initialize COM" << endl;
        return;
    }


    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        cout << "[+] TPM: Failed to create WMI locator" << endl;
        CoUninitialize();
        return;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMv2\\Security\\MicrosoftTpm"), // WMI namespace für TPM
        NULL,                                            // User name
        NULL,                                            // User password
        0,                                               // Locale
        0,                                               // Security flags
        0,                                               // Authority
        0,                                               // Context object
        &pSvc
    );
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        cout << "[+] TPM: Failed to connect to WMI" << endl;
        return;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] TPM: Failed to set proxy blanket" << endl;
        return;
    }

    // Use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT IsEnabled_InitialValue, IsActivated_InitialValue FROM Win32_Tpm"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] TPM: Failed to execute WMI query" << endl;
        return;
    }

    // Get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;

        VARIANT vtEnabled, vtActivated;
        VariantInit(&vtEnabled);
        VariantInit(&vtActivated);

        // Get the value of the IsEnabled_InitialValue property
        hres = pclsObj->Get(L"IsEnabled_InitialValue", 0, &vtEnabled, 0, 0);
        if (SUCCEEDED(hres) && vtEnabled.vt == VT_BOOL && vtEnabled.boolVal != VARIANT_FALSE) {
            // Get the value of the IsActivated_InitialValue property
            hres = pclsObj->Get(L"IsActivated_InitialValue", 0, &vtActivated, 0, 0);
            if (SUCCEEDED(hres) && vtActivated.vt == VT_BOOL && vtActivated.boolVal != VARIANT_FALSE) {
                tpmEnabled = true;
            }
        }
        VariantClear(&vtEnabled);
        VariantClear(&vtActivated);
        pclsObj->Release();
    }

    // Cleanup
    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();

    // Output in one line with color
    cout << "[+] Trusted Platform Module: ";
    if (tpmEnabled) {
        SColor(12); // Rot
        cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        cout << "Disabled";
    }
    RColor();
    cout << endl;
}

void DetectBiosBootModeWmi() {
    HRESULT hres;
    bool isUefi = false;

    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        cout << "[+] Boot Mode: Failed to initialize COM" << endl;
        return;
    }


    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        cout << "[+] Boot Mode: Failed to create WMI locator" << endl;
        CoUninitialize();
        return;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // Standard namespace
        NULL,                    // User name
        NULL,                    // User password
        0,                       // Locale
        0,                       // Security flags
        0,                       // Authority
        0,                       // Context object
        &pSvc
    );
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        cout << "[+] Boot Mode: Failed to connect to WMI" << endl;
        return;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] Boot Mode: Failed to set proxy blanket" << endl;
        return;
    }

    // Use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT BootDevice, SystemDrive FROM Win32_OperatingSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        cout << "[+] Boot Mode: Failed to execute WMI query" << endl;
        return;
    }

    // Get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) { // Korrektur: p1Enumerator → pEnumerator
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;

        VARIANT vtBootDevice, vtSystemDrive;
        VariantInit(&vtBootDevice);
        VariantInit(&vtSystemDrive);

        // Get the value of the BootDevice property
        hres = pclsObj->Get(L"BootDevice", 0, &vtBootDevice, 0, 0);
        if (SUCCEEDED(hres) && vtBootDevice.vt == VT_BSTR) {
            wstring bootDevice(vtBootDevice.bstrVal);
            // Einfache Heuristik: Wenn "UEFI" im Pfad, dann UEFI-Boot
            if (bootDevice.find(L"UEFI") != wstring::npos) {
                isUefi = true;
            }
        }
        VariantClear(&vtBootDevice);
        VariantClear(&vtSystemDrive);
        pclsObj->Release();
    }

    // Cleanup
    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();

    // Output in one line with color
    cout << "[+] Boot Mode: ";
    if (isUefi) {
        SColor(10); // Rot
        cout << "UEFI";
    }
    else {
        SColor(10); // Grün
        cout << "Legacy/BIOS";
    }
    RColor();
    cout << endl;
}

bool IsDirectXInstalled() {
    // Typische DirectX-Bibliothek, z.B. d3d11.dll (Direct3D 11)
    HMODULE hDll = LoadLibraryA("d3d11.dll");
    if (hDll) {
        FreeLibrary(hDll);
        return true;
    }
    // Alternativ: d3dx9.dll (Direct3D 9 Extensions)
    hDll = LoadLibraryA("d3dx9.dll");
    if (hDll) {
        FreeLibrary(hDll);
        return true;
    }
    return false;
}

// Hinweis: Es gibt keine zuverlässige Methode, die genaue DirectX-Version abzufragen.
// Diese Funktion gibt nur einen Hinweis auf die installierte Version.
string GetDirectXVersionHint() {
    HMODULE h;
    h = LoadLibraryA("d3d12.dll");
    if (h) { FreeLibrary(h); return "DirectX 12 or higher"; }
    h = LoadLibraryA("d3d11.dll");
    if (h) { FreeLibrary(h); return "DirectX 11 or higher"; }
    h = LoadLibraryA("d3d10.dll");
    if (h) { FreeLibrary(h); return "DirectX 10 or higher"; }
    h = LoadLibraryA("d3d9.dll");
    if (h) { FreeLibrary(h); return "DirectX 9 or higher"; }
    return "DirectX not found";
}

void DetectDirectX() {
    bool installed = IsDirectXInstalled();
    string versionHint = GetDirectXVersionHint();

    cout << "[+] DirectX: ";
    if (installed) {
        SColor(10); // Grün
        cout << "Installed (" << versionHint << ")";
    }
    else {
        SColor(12); // Rot
        cout << "Not Installed";
    }
    RColor();
    cout << endl;
}

bool IsHyperVEnabled() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE service = OpenServiceA(scm, "vmms", SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS_PROCESS status;
    DWORD bytesNeeded;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &bytesNeeded)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return false;
    }

    bool isRunning = (status.dwCurrentState == SERVICE_RUNNING);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return isRunning;
}

void DetectHyperV() {
    bool enabled = IsHyperVEnabled();

    cout << "[+] Hyper-V: ";
    if (enabled) {
        // Rot, wenn aktiviert (wie in deinem Beispiel für Warnungen)
        // Oder grün, je nach Konvention (hier: rot wie bei Fast Startup enabled)
        SColor(12); // Rot
        cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        cout << "Disabled";
    }
    RColor();
    cout << endl;
}

bool IsVirtualMachine() {
    HRESULT hres;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return false;


    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) { CoUninitialize(); return false; }

    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) { pLoc->Release(); CoUninitialize(); return false; }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); CoUninitialize(); return false; }

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_ComputerSystem"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); CoUninitialize(); return false; }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    bool isVM = false;

    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        VARIANT vtProp;
        hres = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres)) {
            std::wstring manufacturer(vtProp.bstrVal);
            VariantClear(&vtProp);
            std::transform(manufacturer.begin(), manufacturer.end(), manufacturer.begin(), ::towlower);
            if (manufacturer.find(L"vmware") != std::wstring::npos ||
                manufacturer.find(L"virtualbox") != std::wstring::npos ||
                manufacturer.find(L"qemu") != std::wstring::npos ||
                manufacturer.find(L"innotek") != std::wstring::npos) {
                isVM = true;
            }
        }

        hres = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres)) {
            std::wstring model(vtProp.bstrVal);
            VariantClear(&vtProp);
            std::transform(model.begin(), model.end(), model.begin(), ::towlower);
            if (model.find(L"virtual") != std::wstring::npos ||
                model.find(L"vmware") != std::wstring::npos ||
                model.find(L"virtualbox") != std::wstring::npos ||
                model.find(L"qemu") != std::wstring::npos ||
                model.find(L"hyper-v") != std::wstring::npos) {
                isVM = true;
            }
        }

        pclsObj->Release();
    }

    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return isVM;
}

void DetectVirtualMachine() {
    bool isVM = IsVirtualMachine();

    std::cout << "[+] Virtual Machine: ";
    if (isVM) {
        // Rot, wenn VM erkannt (wie bei Warnungen)
        SColor(12);
        std::cout << "Detected";
    }
    else {
        SColor(10); // Grün
        std::cout << "Not Detected";
    }
    RColor();
    std::cout << std::endl;
}

void CheckMemoryIntegrity() {
    HKEY hKey;
    DWORD value = 0, size = sizeof(DWORD);
    bool hvciEnabled = false;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"Enabled", nullptr, nullptr, (LPBYTE)&value, &size) == ERROR_SUCCESS && value == 1) {
            hvciEnabled = true;
        }
        RegCloseKey(hKey);
    }

    std::cout << "[+] Core Isolation: ";
    if (hvciEnabled) {
        SColor(12); // Rot
        std::cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        std::cout << "Disabled";
    }
    SColor(7); // Standard
    std::cout << std::endl;
}

void CheckVulnerableDriverBlocklist() {
    HKEY hKey;
    DWORD value = 0, size = sizeof(DWORD);
    bool blocklistEnabled = false;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\CI\\Config", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"VulnerableDriverBlocklistEnable", nullptr, nullptr, (LPBYTE)&value, &size) == ERROR_SUCCESS && value == 1) {
            blocklistEnabled = true;
        }
        RegCloseKey(hKey);
    }

    std::cout << "[+] Vulnerable Driver Blocklist: ";
    if (blocklistEnabled) {
        SColor(12); // Rot
        std::cout << "Enabled";
    }
    else {
        SColor(10); // Grün
        std::cout << "Disabled";
    }
    SColor(7); // Standard
    std::cout << std::endl;
}

// ========== NEW SYSTEM CHECKS ==========

void PrintRAMAmount() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        double totalGB = (double)memInfo.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
        std::cout << "[+] RAM: ";
        SColor(15);
        std::cout << std::fixed;
        std::cout.precision(1);
        std::cout << totalGB << " GB";
        RColor();
        std::cout << std::endl;
    }
    else {
        std::cout << "[+] RAM: Unknown" << std::endl;
    }
}

void PrintNetworkAdapter() {
    HRESULT hres;

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return;

    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) { CoUninitialize(); return; }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) { pLoc->Release(); CoUninitialize(); return; }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); CoUninitialize(); return; }

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT Name, NetConnectionStatus FROM Win32_NetworkAdapter WHERE NetConnectionID IS NOT NULL AND NetConnectionStatus = 2"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); CoUninitialize(); return; }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    bool found = false;

    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        VARIANT vtName;
        VariantInit(&vtName);
        hres = pclsObj->Get(L"Name", 0, &vtName, 0, 0);
        if (SUCCEEDED(hres) && vtName.vt == VT_BSTR) {
            if (!found) {
                std::cout << "[+] Network: ";
                SColor(15);
                found = true;
            }
            std::wcout << vtName.bstrVal;
            std::cout << " (Connected)";
            RColor();
            std::cout << std::endl;
        }
        VariantClear(&vtName);
        pclsObj->Release();
    }

    if (!found) {
        std::cout << "[+] Network: ";
        SColor(12);
        std::cout << "No active connection";
        RColor();
        std::cout << std::endl;
    }

    if (pEnumerator) pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
}

void PrintDotNetVersion() {
    HKEY hKey;
    DWORD release = 0;
    DWORD size = sizeof(DWORD);

    std::cout << "[+] .NET Framework: ";

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"Release", NULL, NULL, (LPBYTE)&release, &size) == ERROR_SUCCESS) {
            SColor(10);
            if (release >= 533320) std::cout << "4.8.1+";
            else if (release >= 528040) std::cout << "4.8";
            else if (release >= 461808) std::cout << "4.7.2";
            else if (release >= 461308) std::cout << "4.7.1";
            else if (release >= 460798) std::cout << "4.7";
            else if (release >= 394802) std::cout << "4.6.2";
            else if (release >= 394254) std::cout << "4.6.1";
            else if (release >= 393295) std::cout << "4.6";
            else std::cout << "4.5+";
            RColor();
        }
        else {
            SColor(12);
            std::cout << "Not Found";
            RColor();
        }
        RegCloseKey(hKey);
    }
    else {
        SColor(12);
        std::cout << "Not Found";
        RColor();
    }
    std::cout << std::endl;
}

void CheckWindowsUpdateStatus() {
    HKEY hKey;
    bool found = false;

    std::cout << "[+] Windows Update: ";

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\Results\\Install", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t lastTime[256] = { 0 };
        DWORD timeSize = sizeof(lastTime);
        if (RegQueryValueExW(hKey, L"LastSuccessTimeStart", NULL, NULL, (LPBYTE)lastTime, &timeSize) == ERROR_SUCCESS) {
            found = true;
            SColor(10);
            std::wcout << L"Last: " << lastTime;
            RColor();
        }
        RegCloseKey(hKey);
    }

    if (!found) {
        // Fallback: check if update service is running
        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (scm) {
            SC_HANDLE svc = OpenServiceA(scm, "wuauserv", SERVICE_QUERY_STATUS);
            if (svc) {
                SERVICE_STATUS_PROCESS status;
                DWORD bytesNeeded;
                if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &bytesNeeded)) {
                    if (status.dwCurrentState == SERVICE_RUNNING) {
                        SColor(10);
                        std::cout << "Service Running";
                    }
                    else {
                        SColor(12);
                        std::cout << "Service Stopped";
                    }
                    RColor();
                }
                CloseServiceHandle(svc);
            }
            else {
                SColor(8);
                std::cout << "Unknown";
                RColor();
            }
            CloseServiceHandle(scm);
        }
    }
    std::cout << std::endl;
}

// ========== FIX FUNCTIONS ==========

bool SetRegistryDWORD(HKEY hRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD value) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(hRoot, subKey, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        // Try to create the key if it doesn't exist
        result = RegCreateKeyExW(hRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL);
        if (result != ERROR_SUCCESS) return false;
    }
    result = RegSetValueExW(hKey, valueName, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

void FixDisableFastStartup() {
    bool success = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power",
        L"HiberbootEnabled", 0);
    if (success) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Fast Startup disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not disable Fast Startup" << endl;
    }
}

void FixDisableCoreIsolation() {
    bool s1 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
        L"Enabled", 0);
    // Additional Memory Integrity registry keys (recommended approach)
    bool s2 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
        L"FeatureSettingsOverride", 3);
    bool s3 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
        L"FeatureSettingsOverrideMask", 3);
    if (s1) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Core Isolation (HVCI) disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not disable Core Isolation" << endl;
    }
}

void FixDisableVBS() {
    bool s1 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
        L"EnableVirtualizationBasedSecurity", 0);
    bool s2 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
        L"RequirePlatformSecurityFeatures", 0);
    if (s1 && s2) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Virtualization Based Security disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not fully disable VBS" << endl;
    }
}

void FixHighPerformancePlan() {
    int result = system("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>&1");
    if (result == 0) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Power plan set to High Performance" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not set power plan" << endl;
    }
}

void FixFlushDNS() {
    int result = system("ipconfig /flushdns >nul 2>&1");
    if (result == 0) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "DNS cache flushed" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not flush DNS" << endl;
    }
}

void FixResetWinsock() {
    int result = system("netsh winsock reset >nul 2>&1");
    if (result == 0) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Winsock reset (requires restart)" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not reset Winsock" << endl;
    }
}

void FixClearTemp() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    // Delete all files in temp
    std::string delCmd = "del /q /f /s \"" + std::string(tempPath) + "\\*\" >nul 2>&1";
    system(delCmd.c_str());
    // Remove all subdirectories in temp
    std::string rdCmd = "for /d %x in (\"" + std::string(tempPath) + "\\*\") do @rd /s /q \"%x\" >nul 2>&1";
    // system() uses cmd /c, so we need %% for for loops in batch
    std::string batchCmd = "cmd /c \"for /d %x in (\"" + std::string(tempPath) + "*\") do @rd /s /q \"%x\"\" >nul 2>&1";
    system(batchCmd.c_str());
    // Also clear Windows prefetch
    system("del /q /f /s \"C:\\Windows\\Prefetch\\*\" >nul 2>&1");
    SColor(10); cout << "  [OK] "; RColor(); cout << "Temp files and prefetch cleared" << endl;
}

void FixDisableDriverBlocklist() {
    bool success = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\CI\\Config",
        L"VulnerableDriverBlocklistEnable", 0);
    if (success) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Vulnerable Driver Blocklist disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not disable Driver Blocklist" << endl;
    }
}

bool FileExistsA(const char* path) {
    DWORD attribs = GetFileAttributesA(path);
    return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY));
}

bool DControlExists() {
    return FileExistsA("dControl.exe") || FileExistsA("dcontrol.exe");
}

std::string GetDControlPath() {
    if (FileExistsA("dControl.exe")) return "dControl.exe";
    if (FileExistsA("dcontrol.exe")) return "dcontrol.exe";
    return "";
}

bool DownloadDControl() {
    HRESULT hr = URLDownloadToFileA(NULL, 
        "https://cloudfinity.win/u/bViQ9yIEgq/files/dc/dControl.exe",
        "dControl.exe", 0, NULL);
    return SUCCEEDED(hr) && FileExistsA("dControl.exe");
}

bool IsDefenderDisabled() {
    // Check the registry key that dControl sets when disabling Defender
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(DWORD);

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Policies\\Microsoft\\Windows Defender", 
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"DisableAntiSpyware", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 1;
        }
        RegCloseKey(hKey);
    }
    return false; // Key not found = Defender not disabled via dControl
}

// ========== ADDITIONAL FIX FUNCTIONS ==========

void FixDisableDefenderService() {
    system("sc config WinDefend start= disabled >nul 2>&1");
    system("sc stop WinDefend >nul 2>&1");
    system("sc config WdNisSvc start= disabled >nul 2>&1");
    system("sc stop WdNisSvc >nul 2>&1");
    SColor(10); cout << "  [OK] "; RColor(); cout << "Windows Defender service disabled" << endl;
}

void FixDisableFirewall() {
    system("netsh advfirewall set allprofiles state off >nul 2>&1");
    SColor(10); cout << "  [OK] "; RColor(); cout << "Windows Firewall disabled" << endl;
}

void FixDisableGameDVR() {
    bool s1 = SetRegistryDWORD(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
        L"AppCaptureEnabled", 0);
    bool s2 = SetRegistryDWORD(HKEY_CURRENT_USER,
        L"System\\GameConfigStore",
        L"GameDVR_Enabled", 0);
    if (s1 || s2) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Game DVR / Game Bar disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not disable Game DVR" << endl;
    }
}

void FixDisableSysMain() {
    system("sc config SysMain start= disabled >nul 2>&1");
    system("sc stop SysMain >nul 2>&1");
    SColor(10); cout << "  [OK] "; RColor(); cout << "SysMain (Superfetch) disabled" << endl;
}

void FixDisableMouseAcceleration() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Mouse", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "MouseSpeed", 0, REG_SZ, (const BYTE*)"0", 2);
        RegSetValueExA(hKey, "MouseThreshold1", 0, REG_SZ, (const BYTE*)"0", 2);
        RegSetValueExA(hKey, "MouseThreshold2", 0, REG_SZ, (const BYTE*)"0", 2);
        RegCloseKey(hKey);
        SColor(10); cout << "  [OK] "; RColor(); cout << "Mouse acceleration disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not disable mouse acceleration" << endl;
    }
}

void FixDisableSmartScreen() {
    // Disable SmartScreen for Explorer
    bool s1 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableSmartScreen", 0);
    // Disable SmartScreen for Edge
    bool s2 = SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\PhishingFilter",
        L"EnabledV9", 0);
    // Disable SmartScreen app check
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        const char* val = "Off";
        RegSetValueExA(hKey, "SmartScreenEnabled", 0, REG_SZ, (const BYTE*)val, (DWORD)strlen(val) + 1);
        RegCloseKey(hKey);
    }
    if (s1) {
        SColor(10); cout << "  [OK] "; RColor(); cout << "Windows SmartScreen disabled" << endl;
    }
    else {
        SColor(12); cout << "  [FAIL] "; RColor(); cout << "Could not disable SmartScreen" << endl;
    }
}

void FixDisableErrorReporting() {
    system("sc config WerSvc start= disabled >nul 2>&1");
    system("sc stop WerSvc >nul 2>&1");
    SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting",
        L"Disabled", 1);
    SColor(10); cout << "  [OK] "; RColor(); cout << "Windows Error Reporting disabled" << endl;
}

void FixDisableTelemetry() {
    // Disable DiagTrack service
    system("sc config DiagTrack start= disabled >nul 2>&1");
    system("sc stop DiagTrack >nul 2>&1");
    // Disable dmwappushservice
    system("sc config dmwappushservice start= disabled >nul 2>&1");
    system("sc stop dmwappushservice >nul 2>&1");
    // Set telemetry level to 0 (Security only)
    SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"AllowTelemetry", 0);
    SColor(10); cout << "  [OK] "; RColor(); cout << "Telemetry and DiagTrack disabled" << endl;
}

void FixDisableASLR() {
    // System-wide ASLR opt-out via MitigationOptions
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0; // Disable mandatory ASLR
        RegSetValueExW(hKey, L"MitigationOptions", 0, REG_QWORD, (const BYTE*)&value, sizeof(DWORD64));
        RegCloseKey(hKey);
    }
    // Also disable via Exploit Protection override
    SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
        L"MoveImages", 0);
    SColor(10); cout << "  [OK] "; RColor(); cout << "ASLR (mandatory) disabled" << endl;
}

void FixDisableCFG() {
    // Disable Control Flow Guard system-wide
    SetRegistryDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
        L"EnableCfg", 0);
    SColor(10); cout << "  [OK] "; RColor(); cout << "Control Flow Guard (CFG) disabled" << endl;
}

