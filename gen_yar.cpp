#include <windows.h>
#include <commdlg.h>
#include <strsafe.h>

#include <loader/loader/utils.h>
#include <loader/loader/ini.h>
#include <loader/loader/paths.h>

#include <nu/autochar.h>
#include <nu/servicebuilder.h>
#include <winamp/gen.h>
#include <winamp/wa_ipc.h>

#include <../wacup_version.h>

#include "resource.h"
#include "api.h"

#define APPNAME  "Yar-matey! Playlist Copier"
#define APPNAMEW L"Yar-matey! Playlist Copier"
#define APPVER   "1.13"


int PlayListCount = 0;
UINT my_menu = (UINT)-1;
BOOL g_bSavePlaylist = FALSE,
	 g_bSavem3u8 = TRUE,
	 g_bNumberFiles = TRUE,
	 g_bUsePlaylistTitle = FALSE,
	 g_bIncludeDirectory = FALSE;
wchar_t *title = 0;

#define MYUPDATER WM_USER+70

// Wasabi based services for localisation support
SETUP_API_LNG_VARS;

#define GET_UNICODE_TITLE()\
		{if(!title){wchar_t tempt[256] = { 0 };StringCchPrintf(tempt,\
		ARRAYSIZE(tempt), WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME),\
		TEXT(APPVER)); title = plugin.memmgr->sysDupStr(tempt); }}

void config();
void quit(){}
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
						title, L"Darren Owen aka DrO (2006-" WACUP_COPYRIGHT L")", TEXT(__DATE__));
		MessageBox(0, message, title, MB_SYSTEMMODAL | MB_ICONINFORMATION);
		config_open = 0;
	}
	else{
		SetActiveWindow(FindWindow(L"#32770", (wchar_t*)plugin.description));
	}
}

DWORD WINAPI FileCountThread(LPVOID lpParameter){
HWND hwnd=(HWND)lpParameter;
size_t fsize=0;

	for(int x = 0; x < PlayListCount; x++){
		const size_t size = GetFileSizeByPath(GetPlaylistItemFile(x));
		if (size && (size != INVALID_FILE_SIZE))
		{
			fsize += (size / 1024);
		}

		if(PlayListCount > 10 && (x % (PlayListCount / 10) == 0)){
			if(IsWindow(hwnd)) PostMessage(hwnd, MYUPDATER, fsize, TRUE);
			else return 0;
		}
	}

	if(IsWindow(hwnd)) PostMessage(hwnd, MYUPDATER, fsize, FALSE);
	return 0;
}

void FileHookInitDialog(HWND hdlg){
DWORD dwThreadId = 0;
wchar_t temp[255] = { 0 };

	PlayListCount = GetPlaylistLength();
	if (!CreateThread(NULL, 0, FileCountThread, (LPVOID)hdlg, 0, &dwThreadId))
	{
		return;
	}

	if(PlayListCount>1000){
		StringCchPrintf(temp, ARRAYSIZE(temp), L"%d,%03d", PlayListCount/1000, PlayListCount%1000);
	}
	else{
		StringCchPrintf(temp, ARRAYSIZE(temp), L"%d", PlayListCount);
	}

	SetDlgItemText(hdlg, IDC_FILES, temp);

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
}

UINT_PTR APIENTRY OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam){
	switch(uiMsg)
	{
		case WM_INITDIALOG:
			FileHookInitDialog(hdlg);
		return FALSE;

		case WM_DESTROY:
			FileHookDestroy(hdlg);
		return FALSE;

		case WM_NOTIFY:
			if( ((LPOFNOTIFY)lParam)->hdr.code == CDN_INITDONE) {
			RECT rect = { 0 };
				GetWindowRect(GetParent(hdlg), &rect);
				SetWindowPos(GetParent(hdlg), 0, 
							 (GetSystemMetrics(SM_CXSCREEN)-(rect.right-rect.left))/2,
							 (GetSystemMetrics(SM_CYSCREEN)-(rect.bottom-rect.top))/2,
							 0, 0, SWP_NOSIZE);
			}
		return TRUE;

		case MYUPDATER:
		{
			wchar_t buf[255] = { 0 }, temp[128] = { 0 };
			StringCchPrintf(buf, ARRAYSIZE(buf), L"%s%s", (lParam ? L"+" : L""),
							WASABI_API_LNG->FormattedSizeString(temp, ARRAYSIZE(temp), wParam * 1024));
			SetDlgItemText(hdlg, IDC_FILESIZE, buf);
		}
		return TRUE;
	}
	return FALSE;
}

