#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct MappedFile {
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE mapping = nullptr;
    BYTE* data = nullptr;

    ~MappedFile() {
        if (data != nullptr) {
            UnmapViewOfFile(data);
        }
        if (mapping != nullptr) {
            CloseHandle(mapping);
        }
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
    }
};

struct ModuleRange {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::wstring name;
};

std::wstring ReadMinidumpString(const BYTE* base, RVA rva) {
    if (rva == 0) {
        return {};
    }
    const auto* value = reinterpret_cast<const MINIDUMP_STRING*>(base + rva);
    return std::wstring(value->Buffer, value->Length / sizeof(wchar_t));
}

const ModuleRange* FindModule(const std::vector<ModuleRange>& modules, std::uint64_t address) {
    const auto found = std::find_if(modules.begin(), modules.end(), [address](const ModuleRange& module) {
        return address >= module.start && address < module.end;
    });
    return found == modules.end() ? nullptr : &*found;
}

std::wstring FileNameOnly(const std::wstring& path) {
    return std::filesystem::path(path).filename().wstring();
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::wcerr << L"Usage: minidump_stack_probe.exe <winamp-crash.dmp>\n";
        return 1;
    }

    MappedFile dump;
    dump.file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dump.file == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Unable to open dump (error " << GetLastError() << L").\n";
        return 2;
    }
    dump.mapping = CreateFileMappingW(dump.file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (dump.mapping == nullptr) {
        std::wcerr << L"Unable to map dump (error " << GetLastError() << L").\n";
        return 3;
    }
    dump.data = static_cast<BYTE*>(MapViewOfFile(dump.mapping, FILE_MAP_READ, 0, 0, 0));
    if (dump.data == nullptr) {
        std::wcerr << L"Unable to view dump (error " << GetLastError() << L").\n";
        return 4;
    }

    PMINIDUMP_DIRECTORY directory = nullptr;
    PVOID stream = nullptr;
    ULONG streamSize = 0;

    if (!MiniDumpReadDumpStream(dump.data, ModuleListStream, &directory, &stream, &streamSize)) {
        std::wcerr << L"Module list is unavailable.\n";
        return 5;
    }
    const auto* moduleList = static_cast<const MINIDUMP_MODULE_LIST*>(stream);
    std::vector<ModuleRange> modules;
    modules.reserve(moduleList->NumberOfModules);
    for (ULONG index = 0; index < moduleList->NumberOfModules; ++index) {
        const auto& module = moduleList->Modules[index];
        modules.push_back(ModuleRange{
            module.BaseOfImage,
            module.BaseOfImage + module.SizeOfImage,
            ReadMinidumpString(dump.data, module.ModuleNameRva),
        });
    }

    if (!MiniDumpReadDumpStream(dump.data, ExceptionStream, &directory, &stream, &streamSize)) {
        std::wcerr << L"Exception stream is unavailable.\n";
        return 6;
    }
    const auto* exception = static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
    if (exception->ThreadContext.DataSize < sizeof(CONTEXT)) {
        std::wcerr << L"Unexpected thread-context size: "
                   << exception->ThreadContext.DataSize << L" bytes.\n";
        return 7;
    }
    const auto* context = reinterpret_cast<const CONTEXT*>(
        dump.data + exception->ThreadContext.Rva);

    std::wcout << L"Dump: " << argv[1] << L"\n"
               << L"Exception: 0x" << std::hex << std::uppercase
               << exception->ExceptionRecord.ExceptionCode << L" at 0x"
               << exception->ExceptionRecord.ExceptionAddress << L"\n"
               << L"Thread: " << std::dec << exception->ThreadId << L"\n"
               << L"EIP=0x" << std::hex << context->Eip
               << L" ESP=0x" << context->Esp
               << L" EBP=0x" << context->Ebp << L"\n";

    const auto* faultModule = FindModule(modules, context->Eip);
    if (faultModule != nullptr) {
        std::wcout << L"Fault module: " << FileNameOnly(faultModule->name)
                   << L"+0x" << std::hex << (context->Eip - faultModule->start) << L"\n";
    }

    if (!MiniDumpReadDumpStream(dump.data, ThreadListStream, &directory, &stream, &streamSize)) {
        std::wcerr << L"Thread list is unavailable.\n";
        return 8;
    }
    const auto* threadList = static_cast<const MINIDUMP_THREAD_LIST*>(stream);
    const MINIDUMP_THREAD* crashThread = nullptr;
    for (ULONG index = 0; index < threadList->NumberOfThreads; ++index) {
        if (threadList->Threads[index].ThreadId == exception->ThreadId) {
            crashThread = &threadList->Threads[index];
            break;
        }
    }
    if (crashThread == nullptr || crashThread->Stack.Memory.DataSize == 0) {
        std::wcerr << L"Crash-thread stack memory is unavailable.\n";
        return 9;
    }

    const auto stackStart = crashThread->Stack.StartOfMemoryRange;
    const auto stackEnd = stackStart + crashThread->Stack.Memory.DataSize;
    if (context->Esp < stackStart || context->Esp >= stackEnd) {
        std::wcerr << L"ESP is outside captured stack memory.\n";
        return 10;
    }

    const auto stackOffset = static_cast<std::size_t>(context->Esp - stackStart);
    const BYTE* stackBytes = dump.data + crashThread->Stack.Memory.Rva + stackOffset;
    const auto bytesAvailable = static_cast<std::size_t>(stackEnd - context->Esp);
    const auto wordCount = std::min<std::size_t>(bytesAvailable / sizeof(std::uint32_t), 512);

    std::wcout << L"EBP frame chain:\n";
    std::uint32_t framePointer = context->Ebp;
    for (int frame = 0; frame < 64; ++frame) {
        if (framePointer < stackStart ||
            static_cast<std::uint64_t>(framePointer) + 2 * sizeof(std::uint32_t) > stackEnd) {
            break;
        }
        const auto frameOffset = static_cast<std::size_t>(framePointer - stackStart);
        const BYTE* frameBytes = dump.data + crashThread->Stack.Memory.Rva + frameOffset;
        std::uint32_t nextFramePointer = 0;
        std::uint32_t returnAddress = 0;
        std::memcpy(&nextFramePointer, frameBytes, sizeof(nextFramePointer));
        std::memcpy(&returnAddress, frameBytes + sizeof(nextFramePointer), sizeof(returnAddress));
        const auto* module = FindModule(modules, returnAddress);
        std::wcout << L"  #" << std::dec << frame << L" return=0x" << std::hex
                   << returnAddress;
        if (module != nullptr) {
            std::wcout << L" " << FileNameOnly(module->name) << L"+0x"
                       << (returnAddress - module->start);
        }
        std::wcout << L"\n";
        if (nextFramePointer <= framePointer) {
            break;
        }
        framePointer = nextFramePointer;
    }

    std::wcout << L"Potential code addresses on the crashing stack:\n";
    std::wstring previousModule;
    std::uint64_t previousAddress = 0;
    int printed = 0;
    for (std::size_t index = 0; index < wordCount && printed < 80; ++index) {
        std::uint32_t value = 0;
        std::memcpy(&value, stackBytes + index * sizeof(value), sizeof(value));
        const auto* module = FindModule(modules, value);
        if (module == nullptr) {
            continue;
        }
        const auto moduleName = FileNameOnly(module->name);
        if (moduleName == previousModule && value == previousAddress) {
            continue;
        }
        std::wcout << L"  ESP+0x" << std::hex << index * sizeof(value)
                   << L": " << moduleName << L"+0x" << (value - module->start) << L"\n";
        previousModule = moduleName;
        previousAddress = value;
        ++printed;
    }

    return 0;
}
