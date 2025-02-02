#include <windows.h>
#include <commdlg.h>
#include <strsafe.h>

#include <loader/loader/utils.h>
#include <loader/loader/ini.h>
#include <loader/loader/paths.h>
#include <loader/loader/runtime_helper.h>

#include <nu/autochar.h>
#include <nu/servicebuilder.h>
#include <nu/MediaLibraryInterface2.h>

#include <winamp/gen.h>
#include <winamp/wa_cup.h>

#include "resource.h"
#include "api.h"

#include <vector>

#define APPNAME  "Yar-matey! Playlist Copier"
#define APPNAMEW L"Yar-matey! Playlist Copier"
#define APPVER   "2.0.3"


int PlayListCount = 0;
HANDLE g_hCopyThread = NULL;
UINT my_menu = (UINT)-1;
BOOL g_bSavePlaylist = FALSE,
	 g_bSavem3u8 = TRUE,
	 g_bNumberFiles = TRUE,
	 g_bUsePlaylistTitle = FALSE,
	 g_bIncludeDirectory = FALSE;
RECT g_yar_rect = { -1, -1 };
wchar_t *unicode_title = NULL;

#define MYUPDATER WM_USER+70

// Wasabi based services for localisation support
SETUP_API_LNG_VARS;

#define GET_UNICODE_TITLE()\
		{if(!unicode_title){wchar_t tempt[256] = { 0 };StringCchPrintf(tempt,\
		ARRAYSIZE(tempt), WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME), TEXT(APPVER));\
		unicode_title = plugin.memmgr->sysDupStr(tempt); }}

void config();
void quit();
int init();

void __cdecl MessageProc(HWND hWnd, const UINT uMsg, const
						 WPARAM wParam, const LPARAM lParam);

winampGeneralPurposePlugin plugin =
{
	GPPHDR_VER_WACUP, (char*)APPNAME " v" APPVER,
	init, config, quit, GEN_INIT_WACUP_HAS_MESSAGES
};

int config_open = 0;
void config(){
	if(!config_open){
	wchar_t message[512] = { 0 };
		config_open = 1;
		GET_UNICODE_TITLE();
		StringCchPrintf(message, ARRAYSIZE(message), WASABI_API_LNGSTRINGW(IDS_ABOUT_MESSAGE),
						unicode_title, WACUP_Author(), WACUP_Copyright(), TEXT(__DATE__));
		MessageBox(0, message, unicode_title, MB_SYSTEMMODAL | MB_ICONINFORMATION);
		config_open = 0;
	}
	else{
		SetActiveWindow(FindWindow(WC_DIALOG, (wchar_t*)plugin.description));
	}
}

typedef struct
{
	HWND hdlg;
	std::vector<wchar_t>* queue;
} FileCountParam;

DWORD WINAPI FileCountThread(LPVOID lpParameter){
FileCountParam *param=(FileCountParam*)lpParameter;
	if (param) {
		size_t fsize = 0;

		if (!param->queue) {
			PlayListCount = GetPlaylistLength();

			for (int x = 0; x < PlayListCount; x++) {
				const size_t size = GetFileSizeByPath(GetPlaylistItemFile(x, NULL));
				if (size && (size != INVALID_FILE_SIZE))
				{
					fsize += (size / 1024);
				}

				if (PlayListCount > 10 && (x % (PlayListCount / 10) == 0)) {
					if (IsWindow(param->hdlg)) PostMessage(param->hdlg, MYUPDATER, fsize, TRUE);
					else break;
				}
			}
		}
		else {
			PlayListCount = 0;

			wchar_t *files = param->queue->data(), *file = files;
			while (file && *file) {
				const size_t size = GetFileSizeByPath(file);
				if (size && (size != INVALID_FILE_SIZE))
				{
					fsize += (size / 1024);
					++PlayListCount;
				}
				file += (wcslen(file) + 1);
			}
		}

		if (IsWindow(param->hdlg)) {
			wchar_t temp[64] = { 0 };
			if (PlayListCount > 1000) {
				StringCchPrintf(temp, ARRAYSIZE(temp), L"%d,%03d", PlayListCount / 1000, PlayListCount % 1000);
			}
			else {
				I2WStr(PlayListCount, temp, ARRAYSIZE(temp));
			}

			SetDlgItemText(param->hdlg, IDC_FILES, temp);
			PostMessage(param->hdlg, MYUPDATER, fsize, FALSE);
		}
		plugin.memmgr->sysFree(lpParameter);
	}
	return 0;
}

