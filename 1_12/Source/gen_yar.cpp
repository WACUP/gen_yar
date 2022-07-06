#define UNICODE
#define WIN32_MEAN_AND_LEAN
// this shows places bar in filedialog
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <strsafe.h>

#include "../sdk/nu/AutoChar.h"
#include "../sdk/winamp/gen.h"
#include "../sdk/winamp/wa_ipc.h"
#ifndef WINAMP_BUTTON4
#define WINAMP_BUTTON4 40047
#endif

#include "resource.h"
#include "api.h"

#define APPNAME  "Yar-matey! Playlist Copier"
#define APPNAMEW L"Yar-matey! Playlist Copier"
#define APPVER   "1.12"
#define APPVERW  L"1.12"


int PlayListCount = 0;
UINT my_menu = -1;
BOOL g_bSavePlaylist = FALSE,
	 g_bSavem3u8 = FALSE,
	 g_bNumberFiles = TRUE,
	 g_bUsePlaylistTitle = FALSE,
	 g_bIncludeDirectory = FALSE;
wchar_t ini_file[MAX_PATH] = {0},
		title[256] = {0};
WNDPROC WinampProc = 0;
HWND playlist_wnd = 0;

#define MYUPDATER WM_USER+70

// Wasabi based services for localisation support
api_service *WASABI_API_SVC = 0;
api_application *WASABI_API_APP = 0;
api_language *WASABI_API_LNG = 0;
// these two must be declared as they're used by the language api's
// when the system is comparing/loading the different resources
HINSTANCE WASABI_API_LNG_HINST = 0,
		  WASABI_API_ORIG_HINST = 0;

#define GET_UNICODE_TITLE() {if(!title[0]) StringCchPrintf(title, 256, WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME), APPVERW);}

#ifndef _DEBUG
BOOL WINAPI _DllMainCRTStartup(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved){
	DisableThreadLibraryCalls(hModule);
	return TRUE;
}
#endif

void config();
void quit(){}
int init();

winampGeneralPurposePlugin plugin = {
	GPPHDR_VER_U,
	APPNAME" v"APPVER,
	init,
	config,
	quit,
};

UINT ver = -1;
UINT GetWinampVersion(HWND winamp){
	if(ver == -1){
		return (ver = SendMessage(winamp,WM_WA_IPC,0,IPC_GETVERSION));
	}
	return ver;
}

int config_open = 0;
void config(){
	if(!config_open){
	wchar_t message[512] = {0};
		config_open = 1;
		GET_UNICODE_TITLE();
		StringCchPrintf(message, 512, WASABI_API_LNGSTRINGW(IDS_ABOUT_MESSAGE), title);
		MessageBox(0, message, title, MB_SYSTEMMODAL | MB_ICONINFORMATION);
		config_open = 0;
	}
	else{
		SetActiveWindow(FindWindow(L"#32770", (wchar_t*)plugin.description));
	}
}

void config_read(void){
	g_bSavePlaylist = GetPrivateProfileInt(APPNAMEW, L"SavePlaylist", g_bSavePlaylist, ini_file);
	g_bSavem3u8 = GetPrivateProfileInt(APPNAMEW, L"SaveM3U8", g_bSavem3u8, ini_file);
	g_bNumberFiles = GetPrivateProfileInt(APPNAMEW, L"NumberFilenames", g_bNumberFiles, ini_file);
	g_bUsePlaylistTitle = GetPrivateProfileInt(APPNAMEW, L"UsePlaylistTitle", g_bUsePlaylistTitle, ini_file);
	g_bIncludeDirectory = GetPrivateProfileInt(APPNAMEW,L"IncludeDirectory", g_bIncludeDirectory, ini_file);
}

