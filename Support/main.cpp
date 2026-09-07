#include "main.h"

#include <array>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace
{
constexpr const char* kAccessKeyFile = "support.key";
constexpr size_t kAccessKeyBytes = 16;

filesystem::path GetAccessKeyPath()
{
	char executablePath[MAX_PATH]{};
	DWORD pathLength = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
	if (pathLength == 0 || pathLength >= MAX_PATH)
	{
		return filesystem::path(kAccessKeyFile);
	}

	return filesystem::path(executablePath).parent_path() / kAccessKeyFile;
}

string GenerateAccessKey()
{
	array<unsigned char, kAccessKeyBytes> randomBytes{};
	if (BCryptGenRandom(nullptr, randomBytes.data(), static_cast<ULONG>(randomBytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
	{
		return {};
	}

	constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	string accessKey;
	accessKey.reserve(kAccessKeyBytes * 2);
	for (unsigned char byte : randomBytes)
	{
		accessKey.push_back(alphabet[byte >> 3]);
		accessKey.push_back(alphabet[byte & 0x1F]);
	}
	return accessKey;
}

bool GenerateAccessKeyFile()
{
	string accessKey = GenerateAccessKey();
	if (accessKey.empty())
	{
		return false;
	}

	filesystem::path keyPath = GetAccessKeyPath();
	ofstream keyFile(keyPath, ios::trunc);
	if (!keyFile)
	{
		return false;
	}
	keyFile << accessKey << '\n';
	return keyFile.good();
}

string LoadExpectedAccessKey()
{
	filesystem::path keyPath = GetAccessKeyPath();
	ifstream keyFile(keyPath);
	string accessKey;
	if (keyFile && getline(keyFile, accessKey))
	{
		return TrimString(accessKey);
	}

	return "TEAM073-2026";
}

bool ValidateAccessKey()
{
	constexpr int maxAttempts = 3;
	const string expectedKey = LoadExpectedAccessKey();

	SetConsoleTitleA("Nugz Support Tool - Access");
	cout << "\n NUGZ SUPPORT TOOL\n";
	cout << " Enter access key to continue.\n";

	for (int attempt = 1; attempt <= maxAttempts; ++attempt)
	{
		cout << "\n Access key: ";
		string enteredKey;

		while (true)
		{
			int key = _getch();
			if (key == '\r')
			{
				break;
			}
			if (key == '\b')
			{
				if (!enteredKey.empty())
				{
					enteredKey.pop_back();
					cout << "\b \b";
				}
				continue;
			}
			if (key >= 32 && key <= 126)
			{
				enteredKey.push_back(static_cast<char>(key));
				cout << '*';
			}
		}

		cout << endl;
		if (!expectedKey.empty() && enteredKey == expectedKey)
		{
			SColor(10);
			cout << " Access granted." << endl;
			RColor();
			Sleep(500);
			return true;
		}

		SColor(12);
		cout << " Invalid access key. Attempts remaining: " << maxAttempts - attempt << endl;
		RColor();
	}

	SColor(12);
	cout << " Access denied." << endl;
	RColor();
	Sleep(1200);
	return false;
}
}

void RunCheckSystem()
{
	SColor(8);
	cout << " ==== System Information ====\n"; RColor();
	PrintWindowsVersionInfo();
	PrintCPUNameWMI();
	PrintGPUName();
	PrintMotherboardInfo();
	PrintRAMAmount();
	PrintNetworkAdapter();
	SColor(8);
	cout << " \n==== General System Checks ====\n"; RColor();
	DetectHyperV();
	CheckMemoryIntegrity();
	CheckVulnerableDriverBlocklist();
	CheckUACStatus();
	CheckVCRedistInstalled();
	DetectDirectX();
	PrintDotNetVersion();
	DetectVirtualMachine();
	CheckWindowsUpdateStatus();
	SColor(8);
	cout << " \n==== Security Checks ====\n"; RColor();
	CheckWindowsDefenderStatus();
	CheckWindowsFirewallStatus();
	DetectAntivirus();
	DetectAntiCheat();
	SColor(8);
	cout << " \n==== BIOS Checks ====\n"; RColor();
	DetectFastStartup();
	DetectSecureBootWMI();
	DetectVirtualizationWmi();
	DetectBiosBootModeWmi();
	DetectTpmWmi();
	DetectVbsWmi();
	cout << endl;
	SColor(8);
	cout << " Press [S] to save output, [E] to export HWID serials, any other key to return...";
	RColor();
	int key = _getch();
	if (key == 'e' || key == 'E')
	{
		if (SaveHWIDSerialsToFile("hwid_serials.txt"))
		{
			SColor(10);
			cout << endl << " Exported to hwid_serials.txt!" << endl;
			RColor();
		}
		else
		{
			SColor(12);
			cout << endl << " Failed to export hwid_serials.txt" << endl;
			RColor();
		}
		Sleep(1200);
	}
	else if (key == 's' || key == 'S')
	{
		if (SaveConsoleOutputToFile("output.txt"))
		{
			SColor(10);
			cout << endl << " Saved to output.txt!" << endl;
		}
		else
		{
			SColor(12);
			cout << endl << " Failed to save output.txt" << endl;
		}
		RColor();
		Sleep(1000);
	}
}

void RunHWIDChecker()
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO originalInfo{};
	bool hasOriginalSize = GetConsoleScreenBufferInfo(hConsole, &originalInfo) != 0;

	if (hasOriginalSize)
	{
		SHORT originalWidth = static_cast<SHORT>(originalInfo.srWindow.Right - originalInfo.srWindow.Left + 1);
		SHORT originalHeight = static_cast<SHORT>(originalInfo.srWindow.Bottom - originalInfo.srWindow.Top + 1);

		SHORT targetWidth = max<SHORT>(originalWidth, 165);
		SHORT targetHeight = max<SHORT>(originalHeight, 58);
		string modeCommand = "mode con: cols=" + to_string(targetWidth) + " lines=" + to_string(targetHeight);
		system(modeCommand.c_str());

		CONSOLE_SCREEN_BUFFER_INFO hwidInfo{};
		if (GetConsoleScreenBufferInfo(hConsole, &hwidInfo))
		{
			COORD scrollBufferSize{};
			scrollBufferSize.X = hwidInfo.dwSize.X;
			scrollBufferSize.Y = max<SHORT>(hwidInfo.dwSize.Y, 1200);
			SetConsoleScreenBufferSize(hConsole, scrollBufferSize);
		}
	}

	SColor(8);
	PrintHWIDCheckerReport();

	cout << endl;
	SColor(8);
	cout << " Press [E] to export HWID serials, any other key to return...";
	RColor();

	int key = _getch();
	if (key == 'e' || key == 'E')
	{
		if (SaveHWIDSerialsToFile("hwid_serials.txt"))
		{
			SColor(10);
			cout << endl << " Exported to hwid_serials.txt!" << endl;
			RColor();
		}
		else
		{
			SColor(12);
			cout << endl << " Failed to export hwid_serials.txt" << endl;
			RColor();
		}
		Sleep(1200);
	}

	if (hasOriginalSize)
	{
		SHORT originalWidth = static_cast<SHORT>(originalInfo.srWindow.Right - originalInfo.srWindow.Left + 1);
		SHORT originalHeight = static_cast<SHORT>(originalInfo.srWindow.Bottom - originalInfo.srWindow.Top + 1);
		string modeCommand = "mode con: cols=" + to_string(originalWidth) + " lines=" + to_string(originalHeight);
		system(modeCommand.c_str());
	}
}

void RunApplyFixes()
{
	// Step 1: Check for DControl
	SColor(8);
	cout << " ==== Apply Fixes ====\n";
	cout << endl;
	RColor();

	cout << " Checking Windows Defender status...\n";
	cout << endl;

	if (IsDefenderDisabled())
	{
		SColor(10);
		cout << " [OK] Windows Defender is already disabled!" << endl;
		RColor();
	}
	else
	{
		// Need DControl to disable Defender
		cout << " Checking for dControl.exe..." << endl;

		if (!DControlExists())
		{
			SColor(8);
			cout << " Downloading dControl.exe..." << endl;
			RColor();

			if (DownloadDControl())
			{
				SColor(10);
				cout << " [OK] dControl.exe downloaded successfully!" << endl;
				RColor();
			}
			else
			{
				SColor(12);
				cout << " [!] Auto-download failed." << endl;
				RColor();
				cout << endl;
				cout << " Opening download page... Please download and place" << endl;
				cout << " dControl.exe in the same folder as this tool." << endl;
				cout << endl;

				ShellExecuteA(NULL, "open", "https://www.sordum.org/9480/defender-control-v2-1/", NULL, NULL, SW_SHOWNORMAL);

				SColor(8);
				cout << " Press any key after placing dControl.exe here...";
				RColor();
				FlushInputBuffer();
				_getch();
				cout << endl << endl;

				if (!DControlExists())
				{
					SColor(12);
					cout << " [!] dControl.exe still not found. Returning to menu." << endl;
					RColor();
					Sleep(2000);
					return;
				}
			}
		}

		SColor(10);
		cout << " [OK] dControl.exe found!" << endl;
		RColor();
		cout << endl;

		SColor(9);
		cout << " Launching DControl..." << endl;
		RColor();

		string dcPath = GetDControlPath();
		ShellExecuteA(NULL, "open", dcPath.c_str(), NULL, NULL, SW_SHOWNORMAL);

		cout << endl;
		SColor(14);
		cout << " ======================================================" << endl;
		cout << "  Please disable Windows Defender ENTIRELY using DControl" << endl;
		cout << "  Click 'Disable Windows Defender' in the DControl window" << endl;
		cout << " ======================================================" << endl;
		RColor();
		cout << endl;

		SColor(8);
		cout << " Press any key AFTER you have disabled Defender...";
		RColor();
		FlushInputBuffer();
		_getch();
	}
	cout << endl << endl;

	// Step 3: Apply all fixes
	SColor(8);
	cout << " ==== Applying Fixes ====" << endl;
	cout << endl;
	RColor();

	FixDisableFastStartup();
	FixDisableCoreIsolation();
	FixDisableVBS();
	FixHighPerformancePlan();
	FixFlushDNS();
	FixResetWinsock();
	FixClearTemp();
	FixDisableDriverBlocklist();
	FixDisableDefenderService();
	FixDisableFirewall();
	FixDisableGameDVR();
	FixDisableSysMain();
	FixDisableMouseAcceleration();
	FixDisableSmartScreen();
	FixDisableErrorReporting();
	FixDisableTelemetry();
	FixDisableASLR();
	FixDisableCFG();

	// Step 4: Check for Hyper-V
	cout << endl;
	SColor(8);
	cout << " Checking for Hyper-V..." << endl;
	RColor();

	if (IsHyperVEnabled()) {
		SColor(14);
		cout << " [!] Hyper-V is currently enabled." << endl;
		RColor();
		SColor(8);
		cout << " Disable Hyper-V? (Y/N): ";
		RColor();
		FlushInputBuffer();
		int hvChoice = _getch();
		cout << (char)hvChoice << endl;
		if (hvChoice == 'y' || hvChoice == 'Y') {
			system("bcdedit /set hypervisorlaunchtype off >nul 2>&1");
			system("DISM /Online /Disable-Feature /FeatureName:Microsoft-Hyper-V-All /NoRestart >nul 2>&1");
			SColor(10); cout << "  [OK] "; RColor(); cout << "Hyper-V disabled (requires restart)" << endl;
		}
		else {
			SColor(8); cout << "  [--] "; RColor(); cout << "Hyper-V left enabled" << endl;
		}
	}
	else {
		SColor(14);
		cout << " [!] Hyper-V is currently disabled." << endl;
		RColor();
		SColor(8);
		cout << " Enable Hyper-V? (Y/N): ";
		RColor();
		FlushInputBuffer();
		int hvChoice = _getch();
		cout << (char)hvChoice << endl;
		if (hvChoice == 'y' || hvChoice == 'Y') {
			system("bcdedit /set hypervisorlaunchtype auto >nul 2>&1");
			system("DISM /Online /Enable-Feature /FeatureName:Microsoft-Hyper-V-All /NoRestart >nul 2>&1");
			SColor(10); cout << "  [OK] "; RColor(); cout << "Hyper-V enabled (requires restart)" << endl;
		}
		else {
			SColor(8); cout << "  [--] "; RColor(); cout << "Hyper-V left disabled" << endl;
		}
	}

	// Step 5: Check for Riot Vanguard and disable if found
	cout << endl;
	SColor(8);
	cout << " Checking for Riot Vanguard..." << endl;
	RColor();

	SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	bool vanguardFound = false;
	if (scm) {
		SC_HANDLE vgc = OpenServiceA(scm, "vgc", SERVICE_QUERY_STATUS);
		if (vgc) {
			vanguardFound = true;
			CloseServiceHandle(vgc);
		}
		CloseServiceHandle(scm);
	}

	if (vanguardFound) {
		SColor(12);
		cout << " [!] Riot Vanguard detected - disabling..." << endl;
		RColor();
		system("sc config vgc start= disabled >nul 2>&1");
		system("sc config vgk start= disabled >nul 2>&1");
		system("net stop vgc >nul 2>&1");
		system("net stop vgk >nul 2>&1");
		system("taskkill /IM vgtray.exe /F >nul 2>&1");
		SColor(10); cout << "  [OK] "; RColor(); cout << "Riot Vanguard disabled and stopped" << endl;
	}
	else {
		SColor(10); cout << "  [OK] "; RColor(); cout << "Riot Vanguard not found" << endl;
	}

	cout << endl;
	SColor(10);
	cout << " All fixes applied!" << endl;
	RColor();

	SColor(14);
	cout << " Note: Some changes require a restart to take effect." << endl;
	RColor();

	cout << endl;
	SColor(8);
	cout << " Press any key to return to menu...";
	RColor();
	FlushInputBuffer();
	_getch();
}

void OpenURL(const char* url) {
	ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void RunInstalls()
{
	while (true)
	{
		ClearScreen();

		SColor(8);
		cout << endl << "  ==== Installs ====" << endl;
		cout << endl;
		cout << "  Select a program to download:" << endl;
		cout << endl;
		RColor();

		SColor(9); cout << "   [1] "; RColor(); cout << "C++ Redistributable (2015-2022)" << endl;
		SColor(9); cout << "   [2] "; RColor(); cout << "DirectX Runtime" << endl;
		SColor(9); cout << "   [3] "; RColor(); cout << "Cloudflare WARP (VPN)" << endl;
		SColor(9); cout << "   [4] "; RColor(); cout << "DS4Windows (Controller)" << endl;
		SColor(9); cout << "   [5] "; RColor(); cout << "DControl (Defender Control)" << endl;
		cout << endl;
		SColor(9); cout << "   [0] "; RColor(); cout << "Back to Menu" << endl;

		cout << endl;
		SColor(8); cout << "  > "; RColor();

		int choice = _getch();

		if (choice == '1')
		{
			OpenURL("https://aka.ms/vs/17/release/vc_redist.x64.exe");
			SColor(10); cout << endl << "  Opening C++ Redistributable download..." << endl; RColor();
			Sleep(1500);
		}
		else if (choice == '2')
		{
			OpenURL("https://www.microsoft.com/en-us/download/details.aspx?id=35");
			SColor(10); cout << endl << "  Opening DirectX Runtime download page..." << endl; RColor();
			Sleep(1500);
		}
		else if (choice == '3')
		{
			OpenURL("https://one.one.one.one/");
			SColor(10); cout << endl << "  Opening Cloudflare WARP download page..." << endl; RColor();
			Sleep(1500);
		}
		else if (choice == '4')
		{
			OpenURL("https://ds4-windows.com/download/ryochan7-ds4windows/");
			SColor(10); cout << endl << "  Opening DS4Windows download page..." << endl; RColor();
			Sleep(1500);
		}
		else if (choice == '5')
		{
			OpenURL("https://www.sordum.org/9480/defender-control-v2-1/");
			SColor(10); cout << endl << "  Opening DControl download page..." << endl; RColor();
			Sleep(1500);
		}
		else if (choice == '0')
		{
			break;
		}
	}
}

int main(int argc, char* argv[])
{
	if (argc > 1 && string(argv[1]) == "--generate-key")
	{
		if (GenerateAccessKeyFile())
		{
			cout << "Generated a new access key in " << GetAccessKeyPath().string() << "." << endl;
			return 0;
		}

		cerr << "Failed to generate access key." << endl;
		return 1;
	}

	if (!ValidateAccessKey())
	{
		return 1;
	}

	// COM einmalig initialisieren (muss vor allen WMI-Abfragen geschehen)
	CoInitializeEx(0, COINIT_MULTITHREADED);
	CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	int result = RunDashboard();
	CoUninitialize();
	return result;
}