void FileHookInitDialog(HWND hdlg, LPARAM lParam){

	FileCountParam* param = (FileCountParam*)plugin.memmgr->sysMalloc(sizeof(FileCountParam));
	if (param)
	{
		param->hdlg = hdlg;
		param->queue = ((std::vector<wchar_t>*)lParam);
	}

	if (StartThread(FileCountThread, param, THREAD_PRIORITY_NORMAL, 0, NULL))
	{
		g_bSavePlaylist = GetNativeIniInt(PLUGIN_INI, APPNAMEW, L"SavePlaylist", g_bSavePlaylist);
		g_bSavem3u8 = GetNativeIniInt(PLUGIN_INI, APPNAMEW, L"SaveM3U8", g_bSavem3u8);
		g_bNumberFiles = GetNativeIniInt(PLUGIN_INI, APPNAMEW, L"NumberFilenames", g_bNumberFiles);
		g_bUsePlaylistTitle = GetNativeIniInt(PLUGIN_INI, APPNAMEW, L"UsePlaylistTitle", g_bUsePlaylistTitle);
		g_bIncludeDirectory = GetNativeIniInt(PLUGIN_INI, APPNAMEW, L"IncludeDirectory", g_bIncludeDirectory);

		CheckDlgButton(hdlg, IDC_PLAYLIST, g_bSavePlaylist);
		CheckDlgButton(hdlg, IDC_PLAYLIST2, g_bSavem3u8);
		CheckDlgButton(hdlg, IDC_NUMBER, g_bNumberFiles);
		CheckDlgButton(hdlg, IDC_PLAYLISTTITLE, g_bUsePlaylistTitle);
		CheckDlgButton(hdlg, IDC_INCLUDEDIR, g_bIncludeDirectory);
	}
}

void FileHookDestroy(HWND hdlg){
	g_bSavePlaylist = IsDlgButtonChecked(hdlg, IDC_PLAYLIST);
	g_bSavem3u8 = IsDlgButtonChecked(hdlg, IDC_PLAYLIST2);
	g_bNumberFiles = IsDlgButtonChecked(hdlg, IDC_NUMBER);
	g_bUsePlaylistTitle = IsDlgButtonChecked(hdlg, IDC_PLAYLISTTITLE);
	g_bIncludeDirectory = IsDlgButtonChecked(hdlg, IDC_INCLUDEDIR);

	SaveNativeIniString(PLUGIN_INI, APPNAMEW, L"SavePlaylist", (g_bSavePlaylist ? L"1" : NULL));
	SaveNativeIniString(PLUGIN_INI, APPNAMEW, L"SaveM3U8", (g_bSavem3u8 ? NULL : L"0"));
	SaveNativeIniString(PLUGIN_INI, APPNAMEW, L"NumberFilenames", (g_bNumberFiles ? NULL : L"0"));
	SaveNativeIniString(PLUGIN_INI, APPNAMEW, L"UsePlaylistTitle", (g_bUsePlaylistTitle ? L"1" : NULL));
	SaveNativeIniString(PLUGIN_INI, APPNAMEW, L"IncludeDirectory", (g_bIncludeDirectory ? L"1" : NULL));

	GetWindowRect(GetParent(hdlg), &g_yar_rect);
	SaveNativeIniInt(PLUGIN_INI, APPNAMEW, L"yar_wx", g_yar_rect.left);
	SaveNativeIniInt(PLUGIN_INI, APPNAMEW, L"yar_wy", g_yar_rect.top);
}

