#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <rpc.h>           // 用于 UuidCreate
#pragma comment(lib, "rpcrt4.lib")  // 链接 UUID 库

// 默认扩展名（内置保底）
const wchar_t* DEFAULT_EXTENSIONS[] = {
	L".mp4", L".mkv", L".avi", L".mov", L".wmv",
	L".flv", L".webm", L".m4v", L".mpg", L".mpeg",
	L".ts", L".m2ts", L".3gp", L".rmvb"
};
const int DEFAULT_EXT_COUNT = sizeof(DEFAULT_EXTENSIONS) / sizeof(DEFAULT_EXTENSIONS[0]);

// 全局变量
std::vector<std::wstring> g_extensions;
std::wstring g_gridPlayerPath;
// 最大文件数阈值（默认50）
int g_maxFileCount = 50;

// 命令行长度阈值（超过此数量改用播放列表）
const int MAX_CMDLINE_FILES = 3;

// ------------------------- 工具函数 -------------------------

std::wstring GetExeDirectory()
{
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::wstring exePath = path;
	size_t pos = exePath.find_last_of(L'\\');
	if (pos != std::wstring::npos)
		return exePath.substr(0, pos + 1);
	return L"";
}

void WriteLog(const std::wstring& text)
{
	wchar_t tempPath[MAX_PATH];
	if (GetTempPathW(MAX_PATH, tempPath) == 0)
		return;
	std::wstring logFile = std::wstring(tempPath) + L"GridPlayerLauncher.log";
	HANDLE hFile = CreateFileW(logFile.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;
	SYSTEMTIME st;
	GetLocalTime(&st);
	wchar_t buf[128];
	wsprintfW(buf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s\r\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, text.c_str());
	DWORD written;
	WriteFile(hFile, buf, wcslen(buf) * sizeof(wchar_t), &written, NULL);
	CloseHandle(hFile);
}

// ------------------------- 配置文件读写 -------------------------

void SaveConfig(const std::wstring& gridPlayerPath, const std::vector<std::wstring>& extensions)
{
	std::wstring configPath = GetExeDirectory() + L"GridPlayerLauncher.ini";
	std::ofstream file(configPath);
	if (!file.is_open())
	{
		WriteLog(L"SaveConfig: 无法写入配置文件");
		return;
	}

	if (!gridPlayerPath.empty())
	{
		std::string pathStr(gridPlayerPath.begin(), gridPlayerPath.end());
		file << "GridPlayerPath=" << pathStr << std::endl;
		file << std::endl;
	}

	// 写入最大文件数（默认值）
	file << "MaxFileCount=" << g_maxFileCount << std::endl;
	file << std::endl;

	for (const auto& ext : extensions)
	{
		std::string extStr(ext.begin(), ext.end());
		file << extStr << std::endl;
	}

	file.flush();
	file.close();
	WriteLog(L"SaveConfig: 成功写入配置");
}

void LoadConfig()
{
	g_extensions.clear();
	g_gridPlayerPath.clear();

	std::wstring configPath = GetExeDirectory() + L"GridPlayerLauncher.ini";

	if (GetFileAttributesW(configPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		WriteLog(L"配置文件不存在，创建默认配置...");
		for (int i = 0; i < DEFAULT_EXT_COUNT; ++i)
			g_extensions.push_back(DEFAULT_EXTENSIONS[i]);
		SaveConfig(L"", g_extensions);
		return;
	}

	std::ifstream file(configPath);
	if (!file.is_open())
	{
		WriteLog(L"无法打开配置文件，使用默认扩展名");
		for (int i = 0; i < DEFAULT_EXT_COUNT; ++i)
			g_extensions.push_back(DEFAULT_EXTENSIONS[i]);
		return;
	}

	std::string line;
	while (std::getline(file, line))
	{
		line.erase(0, line.find_first_not_of(" \t\r\n"));
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		if (line.empty())
			continue;
		if (line[0] == ';' || line[0] == '#')
			continue;

		if (line.find("GridPlayerPath=") == 0)
		{
			std::string value = line.substr(15);
			value.erase(0, value.find_first_not_of(" \t\r\n"));
			value.erase(value.find_last_not_of(" \t\r\n") + 1);
			if (!value.empty())
			{
				std::wstring wpath(value.begin(), value.end());
				g_gridPlayerPath = wpath;
				WriteLog(L"从配置读取 GridPlayerPath: " + g_gridPlayerPath);
			}
			continue;
		}

		// 读取最大文件数
		if (line.find("MaxFileCount=") == 0)
		{
			std::string value = line.substr(13);
			value.erase(0, value.find_first_not_of(" \t\r\n"));
			value.erase(value.find_last_not_of(" \t\r\n") + 1);
			if (!value.empty())
			{
				int val = std::stoi(value);
				if (val > 0)
				{
					g_maxFileCount = val;
					WriteLog(L"从配置读取 MaxFileCount: " + std::to_wstring(g_maxFileCount));
				}
			}
			continue;
		}

		if (line[0] != '.')
			line = "." + line;
		std::transform(line.begin(), line.end(), line.begin(), ::tolower);
		std::wstring wext(line.begin(), line.end());
		g_extensions.push_back(wext);
	}
	file.close();

	if (g_extensions.empty())
	{
		WriteLog(L"配置无有效扩展名，使用默认");
		for (int i = 0; i < DEFAULT_EXT_COUNT; ++i)
			g_extensions.push_back(DEFAULT_EXTENSIONS[i]);
	}
	else
	{
		WriteLog(L"从配置加载了 " + std::to_wstring(g_extensions.size()) + L" 个扩展名");
	}
}

// ------------------------- 获取 GridPlayer 路径（含弹窗） -------------------------

std::wstring GetGridPlayerPath()
{
	std::wstring exeDir = GetExeDirectory();

	if (!g_gridPlayerPath.empty() && GetFileAttributesW(g_gridPlayerPath.c_str()) != INVALID_FILE_ATTRIBUTES)
		return g_gridPlayerPath;

	std::wstring localPath = exeDir + L"GridPlayer.exe";
	if (GetFileAttributesW(localPath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		g_gridPlayerPath = localPath;
		SaveConfig(g_gridPlayerPath, g_extensions);
		return localPath;
	}

	WriteLog(L"GetGridPlayerPath: 未找到，弹窗让用户选择");
	wchar_t filename[MAX_PATH] = { 0 };
	OPENFILENAMEW ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = L"GridPlayer.exe\0GridPlayer.exe\0All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = L"请选择 GridPlayer.exe";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

	if (!GetOpenFileNameW(&ofn))
	{
		WriteLog(L"用户取消选择");
		return L"";
	}

	std::wstring selected = filename;
	WriteLog(L"用户选择了: " + selected);
	g_gridPlayerPath = selected;
	SaveConfig(g_gridPlayerPath, g_extensions);
	return selected;
}

// ------------------------- 生成 .reg 文件（UTF-16 LE with BOM） -------------------------

bool WriteRegFile(const std::wstring& filePath, const std::wstring& content)
{
	HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	BYTE bom[] = { 0xFF, 0xFE };
	DWORD written;
	if (!WriteFile(hFile, bom, 2, &written, NULL) || written != 2)
	{
		CloseHandle(hFile);
		return false;
	}

	if (!WriteFile(hFile, content.c_str(), content.size() * sizeof(wchar_t), &written, NULL) ||
		written != content.size() * sizeof(wchar_t))
	{
		CloseHandle(hFile);
		return false;
	}

	FlushFileBuffers(hFile);
	CloseHandle(hFile);
	return true;
}

bool GenerateRegFiles(const std::wstring& gridPath)
{
	if (gridPath.empty() || GetFileAttributesW(gridPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBoxW(NULL, L"无效的 GridPlayer.exe 路径，无法生成注册表。", L"错误", MB_OK | MB_ICONERROR);
		return false;
	}

	std::wstring exeDir = GetExeDirectory();
	std::wstring launcherPath = exeDir + L"GridPlayerLauncher.exe";

	std::wstring launcherEsc = launcherPath;
	std::wstring gridEsc = gridPath;
	for (size_t i = 0; i < launcherEsc.size(); ++i)
		if (launcherEsc[i] == L'\\') launcherEsc.insert(i++, L"\\");
	for (size_t i = 0; i < gridEsc.size(); ++i)
		if (gridEsc[i] == L'\\') gridEsc.insert(i++, L"\\");

	std::wstring addContent =
		L"Windows Registry Editor Version 5.00\n\n"
		L"[HKEY_CLASSES_ROOT\\Folder\\shell\\OpenWithGridPlayer]\n"
		L"@=\"用 GridPlayer 播放此文件夹\"\n"
		L"\"Icon\"=\"" + gridEsc + L"\"\n\n"
		L"[HKEY_CLASSES_ROOT\\Folder\\shell\\OpenWithGridPlayer\\command]\n"
		L"@=\"\\\"" + launcherEsc + L"\\\" \\\"%1\\\"\"\n";

	std::wstring delContent =
		L"Windows Registry Editor Version 5.00\n\n"
		L"[-HKEY_CLASSES_ROOT\\Folder\\shell\\OpenWithGridPlayer]\n";

	bool addOk = WriteRegFile(exeDir + L"GridPlayerLauncher添加右键菜单.reg", addContent);
	bool delOk = WriteRegFile(exeDir + L"GridPlayerLauncher删除右键菜单.reg", delContent);

	WriteLog(L"生成注册表文件: 添加=" + std::wstring(addOk ? L"成功" : L"失败") +
		L", 删除=" + std::wstring(delOk ? L"成功" : L"失败"));
	return addOk && delOk;
}

// ------------------------- 判断是否为视频文件 -------------------------

bool IsVideoFile(const std::wstring& filename)
{
	std::wstring ext = filename;
	size_t pos = ext.find_last_of(L'.');
	if (pos == std::wstring::npos)
		return false;
	ext = ext.substr(pos);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

	for (const auto& e : g_extensions)
		if (ext == e)
			return true;
	return false;
}

// ------------------------- UTF-8 转换辅助函数 -------------------------

std::string WideToUTF8(const std::wstring& wstr)
{
	if (wstr.empty()) return "";
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	if (len <= 0) return "";
	std::vector<char> buf(len);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buf.data(), len, NULL, NULL);
	return std::string(buf.data(), len - 1);
}

// ------------------------- 生成 UUID 字符串 -------------------------

std::string GenerateUUID()
{
	UUID uuid;
	UuidCreate(&uuid);
	RPC_WSTR rpcStr;
	UuidToStringW(&uuid, &rpcStr);
	std::wstring wstr((wchar_t*)rpcStr);
	RpcStringFreeW(&rpcStr);
	return WideToUTF8(wstr);
}

// ------------------------- 生成播放列表内容（标准 .gpl 格式） -------------------------

std::string GeneratePlaylistContent(const std::vector<std::wstring>& files)
{
	std::string content;
	content += "#GRIDPLAYER\r\n";
	content += "#P:{\"grid_state\": {\"mode\": \"auto_rows\", \"is_fit\": true, \"size\": 0}, \"snapshots\": {}, \"seek_sync_mode\": \"disabled\", \"shuffle_on_load\": false, \"disable_click_pause\": false, \"disable_wheel_seek\": false}\r\n";

	const char* metaTemplate = R"({"id": "%s", "color": "white", "repeat_mode": "dir_shuffle", "is_start_random": false, "rate": 1.0, "aspect_mode": "none", "is_muted": false, "scale": 1.0, "volume": 1.0, "stream_quality": "best", "auto_reload_timer_min": 0, "audio_track_id": 1, "video_track_id": 0, "audio_channel_mode": "unset"})";

	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string uuid = GenerateUUID();
		std::string meta = metaTemplate;
		size_t pos = meta.find("%s");
		if (pos != std::string::npos)
			meta.replace(pos, 2, uuid);
		content += "#V" + std::to_string(i) + ":" + meta + "\r\n";
	}

	// 文件列表，最后一项不加换行
	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string pathUtf8 = WideToUTF8(files[i]);
		content += pathUtf8;
		if (i != files.size() - 1)
			content += "\r\n";
	}

	return content;
}
// ------------------------- 启动 GridPlayer（支持命令行或播放列表） -------------------------

bool LaunchGridPlayer(const std::wstring& gridPath,
	const std::wstring& folderPath,
	const std::vector<std::wstring>& files)
{
	if (files.empty())
		return false;

	// 如果文件数超过阈值，使用播放列表
	if (files.size() > MAX_CMDLINE_FILES)
	{
		WriteLog(L"文件数 " + std::to_wstring(files.size()) + L" 超过阈值，改用播放列表");

		// 1. 提取文件夹名称（最后一级目录名）
		std::wstring dirName;
		size_t pos = folderPath.find_last_of(L'\\');
		if (pos != std::wstring::npos && pos != folderPath.length() - 1) {
			dirName = folderPath.substr(pos + 1);
		}
		else if (pos == folderPath.length() - 1) {
			// 路径以反斜杠结尾，去掉后再取
			std::wstring trimmed = folderPath.substr(0, folderPath.length() - 1);
			size_t p2 = trimmed.find_last_of(L'\\');
			if (p2 != std::wstring::npos)
				dirName = trimmed.substr(p2 + 1);
			else
				dirName = trimmed; // 如果只剩盘符，如 "E:\"
		}
		else {
			// 没有反斜杠，可能是根目录或相对路径
			dirName = folderPath;
		}

		// 如果目录名为空（比如根目录），使用默认名
		if (dirName.empty())
			dirName = L"播放列表";

		// 2. 构造播放列表文件路径：放在目标文件夹内
		std::wstring playlistFile = folderPath + L"\\" + dirName + L".gpls";
		WriteLog(L"生成播放列表: " + playlistFile);

		// 3. 生成内容（UTF-8 std::string）
		std::string content = GeneratePlaylistContent(files);

		// 4. 用 CreateFileW 写入二进制（无 BOM）
		HANDLE hFile = CreateFileW(playlistFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			WriteLog(L"无法创建播放列表文件");
			return false;
		}
		DWORD written;
		BOOL writeOk = WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, NULL);
		CloseHandle(hFile);
		if (!writeOk || written != content.size()) {
			WriteLog(L"写入播放列表失败");
			return false;
		}
		WriteLog(L"播放列表写入成功，共 " + std::to_wstring(files.size()) + L" 个文件");

		// 5. 启动 GridPlayer（用 ShellExecute 更可靠）
		std::wstring cmdLine = L"\"" + playlistFile + L"\"";
		INT_PTR result = (INT_PTR)ShellExecuteW(NULL, L"open", gridPath.c_str(), cmdLine.c_str(), NULL, SW_SHOWNORMAL);
		if (result > 32) {
			WriteLog(L"GridPlayer 启动成功（播放列表）");
			return true;
		}
		else {
			WriteLog(L"ShellExecute 启动失败，错误码: " + std::to_wstring(GetLastError()));
			// 备用 CreateProcess
			std::wstring fullCmdLine = L"\"" + gridPath + L"\" " + cmdLine;
			STARTUPINFOW si = { sizeof(si) };
			PROCESS_INFORMATION pi;
			BOOL success = CreateProcessW(NULL, &fullCmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
			if (success) {
				WriteLog(L"CreateProcess 启动成功（播放列表）");
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				return true;
			}
			else {
				WriteLog(L"CreateProcess 启动失败，错误码: " + std::to_wstring(GetLastError()));
				return false;
			}
		}
	}
	else
	{
		// 文件数较少，使用命令行传参（原有逻辑不变）
		WriteLog(L"文件数 " + std::to_wstring(files.size()) + L" 未超阈值，使用命令行传参");
		std::wstring cmdLine;
		for (size_t i = 0; i < files.size(); ++i) {
			if (i > 0) cmdLine += L" ";
			cmdLine += L"\"";
			cmdLine += files[i];
			cmdLine += L"\"";
		}
		std::wstring fullCmdLine = L"\"" + gridPath + L"\" " + cmdLine;
		WriteLog(L"启动命令: " + fullCmdLine);

		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi;
		BOOL success = CreateProcessW(NULL, &fullCmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		if (success) {
			WriteLog(L"GridPlayer 启动成功（命令行）");
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			return true;
		}
		else {
			DWORD err = GetLastError();
			WriteLog(L"启动失败，错误码: " + std::to_wstring(err));
			ShellExecuteW(NULL, L"open", gridPath.c_str(), cmdLine.c_str(), NULL, SW_SHOWNORMAL);
			return false;
		}
	}
}

// ------------------------- 主逻辑 -------------------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
	// 1. 加载配置
	LoadConfig();

	// 2. 获取有效的 GridPlayer.exe 路径
	std::wstring gridPath = GetGridPlayerPath();
	if (gridPath.empty())
	{
		MessageBoxW(NULL, L"未选择 GridPlayer.exe，程序退出。", L"提示", MB_OK);
		return 0;
	}

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	WriteLog(L"=== 启动 ===");

	// ========== 无参数：生成注册表文件 ==========
	if (argc < 2)
	{
		WriteLog(L"无参数，进入生成注册表模式");
		if (GenerateRegFiles(gridPath))
		{
			MessageBoxW(NULL,
				L"注册表文件已生成到程序目录：\n\n"
				L"GridPlayerLauncher添加右键菜单.reg\n"
				L"GridPlayerLauncher删除右键菜单.reg\n\n"
				L"请以管理员身份运行“GridPlayerLauncher添加右键菜单.reg”导入。",
				L"成功", MB_OK | MB_ICONINFORMATION);
		}
		LocalFree(argv);
		return 0;
	}

	// ========== 有参数：处理文件夹（右键菜单调用） ==========
	std::wstring folderPath = argv[1];
	if (!folderPath.empty() && folderPath.front() == L'\"' && folderPath.back() == L'\"')
		folderPath = folderPath.substr(1, folderPath.length() - 2);

	WriteLog(std::wstring(L"文件夹路径: ") + folderPath);

	DWORD attrs = GetFileAttributesW(folderPath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
	{
		WriteLog(L"错误: 不是有效的文件夹路径");
		MessageBoxW(NULL, L"传入参数不是有效的文件夹路径。", L"GridPlayer Launcher", MB_OK | MB_ICONERROR);
		LocalFree(argv);
		return 1;
	}

	// 扫描文件夹，收集所有视频文件
	std::vector<std::wstring> videoFiles;
	std::wstring searchPath = folderPath + L"\\*.*";
	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			std::wstring filename = findData.cFileName;
			if (IsVideoFile(filename))
			{
				videoFiles.push_back(folderPath + L"\\" + filename);
				WriteLog(std::wstring(L"  找到视频: ") + filename);
			}
		} while (FindNextFileW(hFind, &findData));
		FindClose(hFind);
	}

	WriteLog(std::wstring(L"共找到 ") + std::to_wstring(videoFiles.size()) + L" 个视频文件");

	// 新增：检查文件数是否超过阈值
	if ((int)videoFiles.size() > g_maxFileCount)
	{
		std::wstring msg = L"该文件夹下有 " + std::to_wstring(videoFiles.size()) +
			L" 个视频文件，超过了当前限制（" + std::to_wstring(g_maxFileCount) + L" 个）。\n\n" +
			L"加载大量文件可能导致 GridPlayer 卡顿或异常。\n\n" +
			L"是否继续？";
		int result = MessageBoxW(NULL, msg.c_str(), L"文件数过多", MB_YESNO | MB_ICONWARNING);
		if (result != IDYES)
		{
			WriteLog(L"用户取消加载大量文件");
			LocalFree(argv);
			return 0;
		}
		WriteLog(L"用户确认继续加载大量文件");
	}

	if (videoFiles.empty())
	{
		MessageBoxW(NULL, L"该文件夹中没有找到支持的视频文件。", L"GridPlayer Launcher", MB_OK | MB_ICONINFORMATION);
		LocalFree(argv);
		return 0;
	}

	// 启动 GridPlayer（自动选择命令行或播放列表）
	bool launchOk = LaunchGridPlayer(gridPath, folderPath, videoFiles);
	if (!launchOk)
	{
		MessageBoxW(NULL, L"启动 GridPlayer 失败，请查看日志。", L"错误", MB_OK | MB_ICONERROR);
	}

	LocalFree(argv);
	WriteLog(L"=== 结束 ===");
	return launchOk ? 0 : 1;
}