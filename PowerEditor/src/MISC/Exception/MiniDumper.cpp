//Adapted from http://www.codeproject.com/KB/debug/postmortemdebug_standalone1.aspx#_Reading_a_Minidump_with%20Visual%20Stud
//Modified for use by Npp

// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include <windows.h>
#include <dbghelp.h>
#include <shlwapi.h>
#include "MiniDumper.h"

typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
	const PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	const PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	const PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

static const wchar_t msgTitle[] = L"Notepad++ crash analysis";
static const wchar_t nppDumpName[] = L"NppDump";
static const wchar_t nppDumpDateId[] = L"_yyyymmdd_hhmmss";
static const wchar_t nppDumpExt[] = L".dmp";
static constexpr size_t nppDumpPathEndingLen = (sizeof(nppDumpName) + sizeof(nppDumpDateId) + sizeof(nppDumpExt)) / sizeof(wchar_t);


// important note: as this is used at the crash-time, stick to base WINAPI calls, do not use here any library (C++ and CRT included)


MiniDumper::MiniDumper()
{
}

bool MiniDumper::writeDump(EXCEPTION_POINTERS* pExceptionInfo, SYSTEMTIME stExc)
{
	wchar_t wszScratch[128 + MAX_PATH + nppDumpPathEndingLen]{};
	const wchar_t* wszResult = nullptr;
	bool retval = false;

	HMODULE hDll = ::LoadLibraryEx(L"DBGHELP.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!hDll)
	{
		wszResult = L"Unable to load the debugging DLL,\r\nfind a recent copy of dbghelp.dll and install it.";
	}
	else
	{
		MINIDUMPWRITEDUMP pfnDump = reinterpret_cast<MINIDUMPWRITEDUMP>(::GetProcAddress(hDll, "MiniDumpWriteDump"));
		if (!pfnDump)
		{
			wszResult = L"The debugging DLL is outdated,\r\nfind a recent copy of dbghelp.dll and install it.";
		}
		else
		{
			int msgret = ::MessageBoxW(NULL, L"Do you want to save a minidump file?\r\n(doing so can aid in developing Notepad++)",
				msgTitle, MB_YESNO | MB_ICONQUESTION);
			if (msgret == IDYES)
			{
				wchar_t wszDumpPath[MAX_PATH + nppDumpPathEndingLen + 1]{};
				::GetTempPathW(MAX_PATH, wszDumpPath); // always normalized with backslash at the end
				wsprintfW(wszDumpPath + lstrlenW(wszDumpPath), L"%s_%04u%02u%02u_%02u%02u%02u%s",
					nppDumpName,
					stExc.wYear, stExc.wMonth, stExc.wDay, stExc.wHour, stExc.wMinute, stExc.wSecond,
					nppDumpExt);

				HANDLE hFile = ::CreateFileW(wszDumpPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile == INVALID_HANDLE_VALUE)
				{
					wsprintfW(wszScratch, L"Failed to create dump file '%s' (error %d)!", wszDumpPath, ::GetLastError());
					wszResult = wszScratch;
				}
				else
				{
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo{};
					ExInfo.ThreadId = ::GetCurrentThreadId();
					ExInfo.ExceptionPointers = pExceptionInfo;
					ExInfo.ClientPointers = FALSE;

					// write the dump
					if (pfnDump(::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL))
					{
						wsprintfW(wszScratch, L"Successfully saved the dump file to:\r\n'%s'", wszDumpPath);
						wszResult = wszScratch;
						retval = true;
					}
					else
					{
						wsprintfW(wszScratch, L"Failed to save dump file to '%s' (error %d)!", wszDumpPath, ::GetLastError());
						wszResult = wszScratch;
					}
					::CloseHandle(hFile);
				}
			}
		}
		
		::FreeLibrary(hDll);
	}

	if (wszResult)
		::MessageBoxW(NULL, wszResult, msgTitle, retval ? (MB_OK | MB_ICONINFORMATION) : (MB_OK | MB_ICONWARNING));

	return retval;
}