UINT_PTR APIENTRY OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam){
	switch(uiMsg)
	{
		case WM_INITDIALOG:
			FileHookInitDialog(hdlg, ((OPENFILENAME*)lParam)->lCustData);
		return FALSE;

		case WM_DESTROY:
			FileHookDestroy(hdlg);
		return FALSE;

		case WM_NOTIFY:
			if( ((LPOFNOTIFY)lParam)->hdr.code == CDN_INITDONE) {
			HWND parent = GetParent(hdlg);

				GetNativeIniIntParam(PLUGIN_INI, APPNAMEW, L"yar_wx", (int *)&g_yar_rect.left);
				GetNativeIniIntParam(PLUGIN_INI, APPNAMEW, L"yar_wy", (int *)&g_yar_rect.top);

				// restore last position as applicable
				POINT pt = { g_yar_rect.left, g_yar_rect.top };
				if (!WindowOffScreen(parent, pt))
				{
					SetWindowPos(parent, HWND_TOP, g_yar_rect.left, g_yar_rect.top,
								 0, 0, SWP_NOSIZE | SWP_NOSENDCHANGING);
				}

				DarkModeSetup(parent);
			}
			else if( ((LPOFNOTIFY)lParam)->hdr.code == CDN_FOLDERCHANGE) {
				DarkModeSetup(GetParent(hdlg));
			}
		return TRUE;

		case MYUPDATER:
		{
			wchar_t buf[128] = { 0 }, temp[128] = { 0 };
			StringCchPrintf(buf, ARRAYSIZE(buf), L"%s%s", (lParam ? L"+" : L""),
							WASABI_API_LNG->FormattedSizeString(temp, ARRAYSIZE(temp), wParam * 1024));
			SetDlgItemText(hdlg, IDC_FILESIZE, buf);
		}
		return TRUE;
	}
	return FALSE;
}

void WriteLine(HANDLE hFile, const wchar_t* text) {
	if (text && *text) {
	DWORD byteswritten = 0;

		// encode as ANSI when saving to a m3u playlist
		// encode as UTF8 when saving to a m3u8 playlist
		AutoChar textA8(text, (!g_bSavem3u8 ? CP_ACP : CP_UTF8));
		(void)WriteFile(hFile, textA8, (DWORD)strlen(textA8), &byteswritten, NULL);
	}
}

typedef struct
{
	wchar_t destination[MAX_PATH];
	std::vector<wchar_t>* queue;
} CopyThreadParam;