void config_write(void){
wchar_t string[32] = {0};

	StringCchPrintf(string, 32, L"%d", g_bSavePlaylist);
	WritePrivateProfileString(APPNAMEW, L"SavePlaylist", string, ini_file);

	StringCchPrintf(string, 32, L"%d", g_bSavem3u8);
	WritePrivateProfileString(APPNAMEW, L"SaveM3U8", string, ini_file);

	StringCchPrintf(string, 32, L"%d", g_bNumberFiles);
	WritePrivateProfileString(APPNAMEW, L"NumberFilenames", string, ini_file);

	StringCchPrintf(string, 32, L"%d", g_bUsePlaylistTitle);
	WritePrivateProfileString(APPNAMEW, L"UsePlaylistTitle", string, ini_file);

	StringCchPrintf(string, 32, L"%d", g_bIncludeDirectory);
	WritePrivateProfileString(APPNAMEW, L"IncludeDirectory", string, ini_file);
}

DWORD FileSize(const wchar_t* szFileName){
WIN32_FIND_DATA ffd = {0};
HANDLE hFind = FindFirstFile(szFileName, &ffd);
	if(hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
	else ffd.nFileSizeLow=0;
	return ffd.nFileSizeLow;
}

DWORD WINAPI FileCountThread(LPVOID lpParameter){
HWND hwnd=(HWND)lpParameter;
DWORD fsize=0;

	for(int x = 0; x < PlayListCount; x++){
		const wchar_t *szFileName = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x, IPC_GETPLAYLISTFILEW);
		fsize += (FileSize(szFileName) / 1024);

		if(PlayListCount > 10 && (x % (PlayListCount / 10) == 0)){
			if(IsWindow(hwnd)) PostMessage((HWND)hwnd, MYUPDATER, fsize, TRUE);
			else return 0;
		}
	}

	if(IsWindow(hwnd)) PostMessage((HWND)hwnd, MYUPDATER, fsize, FALSE);
	return 0;
}

void FileHookInitDialog(HWND hdlg){
DWORD dwThreadId = 0;
wchar_t crap[255] = {0};

	PlayListCount = SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GETLISTLENGTH);
	CreateThread(NULL, 0, FileCountThread, (LPVOID)hdlg, 0, &dwThreadId);

	if(PlayListCount>1000){
		StringCchPrintf(crap, 255, L"%d,%03d", PlayListCount/1000, PlayListCount%1000);
	}
	else{
		StringCchPrintf(crap, 255, L"%d", PlayListCount);
	}

	SetDlgItemText(hdlg, IDC_FILES, crap);

	config_read();
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
	config_write();
}

UINT APIENTRY OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam){
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
			RECT rect = {0};
				GetWindowRect(GetParent(hdlg), &rect);
				SetWindowPos(GetParent(hdlg), 0, 
							 (GetSystemMetrics(SM_CXSCREEN)-(rect.right-rect.left))/2,
							 (GetSystemMetrics(SM_CYSCREEN)-(rect.bottom-rect.top))/2,
							 0, 0, SWP_NOSIZE);
			}
		return TRUE;

		case MYUPDATER:
		{
			wchar_t crap[255] = {0}, temp[128] = {0};
			StringCchPrintf(crap, 255, L"%s%s",
							(lParam ? L"+" : L""),
							WASABI_API_LNG->FormattedSizeString(temp, ARRAYSIZE(temp), wParam * 1024));
			SetDlgItemText(hdlg, IDC_FILESIZE, crap);
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
	return WriteFile(hFile, textA8, lstrlenA(textA8), &byteswritten, NULL);
}