BOOL WriteLine(HANDLE hFile, const wchar_t* text){
DWORD byteswritten = 0;

	// encode as ANSI when saving to a m3u playlist
	// encode as UTF8 when saving to a m3u8 playlist
	char* textA8 = AutoChar(text,(!g_bSavem3u8?CP_ACP:CP_UTF8));
	return WriteFile(hFile, textA8, (DWORD)strlen(textA8), &byteswritten, NULL);
}

// given szFile = "c:\pathname\stuff\application.exe"
// return int lstrlen
//        NameStart = "application.exe"
//        ExtStart  = ".exe"
//
int CrackFilename(wchar_t* szFile, wchar_t** ppNameStart, wchar_t** ppExtStart){
int stringLength = (int)wcslen(szFile);
wchar_t *NameStart = wcsrchr(szFile, '\\');

	if(!NameStart){
		NameStart = szFile;
	}
	else{
		NameStart++;
	}

	if(ppExtStart){
		wchar_t *ExtStart = wcsrchr(NameStart, '.');
		if(!ExtStart) ExtStart = NameStart+stringLength;
		*ppExtStart = ExtStart;
	}

	if(ppNameStart){
		*ppNameStart = NameStart;
	}
	return stringLength;
}

void CopyFiles(wchar_t* szDestLoc, int nFileOffset){
DWORD buffsizeFrom = 0,
	  buffsizeTo = 0;
wchar_t *bigbuffFrom = 0,
		*bigbuffTo = 0,
		*pFrom = 0,
		*pTo = 0,
		numberFormatString[6] = { 0 }; // "%03d_"
HANDLE hPlsFile = INVALID_HANDLE_VALUE;
int x = 0,
	destDirLength = 0,
	numberStringLength = 0;

	if(g_bSavePlaylist){
		SendMessage(plugin.hwndParent, WM_COMMAND, WINAMP_BUTTON4, 0); // must stop to get lengths
		lstrcpyn(&szDestLoc[nFileOffset], (!g_bSavem3u8?L"playlist.m3u":L"playlist.m3u8"), MAX_PATH);
		hPlsFile = CreateFile(szDestLoc, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);

		if(hPlsFile == INVALID_HANDLE_VALUE){
			GET_UNICODE_TITLE();
			MessageBox(plugin.hwndParent, WASABI_API_LNGSTRINGW(IDS_ERROR_CREATING_PL), title, MB_OK);
			return;
		}

		// if saving as a m3u8 then we need to add a BOM to the start of the file
		if(g_bSavem3u8){
		DWORD byteswritten = 0;
			WriteFile(hPlsFile, "\xEF\xBB\xBF", 3, &byteswritten, NULL);
		}

		WriteLine(hPlsFile, L"#EXTM3U\r\n");
	}

	// set up format for number prepend
	if(g_bNumberFiles){
		int numberLength = (
			PlayListCount <       10 ? 1 :
			PlayListCount <      100 ? 2 :
			PlayListCount <     1000 ? 3 :
			PlayListCount <    10000 ? 4 :
			PlayListCount <   100000 ? 5 :
			PlayListCount <  1000000 ? 6 :
			PlayListCount < 10000000 ? 7 :
			PlayListCount <100000000 ? 8 : 9);
		StringCchPrintf(numberFormatString, ARRAYSIZE(numberFormatString), L"%%0%dd_", numberLength);
		numberStringLength = numberLength +1;
	}

	// how big of a buffer ?
	buffsizeFrom = buffsizeTo = 0;
	destDirLength = (int)wcslen(szDestLoc);
	
	for(x = 0; x < PlayListCount; x++){
	wchar_t *sourcePath,
			*sourceFilename,
			*sourceExtension;
	int sourcePathLength,
		destTitleLength,
		destPathLength;

		// determine source length
		sourcePath = (wchar_t *)GetPlaylistItemFile(x);
		sourcePathLength = CrackFilename(sourcePath, &sourceFilename, &sourceExtension);
		buffsizeFrom += sourcePathLength +1; //+1 for separator

		// determine dest length
		if(g_bUsePlaylistTitle){
			const wchar_t *destTitle = GetPlaylistItemTitle(x);
			destTitleLength = (int)wcslen(destTitle);
		}
		else{
			destTitleLength = (int)(sourceExtension-sourceFilename);
		}

		// get parent dir
		// is this a problem for network shares?
		if(g_bIncludeDirectory && (sourceFilename > sourcePath + 4)){
		wchar_t* parentDirectory = sourceFilename-2;
			while(parentDirectory > sourcePath+4 && *parentDirectory!='\\') parentDirectory--;
			destTitleLength += (int)(sourceFilename-parentDirectory);
		}

		destPathLength = destDirLength + destTitleLength + (int)wcslen(sourceExtension);
		buffsizeTo += destPathLength + 1 +(g_bNumberFiles?numberStringLength:0); //+1 for separator, +4 for "003_"
	}

	// allocate From and To buffers
	bigbuffFrom = (wchar_t*)plugin.memmgr->sysMalloc((buffsizeFrom+1)*sizeof(wchar_t)); // +1 for termination
	bigbuffTo = (wchar_t*)plugin.memmgr->sysMalloc((buffsizeTo + 1) * sizeof(wchar_t)); // +1 for termination

	// now build the big strings
	pFrom = bigbuffFrom;
	pTo = bigbuffTo;
	for(x = 0; x < PlayListCount; x++) {
		const wchar_t *szSrcLoc = GetPlaylistItemFile(x);
		size_t size = GetFileSizeByPath(szSrcLoc);
		if (size && (size != INVALID_FILE_SIZE)) { // skip missing / bad files
		wchar_t *destTitle = 0,
				*sourcePath = 0,
				*sourceFilename = 0,
				*sourceExtension = 0,
				*destFilename = 0;
		int sourcePathLength = 0,
			destTitleLength = 0;

			// -- add source file
			sourcePath = (wchar_t*)szSrcLoc;
			sourcePathLength = CrackFilename(sourcePath, &sourceFilename, &sourceExtension);
			lstrcpyn(pFrom, sourcePath, buffsizeFrom);
			pFrom += sourcePathLength + 1;
			*pFrom = 0;

			// -- add destination file
			// add destination directory
			lstrcpyn(pTo, szDestLoc, nFileOffset+1);
			pTo += nFileOffset;
			destFilename = pTo; // just filename (no path) for playlist

			// include parent dir
			if(g_bIncludeDirectory && sourceFilename > sourcePath + 4){ // is this a problem for network shares?
			wchar_t* parentDirectory = sourceFilename-2;
				while(parentDirectory>sourcePath+4 && *parentDirectory!='\\') parentDirectory--;
				lstrcpyn(pTo, parentDirectory+1, (int)(sourceFilename - parentDirectory));
				pTo += sourceFilename - parentDirectory - 1;
			}

			// add number
			if(g_bNumberFiles){
				StringCchPrintf(pTo, buffsizeTo, numberFormatString, x+1);
				pTo += numberStringLength;
			}

			// get dest title (either playlist title or filename without path and extension)
			if(g_bUsePlaylistTitle){
				destTitle = (wchar_t*)GetPlaylistItemTitle(x);
				destTitleLength = (int)wcslen(destTitle);
			}
			else{
				destTitle = sourceFilename;
				destTitleLength = (int)(sourceExtension-sourceFilename);
			}

			// add dest title
			lstrcpyn(pTo, destTitle, destTitleLength + 1);

			// Remove bad chars from the file name (note much of this dll is not unicode-safe!)
			wchar_t *p = pTo;
			for(;*p;p=CharNext(p)) {
				switch(*p) {
					case L'<': *p=L'«'; break;
					case L'>': *p=L'»'; break;
					case L'"': *p=L'\'';break;
					case L'?': *p=L'¿'; break;
					case L'*': *p=L'+'; break;
					case L'|': *p=L'!'; break;
					case L'\\':*p=L'_'; break;
					case L'/':
					case L':': *p='-';
				}
			}

			pTo += destTitleLength;

			// add file extension
			lstrcpyn(pTo, sourceExtension, buffsizeTo);
			pTo += wcslen(sourceExtension) + 1;
			*pTo = 0;

			// -- add playlist entry
			if(g_bSavePlaylist){
			int songlength;
			wchar_t *playlistTitle = 0, temp[50] = { 0 };

				SetAndPlayPlaylistPos(x, 0);
				songlength = GetCurrentTrackLengthSeconds();
				playlistTitle = (wchar_t*)GetPlaylistItemTitle(x);
				StringCchPrintf(temp, ARRAYSIZE(temp), L"#EXTINF:%d,", songlength);

				WriteLine(hPlsFile, temp);
				WriteLine(hPlsFile, playlistTitle);
				WriteLine(hPlsFile, L"\r\n");
				WriteLine(hPlsFile, destFilename);
				WriteLine(hPlsFile, L"\r\n");
			}
		}
		// bad/missing file
		else{
		wchar_t temp[MAX_PATH + 20] = { 0 };
			StringCchPrintf(temp, ARRAYSIZE(temp), WASABI_API_LNGSTRINGW(IDS_ERROR_COPYING), szSrcLoc);
			GET_UNICODE_TITLE();
			MessageBox(plugin.hwndParent, temp, title, MB_OK);
		}
	}

	if(g_bSavePlaylist){
		CloseHandle(hPlsFile);
	}

	// now copy the files
	SHFILEOPSTRUCT fileOp = { 0 };
	fileOp.hwnd = plugin.hwndParent;
	fileOp.wFunc = FO_COPY;
	fileOp.pFrom = bigbuffFrom;
	fileOp.pTo = bigbuffTo;
	fileOp.fFlags = FOF_MULTIDESTFILES | FOF_NOCONFIRMMKDIR;
	FileAction(&fileOp);

	plugin.memmgr->sysFree(bigbuffFrom);
	plugin.memmgr->sysFree(bigbuffTo);
}