DWORD WINAPI CopyThread(LPVOID lp)
{
	CopyThreadParam* param = (CopyThreadParam*)lp;
	if (param) {
		(void)CreateCOM();

		wchar_t title_str[GETFILEINFO_TITLE_LENGTH] = { 0 },
				*files = (param->queue ? param->queue->data() : NULL), *file = files;
		INT_PTR db_error = FALSE;
		SHFILEOPSTRUCT fileOp = { plugin.hwndParent, FO_COPY, NULL, NULL, FOF_MULTIDESTFILES |
													  FOF_NOCONFIRMMKDIR, FALSE, NULL, NULL };
		std::vector<wchar_t> From, To;
		wchar_t numberFormatString[8] = { 0 }; // "%03d_"
		HANDLE hPlsFile = INVALID_HANDLE_VALUE;

		// give things a hint for the worst case so
		// we're less likely to require any resizes
		From.reserve(PlayListCount * MAX_PATH);
		To.reserve(PlayListCount * MAX_PATH);

		if (g_bSavePlaylist) {
			wchar_t playlistFilename[MAX_PATH] = { 0 };
			hPlsFile = CreateFile(CombinePath(playlistFilename, param->destination, (!g_bSavem3u8 ?
								  L"playlist.m3u" : L"playlist.m3u8")), GENERIC_WRITE,
								  FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);

			if (hPlsFile == INVALID_HANDLE_VALUE) {
				GET_UNICODE_TITLE();
				MessageBox(plugin.hwndParent, WASABI_API_LNGSTRINGW(IDS_ERROR_CREATING_PL), unicode_title, MB_OK);
				goto skip_copy;
			}

			// if saving as a m3u8 then we need to add a BOM to the start of the file
			DWORD byteswritten = 0;
			if (g_bSavem3u8) {
				WriteFile(hPlsFile, "\xEF\xBB\xBF", 3, &byteswritten, NULL);
			}
			WriteFile(hPlsFile, "#EXTM3U\r\n", 9, &byteswritten, NULL);
		}

		// set up format for number prepend
		if (g_bNumberFiles) {
			const int numberLength = (
				PlayListCount < 10 ? 1 :
				PlayListCount < 100 ? 2 :
				PlayListCount < 1000 ? 3 :
				PlayListCount < 10000 ? 4 :
				PlayListCount < 100000 ? 5 :
				PlayListCount < 1000000 ? 6 :
				PlayListCount < 10000000 ? 7 :
				PlayListCount < 100000000 ? 8 : 9);
			StringCchPrintf(numberFormatString, ARRAYSIZE(numberFormatString), L"%%0%dd_", numberLength);
		}

		for (int x = 0; x < PlayListCount; x++) {
			wchar_t destFilename[MAX_PATH] = { 0 };
			BOOL valid_entry = FALSE;
			if (!files)	{
				GetPlaylistFile(x, destFilename, ARRAYSIZE(destFilename), &valid_entry, NULL);
			}
			else {
				StringCchCopy(destFilename, ARRAYSIZE(destFilename), file);
				valid_entry = (file && *file);
				file += (wcslen(file) + 1);
			}

			const BOOL is_an_url = IsAnUrl(destFilename);
			size_t size = (valid_entry && !is_an_url ? GetFileSizeByPath(destFilename) : INVALID_FILE_SIZE);
			if (size && (size != INVALID_FILE_SIZE)) { // skip missing / bad files
				const wchar_t *sourcePath = destFilename,
							  *sourceFilename = FindPathFileName(sourcePath);
				size_t destStartOffset = To.size();
				std::wstring_view from_str(destFilename);
				From.insert(From.end(), from_str.begin(), from_str.end());
				From.push_back(0);

				// -- add source file
				std::wstring_view to_str(param->destination);
				To.insert(To.end(), to_str.begin(), to_str.end());

				// include parent dir
				if (g_bIncludeDirectory && sourceFilename > sourcePath + 4) { // is this a problem for network shares?
					wchar_t* parentDirectory = (wchar_t*)sourceFilename - 2;
					while (parentDirectory > sourcePath + 4 && *parentDirectory != '\\') parentDirectory--;
					to_str = (parentDirectory + 1);
					from_str = to_str.substr(0, (sourceFilename - parentDirectory - 1));
					To.insert(To.end(), from_str.begin(), from_str.end());
				}

				// add number
				if (g_bNumberFiles) {
					wchar_t number[16] = { 0 };
					StringCchPrintf(number, ARRAYSIZE(number), numberFormatString, x + 1);
					to_str = number;
					To.insert(To.end(), to_str.begin(), to_str.end());
				}

				// get dest title (either playlist title or filename without path and extension)
				if (g_bUsePlaylistTitle) {
					if (!files) {
						to_str = (wchar_t*)GetPlaylistItemTitle(x);
					}
					else {
						title_str[0] = 0;
						waFormatTitleExtended fmt_title = { (LPCWSTR)destFilename, 1, NULL, destFilename,
																 title_str, ARRAYSIZE(title_str), 0, 0 };
						Handle_IPC_FORMAT_TITLE_EXTENDED(&fmt_title, FALSE, &db_error);
						to_str = title_str;
					}
				}
				else {
					to_str = sourceFilename;
				}

				// add dest title
				if (!to_str.empty())
				{
					const size_t fileStartOffset = To.size();

					To.insert(To.end(), to_str.begin(), to_str.end());

					// Remove bad chars from the file name
					ReplaceInvalidChars(&To[fileStartOffset]);
				}

				// add file extension which is only needed if the title
				// is being used as unlike older builds we do not strip
				// the extension away when using the existing filename.
				if (g_bUsePlaylistTitle) {
					wchar_t ext[128] = { 0 };
					ExtractExtension(sourceFilename, ext, ARRAYSIZE(ext));
					to_str = ext;
					To.push_back(L'.');	// this isn't provided to us
					To.insert(To.end(), to_str.begin(), to_str.end());
				}

				To.push_back(0);

				// -- add playlist entry
				if (g_bSavePlaylist) {
					char temp[32] = { 0 };
					size_t remaining = ARRAYSIZE(temp);
					DWORD byteswritten = 0;
					int length = -1;
					if (files) {
						bool reentrant = false;
						wchar_t length_str[16] = { 0 };
						GetFileMetaData(destFilename, L"length", length_str, ARRAYSIZE(length_str), NULL, &reentrant, NULL);
						if (length_str[0]) {
							length = (WStr2I(length_str) / 1000);
						}
					}

					StringCchPrintfExA(temp, ARRAYSIZE(temp), NULL, &remaining, NULL,
									   "#EXTINF:%d,", (!files ? GetSongLength(x) : length));
					WriteFile(hPlsFile, temp, (DWORD)(ARRAYSIZE(temp) - remaining), &byteswritten, NULL);

					if (!files) {
						WriteLine(hPlsFile, GetPlaylistItemTitle(x));
					}
					else {
						title_str[0] = 0;
						waFormatTitleExtended fmt_title = { (LPCWSTR)destFilename, 1, NULL, destFilename,
																 title_str, ARRAYSIZE(title_str), 0, 0 };
						Handle_IPC_FORMAT_TITLE_EXTENDED(&fmt_title, FALSE, &db_error);
						if (title_str[0]) {
							WriteLine(hPlsFile, title_str);
						}
					}
					WriteFile(hPlsFile, "\r\n", (DWORD)2, &byteswritten, NULL);

					WriteLine(hPlsFile, &To[destStartOffset]);
					WriteFile(hPlsFile, "\r\n", (DWORD)2, &byteswritten, NULL);
				}
			}
			// bad/missing file
			else if (!is_an_url) {
				wchar_t temp[MAX_PATH + 20] = { 0 };
				StringCchPrintf(temp, ARRAYSIZE(temp), WASABI_API_LNGSTRINGW(IDS_ERROR_COPYING), destFilename);
				GET_UNICODE_TITLE();
				MessageBox(plugin.hwndParent, temp, unicode_title, MB_OK);
			}
		}

		// need to ensure its double-null terminated!
		From.push_back(0);
		To.push_back(0);

		if (g_bSavePlaylist) {
			CloseHandle(hPlsFile);
		}

		// now copy the files
		fileOp.pFrom = From.data();
		fileOp.pTo = To.data();
		FileAction(&fileOp);

skip_copy:
		CloseCOM();

		plugin.memmgr->sysFree(param);
	}

	if (g_hCopyThread != NULL) {
		CloseHandle(g_hCopyThread);
		g_hCopyThread = NULL;
	}
	return 0;
}