// given szFile = "c:\pathname\stuff\application.exe"
// return int lstrlen
//        NameStart = "application.exe"
//        ExtStart  = ".exe"
//
int CrackFilename(wchar_t* szFile, wchar_t** ppNameStart, wchar_t** ppExtStart){
int stringLength = lstrlen(szFile);
wchar_t *NameStart = wcsrchr(szFile, '\\'),
	 *ExtStart = 0;

	if(!NameStart){
		NameStart = szFile;
	}
	else{
		NameStart++;
	}

	if(ppExtStart){
		ExtStart = wcsrchr(NameStart, '.');
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
		numberFormatString[6] = {0}; // "%03d_"
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
		StringCchPrintf(numberFormatString, 6, L"%%0%dd_", numberLength);
		numberStringLength = numberLength +1;
	}

	// how big of a buffer ?
	buffsizeFrom = buffsizeTo = 0;
	destDirLength = lstrlen(szDestLoc);
	
	for(x = 0; x < PlayListCount; x++){
	wchar_t *sourcePath,
			*sourceFilename,
			*sourceExtension;
	int sourcePathLength,
		destTitleLength,
		destPathLength;

		// determine source length
		sourcePath = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x, IPC_GETPLAYLISTFILEW);
		sourcePathLength = CrackFilename(sourcePath, &sourceFilename, &sourceExtension);
		buffsizeFrom += sourcePathLength +1; //+1 for separator

		// determine dest length
		if(g_bUsePlaylistTitle){
			wchar_t *destTitle = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x, IPC_GETPLAYLISTTITLEW);
			destTitleLength = lstrlen(destTitle);
		}
		else{
			destTitleLength = sourceExtension-sourceFilename;
		}

		// get parent dir
		// is this a problem for network shares?
		if(g_bIncludeDirectory && (sourceFilename > sourcePath + 4)){
		wchar_t* parentDirectory = sourceFilename-2;
			while(parentDirectory > sourcePath+4 && *parentDirectory!='\\') parentDirectory--;
			destTitleLength += sourceFilename-parentDirectory;
		}

		destPathLength = destDirLength + destTitleLength + lstrlen(sourceExtension);
		buffsizeTo += destPathLength + 1 +(g_bNumberFiles?numberStringLength:0); //+1 for separator, +4 for "003_"
	}

	// allocate From and To buffers
	bigbuffFrom = (wchar_t*)malloc((buffsizeFrom+1)*sizeof(wchar_t)); // +1 for termination
	bigbuffTo = (wchar_t*)malloc((buffsizeTo+1)*sizeof(wchar_t)); // +1 for termination

	// now build the big strings
	pFrom = bigbuffFrom;
	pTo = bigbuffTo;
	for(x = 0; x < PlayListCount; x++) {
		const wchar_t *szSrcLoc = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x, IPC_GETPLAYLISTFILEW);
		if(FileSize(szSrcLoc) > 0) { // skip missing / bad files
		wchar_t *destTitle = 0,
				*sourcePath = 0,
				*sourceFilename = 0,
				*sourceExtension = 0,
				*destFilename = 0;
		int sourcePathLength = 0,
			destTitleLength = 0;

			// -- add source file
			sourcePath = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x, IPC_GETPLAYLISTFILEW);
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
				lstrcpyn(pTo, parentDirectory+1, sourceFilename - parentDirectory);
				pTo += sourceFilename - parentDirectory - 1;
			}

			// add number
			if(g_bNumberFiles){
				StringCchPrintf(pTo, buffsizeTo, numberFormatString, x+1);
				pTo += numberStringLength;
			}

			// get dest title (either playlist title or filename without path and extension)
			if(g_bUsePlaylistTitle){
				destTitle = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x,IPC_GETPLAYLISTTITLEW);
				destTitleLength = lstrlen(destTitle);
			}
			else{
				destTitle = sourceFilename;
				destTitleLength = sourceExtension-sourceFilename;
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
			pTo += lstrlen(sourceExtension) + 1;
			*pTo = 0;

			// -- add playlist entry
			if(g_bSavePlaylist){
			int songlength;
			wchar_t *playlistTitle = 0,
				 crap[50] = {0};

				SendMessage(plugin.hwndParent, WM_WA_IPC, x, IPC_SETPLAYLISTPOS);
				songlength = SendMessage(plugin.hwndParent, WM_WA_IPC, 1, IPC_GETOUTPUTTIME);
				playlistTitle = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, x,IPC_GETPLAYLISTTITLEW);
				StringCchPrintf(crap, 50, L"#EXTINF:%d,", songlength);

				WriteLine(hPlsFile, crap);
				WriteLine(hPlsFile, playlistTitle);
				WriteLine(hPlsFile, L"\r\n");
				WriteLine(hPlsFile, destFilename);
				WriteLine(hPlsFile, L"\r\n");
			}
		}
		// bad/missing file
		else{
		wchar_t crap[MAX_PATH+20] = {0};
			StringCchPrintf(crap, MAX_PATH+20, WASABI_API_LNGSTRINGW(IDS_ERROR_COPYING), szSrcLoc);
			GET_UNICODE_TITLE();
			MessageBox(plugin.hwndParent, crap, title, MB_OK);
		}
	}

	if(g_bSavePlaylist){
		CloseHandle(hPlsFile);
	}

	// now copy the files
	SHFILEOPSTRUCT fileOp = {0};
	fileOp.hwnd = plugin.hwndParent;
	fileOp.wFunc = FO_COPY;
	fileOp.pFrom = bigbuffFrom;
	fileOp.pTo = bigbuffTo;
	fileOp.fFlags = FOF_MULTIDESTFILES | FOF_NOCONFIRMMKDIR;
	SHFileOperation(&fileOp);

	free(bigbuffFrom);
	free(bigbuffTo);
}