// only use this on 5.53+ otherwise it may crash, etc
void addAccelerators(HWND hwnd, ACCEL* accel, int accel_size, int translate_mode){
	HACCEL hAccel = CreateAcceleratorTable(accel,accel_size);
	if (hAccel) plugin.app->app_addAccelerators(hwnd, &hAccel, accel_size, translate_mode);
}

void SaveThemFiles(void){
OPENFILENAME ofn = { 0 };
wchar_t szDestLoc[MAX_PATH] = { 0 },
		allFiles[32] = { 0 };

	ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK | OFN_ENABLESIZING;
	ofn.hInstance = WASABI_API_LNG_HINST;
	ofn.lpTemplateName = MAKEINTRESOURCE(IDD_FILEDLG);
	ofn.lpfnHook = &OFNHookProc;
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szDestLoc;
	ofn.nMaxFile = ARRAYSIZE(szDestLoc);
	ofn.lpstrTitle = WASABI_API_LNGSTRINGW(IDS_SELECT_LOCATION);
	ofn.hwndOwner = plugin.hwndParent;
	ofn.lpstrFilter = WASABI_API_LNGSTRINGW_BUF(IDS_ALL_FILES,allFiles,ARRAYSIZE(allFiles));
	ofn.lpstrInitialDir = ofn.lpstrDefExt = ofn.lpstrCustomFilter = ofn.lpstrFileTitle = NULL;
	WASABI_API_LNGSTRINGW_BUF(IDS_FILENAME_IGNORED,szDestLoc,MAX_PATH);

	if(SaveFileName(&ofn)){
		CopyFiles(szDestLoc, ofn.nFileOffset);
	}
}