void SaveThemFiles(std::vector<wchar_t>* queue){
	CopyThreadParam* param = (CopyThreadParam*)plugin.memmgr->sysMalloc(sizeof(CopyThreadParam));
	if (param){
		OPENFILENAME ofn = { 0 };
		wchar_t allFiles[32] = { 0 };
		ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK | OFN_ENABLESIZING;
		ofn.hInstance = WASABI_API_LNG_HINST;
		ofn.lpTemplateName = MAKEINTRESOURCE(IDD_FILEDLG);
		ofn.lpfnHook = &OFNHookProc;
		ofn.lCustData = (LPARAM)queue;
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.lpstrFile = param->destination;
		ofn.nMaxFile = ARRAYSIZE(param->destination);
		ofn.lpstrTitle = WASABI_API_LNGSTRINGW(IDS_SELECT_LOCATION);
		ofn.hwndOwner = GetDialogBoxParent();
		ofn.lpstrFilter = FixFilterString(WASABI_API_LNGSTRINGW_BUF(IDS_ALL_FILES,allFiles,ARRAYSIZE(allFiles)));
		ofn.lpstrInitialDir = ofn.lpstrDefExt = ofn.lpstrCustomFilter = ofn.lpstrFileTitle = NULL;
		WASABI_API_LNGSTRINGW_BUF(IDS_FILENAME_IGNORED,param->destination,ARRAYSIZE(param->destination));

		if(SaveFileName(&ofn)){
			param->destination[ofn.nFileOffset] = 0;
			param->queue = queue;
			g_hCopyThread=StartThread(CopyThread,param,THREAD_PRIORITY_NORMAL,1,NULL);
		}
		else{
			plugin.memmgr->sysFree(param);
		}
	}
}