template <class api_T>
void ServiceBuild(api_T *&api_t, GUID factoryGUID_t){
	if (WASABI_API_SVC){
		waServiceFactory *factory = WASABI_API_SVC->service_getServiceByGuid(factoryGUID_t);
		if (factory){
			api_t = (api_T *)factory->getInterface();
		}
	}
}

// only use this on 5.53+ otherwise it may crash, etc
void addAccelerators(HWND hwnd, ACCEL* accel, int accel_size, int translate_mode){
	if(!WASABI_API_APP) ServiceBuild(WASABI_API_APP,applicationApiServiceGuid);
	HACCEL hAccel = CreateAcceleratorTable(accel,accel_size);
	if (hAccel) WASABI_API_APP->app_addAccelerators(hwnd, &hAccel, accel_size, translate_mode);
}

void SaveThemFiles(void){
OPENFILENAME ofn = {0};
wchar_t szDestLoc[MAX_PATH] = {0},
		allFiles[32] = {0};

	ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK | OFN_ENABLESIZING;
	ofn.hInstance = WASABI_API_LNG_HINST;
	ofn.lpTemplateName = MAKEINTRESOURCE(IDD_FILEDLG);
	ofn.lpfnHook = &OFNHookProc;
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szDestLoc;
	ofn.nMaxFile = ARRAYSIZE(szDestLoc);
	ofn.lpstrTitle = WASABI_API_LNGSTRINGW(IDS_SELECT_LOCATION);
	ofn.hwndOwner = plugin.hwndParent;
	ofn.lpstrFilter = WASABI_API_LNGSTRINGW_BUF(IDS_ALL_FILES,allFiles,32);
	ofn.lpstrInitialDir = ofn.lpstrDefExt = ofn.lpstrCustomFilter = ofn.lpstrFileTitle = NULL;
	WASABI_API_LNGSTRINGW_BUF(IDS_FILENAME_IGNORED,szDestLoc,MAX_PATH);

	if(GetSaveFileName(&ofn)){
		CopyFiles(szDestLoc, ofn.nFileOffset);
	}
}

LRESULT CALLBACK HookWinampWnd(HWND hwnd, UINT umsg, WPARAM wp, LPARAM lp){
	// handles the item being selected through the taskbar menu as won't work otherwise
	if(umsg == WM_SYSCOMMAND && (wp&0xFFF0) == my_menu){
		SaveThemFiles();
		return 0;
	}
	else if(umsg == WM_COMMAND && LOWORD(wp) == my_menu){
		SaveThemFiles();
	}
	return CallWindowProc(WinampProc, hwnd, umsg, wp, lp); 
}