void __cdecl MessageProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
	// we handle both messages so we can get the action when sent via the keyboard
	// shortcut but also copes with using the menu via Winamp's taskbar system menu
	if ((uMsg == WM_SYSCOMMAND || uMsg == WM_COMMAND) && LOWORD(wParam) == my_menu)
	{
		SaveThemFiles();
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
	WASABI_API_LNG = plugin.language;
	WASABI_API_START_LANG(plugin.hDllInstance,GenYarLangGUID);

	static wchar_t pluginTitleW[256] = { 0 };
	StringCchPrintf(pluginTitleW, ARRAYSIZE(pluginTitleW), WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME), TEXT(APPVER));
	plugin.description = (char*)plugin.memmgr->sysDupStr(pluginTitleW);

	my_menu = RegisterCommandID(0);
	accel[0].cmd = (WORD)my_menu;
	accel[1].cmd = (WORD)my_menu;

	// we associate the accelerators to the playlist editor as
	// otherwise the adding will fail due to a quirk in the api
	addAccelerators(GetPlaylistWnd(),accel,2,TRANSLATE_MODE_GLOBAL);

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
	mii.cch = (UINT)(wcslen(mii.dwTypeData)+1);
	InsertMenuItem(menu,GetMenuItemCount(menu)-adjuster-1,1,&mii);
	return GEN_INIT_SUCCESS;
}

extern "C" __declspec( dllexport ) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin(){
	return &plugin;
}

extern "C" __declspec( dllexport ) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param){
	GET_UNICODE_TITLE();
	if(MessageBox(hwndDlg, WASABI_API_LNGSTRINGW(IDS_UNINSTALL_PROMPT), title, MB_YESNO) == IDYES){
		WritePrivateProfileString(APPNAMEW,0,0,GetPaths()->plugin_ini_file);
	}
	return GEN_PLUGIN_UNINSTALL_REBOOT;
}