void insert_playlist_item(std::vector<wchar_t>* queue, LPCWSTR filename)
{
	if (!IsAnUrl(filename))
	{
		std::wstring_view str(filename);
		queue->insert(queue->end(), str.begin(), str.end());
		queue->push_back(0);
	}
}

class SendToPlaylistLoader : public ifc_playlistloadercallback
{
public:
	explicit SendToPlaylistLoader(std::vector<wchar_t>* _queue) : queue(_queue) {}
	void OnFile(LPCWSTR filename, LPCWSTR title,
				int lengthInMS, ifc_plentryinfo *info)
	{
		if (queue && filename && *filename)
		{
			insert_playlist_item(queue, filename);
		}
	}

protected:
	std::vector<wchar_t>* queue;
	RECVS_DISPATCH;
};

#define CBCLASS SendToPlaylistLoader
START_DISPATCH;
VCB(IFC_PLAYLISTLOADERCALLBACK_ONFILE, OnFile)
END_DISPATCH;
#undef CBCLASS

void __cdecl MessageProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
	// we handle both messages so we can get the action when sent via the keyboard
	// shortcut but also copes with using the menu via Winamp's taskbar system menu
	if ((uMsg == WM_SYSCOMMAND || uMsg == WM_COMMAND) && (LOWORD(wParam) == my_menu))
	{
		SaveThemFiles(NULL);
	}
	else if (uMsg == WM_WA_IPC)
	{
		if (lParam == IPC_ON_SENDTO_BUILD)
		{
			const onSendToMenuStuff* stuff = (onSendToMenuStuff*)wParam;
			if (wParam && ML_TYPE_HANDLE_FILE_TYPES(stuff->param1))
			{
				static LPCWSTR copy_files_str = WASABI_API_LNGSTRINGW_DUP(IDS_COPY_FILES_FROM_SENDTO);
				MLAddToSendTo(copy_files_str, stuff->param2, (INT_PTR)MessageProc);
			}
		}
		else if (lParam == IPC_ON_SENDTO_SELECT)
		{
			onSendToMenuStuff* stuff = (onSendToMenuStuff*)wParam;
			if (wParam && stuff->param2 && (stuff->param3 == (INT_PTR)MessageProc))
			{
				std::vector<wchar_t>* queue = new std::vector<wchar_t>();
				if (queue)
				{
					if (stuff->param1 == ML_TYPE_ITEMRECORDLISTW)
					{
						itemRecordListW* p = (itemRecordListW*)stuff->param2;
						for (int x = 0; x < p->Size; x++)
						{
							insert_playlist_item(queue, p->Items[x].filename);
						}
					}
					else if (stuff->param1 == ML_TYPE_ITEMRECORDLIST)
					{
						itemRecordList* p = (itemRecordList*)stuff->param2;
						for (int x = 0; x < p->Size; x++)
						{
							insert_playlist_item(queue, ConvertPathToW(p->Items[x].filename, NULL, 0, CP_ACP));
						}
					}
					else if ((stuff->param1 == ML_TYPE_FILENAMES) ||
						(stuff->param1 == ML_TYPE_STREAMNAMES))
					{
						char* p = (char*)stuff->param2;
						while (p && *p)
						{
							insert_playlist_item(queue, ConvertPathToW(p, NULL, 0, CP_ACP));
							p += (strlen(p) + 1);
						}
					}
					else if ((stuff->param1 == ML_TYPE_FILENAMESW) ||
						(stuff->param1 == ML_TYPE_STREAMNAMESW))
					{
						wchar_t* p = (wchar_t*)stuff->param2;
						while (p && *p)
						{
							insert_playlist_item(queue, p);
							p += (wcslen(p) + 1);
						}
					}
					else if (stuff->param1 == ML_TYPE_PLAYLIST)
					{
						mlPlaylist* playlist = (mlPlaylist*)stuff->param2;
						SendToPlaylistLoader loader(queue);
						plugin.playlistManager->Load(playlist->filename, &loader);
					}
					else if (stuff->param1 == ML_TYPE_PLAYLISTS)
					{
						mlPlaylist** playlists = (mlPlaylist**)stuff->param2;
						SendToPlaylistLoader loader(queue);
						while (playlists && *playlists)
						{
							mlPlaylist* playlist = *playlists;
							plugin.playlistManager->Load(playlist->filename, &loader);
							++playlists;
						}
					}
					else
					{
						delete queue;
						return;
					}

					// make sure we're double-null terminated
					if (!queue->empty())
					{
						queue->push_back(0);

						SaveThemFiles(queue);
					}
					else
					{
						delete queue;
						return;
					}
				}

				stuff->was_us = 1;
			}
		}
		else if ((lParam == IPC_HOOK_OKTOQUIT) && wParam)
		{
			CheckThreadHandleIsValid(&g_hCopyThread);

			// if we're in the process of copying then we
			// have to get the user to cancel the action.
			// due to that we're going to block the close
			wchar_t titleStr[96] = { 0 };
			if (!!g_hCopyThread && TimedMessageBox(plugin.hwndParent, WASABI_API_LNGSTRINGW(IDS_CANCEL_AND_QUIT),
				WASABI_API_LNGSTRINGW_BUF(IDS_CONFIRM_QUIT, titleStr, ARRAYSIZE(titleStr)), MB_ICONWARNING, 10000))
			{
				*((WPARAM*)wParam) = 1;
			}
		}
	}
}