int init(){
	if(GetWinampVersion(plugin.hwndParent) < 0x5064){
		MessageBoxA(plugin.hwndParent,"This plug-in requires Winamp v5.64 and up for it to work.\t\n"
									  "Please upgrade your Winamp client to be able to use this.",
									  plugin.description,MB_OK|MB_ICONINFORMATION);
		return GEN_INIT_FAILURE;
	}
	else{
		WASABI_API_SVC = (api_service*)SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GET_API_SERVICE);
		if (WASABI_API_SVC == (api_service*)1) WASABI_API_SVC = NULL;

		if(WASABI_API_SVC != NULL){
		ACCEL accel[] = {{FVIRTKEY|FALT,'C',},
						 // this is an alternative for mainly Bento/SUI skins
						 // where Alt+C is already being used for Skin Settings
						 {FVIRTKEY|FALT,'R',}};
		MENUITEMINFO mii = {0};
		// get a handle of the playlist editor right-click menu
		HMENU menu = GetSubMenu((HMENU)SendMessage(plugin.hwndParent,WM_WA_IPC,(WPARAM)-1,IPC_GET_HMENU),0);

			ServiceBuild(WASABI_API_LNG,languageApiGUID);
			WASABI_API_START_LANG(plugin.hDllInstance,GenYarLangGUID);

			static wchar_t pluginTitle[MAX_PATH] = {0};
			StringCchPrintfW(pluginTitle, MAX_PATH, WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME), APPVERW);
			plugin.description = (char*)pluginTitle;

			playlist_wnd = (HWND)SendMessage(plugin.hwndParent,WM_WA_IPC,IPC_GETWND_PE,IPC_GETWND);

			my_menu = SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_REGISTER_LOWORD_COMMAND);
			accel[0].cmd = my_menu;
			accel[1].cmd = my_menu;

			// we associate the accelerators to the playlist editor as
			// otherwise the adding will fail due to a quirk in the api
			addAccelerators(playlist_wnd,accel,2,TRANSLATE_MODE_GLOBAL);

			#define ID_HELP_HELPTOPICS 40347
			int adjuster = 3 + (GetMenuItemID(menu,GetMenuItemCount(menu)-3) != ID_HELP_HELPTOPICS);
			mii.cbSize = sizeof(MENUITEMINFOW);
			mii.fMask = MIIM_ID|MIIM_TYPE|MIIM_DATA;
			mii.fType = MFT_SEPARATOR;
			InsertMenuItem(menu,GetMenuItemCount(menu)-adjuster,1,&mii);

			mii.wID = my_menu;
			mii.fType = MFT_STRING;
			mii.dwTypeData = WASABI_API_LNGSTRINGW(IDS_COPY_FILE_MENU);
			mii.cch = lstrlen(mii.dwTypeData)+1;
			InsertMenuItem(menu,GetMenuItemCount(menu)-adjuster-1,1,&mii);

			StringCchPrintf(ini_file, MAX_PATH, L"%hs\\Plugins\\plugin.ini",
							SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETINIDIRECTORY));

			WinampProc = (WNDPROC)SetWindowLongPtrW(plugin.hwndParent,GWLP_WNDPROC,(LONG_PTR)HookWinampWnd);
			return GEN_INIT_SUCCESS;
		}
	}
	return GEN_INIT_FAILURE;
}

#ifdef __cplusplus
extern "C"
{
#endif
	__declspec( dllexport ) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin(){
		return &plugin;
	}

	__declspec( dllexport ) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param){
		GET_UNICODE_TITLE();
		if(MessageBox(hwndDlg, WASABI_API_LNGSTRINGW(IDS_UNINSTALL_PROMPT), title, MB_YESNO) == IDYES){
			WritePrivateProfileString(APPNAMEW,0,0,ini_file);
		}
		return GEN_PLUGIN_UNINSTALL_REBOOT;
	}
#ifdef __cplusplus
}
#endif