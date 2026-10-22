/* HopCast.exe: starts bin\64bit\obs64.exe from the folder this file sits in, with the arguments it was
 * given. The program needs its own folder as the working directory to find its libraries, so a
 * double-click on this launcher (or a shortcut to it) is all a person has to do. GPL-2.0-or-later. */
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
	(void)inst; (void)prev; (void)show;
	wchar_t self[MAX_PATH];
	DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
	if (n == 0 || n >= MAX_PATH)
		return 1;
	wchar_t *slash = wcsrchr(self, L'\\');
	if (!slash)
		return 1;
	*slash = L'\0';

	wchar_t dir[MAX_PATH + 16];
	_snwprintf(dir, sizeof dir / sizeof *dir, L"%s\\bin\\64bit", self);

	wchar_t cmd[32768];
	_snwprintf(cmd, sizeof cmd / sizeof *cmd, L"\"%s\\obs64.exe\" %s", dir, args ? args : L"");

	STARTUPINFOW si = {sizeof si};
	PROCESS_INFORMATION pi;
	if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
		MessageBoxW(NULL, L"HopCast could not start. Keep the whole HopCast folder together (bin, data, obs-plugins ...).",
			    L"HopCast", MB_OK | MB_ICONERROR);
		return 1;
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return 0;
}