int init(){
ACCEL accel[] = {{FVIRTKEY|FALT,'C',},
					// this is an alternative for mainly Bento/SUI skins
					// where Alt+C is already being used for Skin Settings
					// changed with 1.13 from Alt+R to Alt+U as that isn't
					// going to conflict with WACUP & it's Waveform Seeker
					// which is using Alt+R out of a limited set of chars.
					{FVIRTKEY|FALT,'U',}};
MENUITEMINFO mii = { 0 };

	WASABI_API_START_LANG_DESC(plugin.language, plugin.hDllInstance,
								GenYarLangGUID, IDS_PLUGIN_NAME,
								TEXT(APPVER), &plugin.description);

	my_menu = RegisterCommandID(0);
	accel[0].cmd = (WORD)my_menu;
	accel[1].cmd = (WORD)my_menu;

	// we associate the accelerators to the playlist editor as
	// otherwise the adding will fail due to a quirk in the api
	HACCEL hAccel = CreateAcceleratorTable(accel, 2);
	if (hAccel) plugin.app->app_addAccelerators(GetPlaylistWnd(), &hAccel, 2, TRANSLATE_MODE_GLOBAL);

	#define ID_HELP_HELPTOPICS 40347
	// get a handle of the playlist editor right-click menu
	const HMENU menu = GetSubMenu(GetNativeMenu((WPARAM)-1), 0);
	int adjuster = 3 + (GetMenuItemID(menu,GetMenuItemCount(menu)-3) != ID_HELP_HELPTOPICS);
	mii.cbSize = sizeof(MENUITEMINFOW);
	mii.fMask = MIIM_ID|MIIM_TYPE|MIIM_DATA;
	mii.fType = MFT_SEPARATOR;
	InsertMenuItem(menu,GetMenuItemCount(menu)-adjuster,1,&mii);

	mii.wID = my_menu;
	mii.fType = MFT_STRING;
	mii.dwTypeData = WASABI_API_LNGSTRINGW(IDS_COPY_FILE_MENU);
	InsertMenuItem(menu,GetMenuItemCount(menu)-adjuster-1,1,&mii);
	return GEN_INIT_SUCCESS;
}

void quit(){
	if(g_hCopyThread != NULL){
		WaitForSingleObject(g_hCopyThread,INFINITE);
		if (g_hCopyThread != NULL) {
			CloseHandle(g_hCopyThread);
		}
		g_hCopyThread = NULL;
	}
}

extern "C" __declspec( dllexport ) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin(){
	return &plugin;
}

extern "C" __declspec( dllexport ) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param){
	GET_UNICODE_TITLE();
	if (plugin.language->UninstallSettingsPrompt(reinterpret_cast<const wchar_t*>(unicode_title))) {
		WritePrivateProfileString(APPNAMEW,0,0,GetPaths()->plugin_ini_file);
	}
	return GEN_PLUGIN_UNINSTALL_REBOOT;
}

RUNTIME_LEN_HELPER_HANDLER