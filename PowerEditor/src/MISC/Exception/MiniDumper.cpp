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
static constexpr size_t nppDumpPathEndingLen = _countof(nppDumpName) - 1 + _countof(nppDumpDateId) - 1 + _countof(nppDumpExt) - 1;


// important note: as this is used at the crash-time, stick to base WINAPI calls, do not use here any library (C++ and CRT included)


MiniDumper::MiniDumper()
{
	// to avoid memory (re)allocations at the crash-time
	_dumppath.resize(MAX_PATH + 1 + nppDumpPathEndingLen);
	_msg.resize(_dumppath.size() + 128);
}

bool MiniDumper::writeDump(EXCEPTION_POINTERS* pExceptionInfo, SYSTEMTIME stExc)
{
	bool retval = false;

	HMODULE hDll = ::LoadLibraryEx(L"DBGHELP.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!hDll)
	{
		_msg = L"Unable to load the debugging DLL,\r\nfind a recent copy of dbghelp.dll and install it.";
	}
	else
	{
		MINIDUMPWRITEDUMP pfnDump = reinterpret_cast<MINIDUMPWRITEDUMP>(::GetProcAddress(hDll, "MiniDumpWriteDump"));
		if (!pfnDump)
		{
			_msg = L"The debugging DLL seems to be outdated,\r\nfind a recent copy of dbghelp.dll and install it.";
		}
		else
		{
			int msgret = ::MessageBoxW(NULL, L"Do you want to save a minidump file?\r\n(doing so can aid in developing Notepad++)",
				msgTitle, MB_YESNO | MB_ICONQUESTION);
			if (msgret == IDYES)
			{
				DWORD dwLen = ::GetTempPathW(MAX_PATH + 1, _dumppath.data()); // always normalized with backslash at the end
				if (dwLen == 0) // GetTempPath cannot return more than MAX_PATH+1
				{
					_msg = L"Failed to get TEMP location for the dump file output! (error " + std::to_wstring(::GetLastError()) + L")";
				}
				else
				{
					int nWCharsWritten = wsprintfW(_dumppath.data() + dwLen, L"%s_%04u%02u%02u_%02u%02u%02u%s",
						nppDumpName,
						stExc.wYear, stExc.wMonth, stExc.wDay, stExc.wHour, stExc.wMinute, stExc.wSecond,
						nppDumpExt);

					if (nWCharsWritten == nppDumpPathEndingLen)
						_dumppath.resize(dwLen + nWCharsWritten);
					else
						_dumppath.resize(dwLen);

					HANDLE hFile = ::CreateFileW(_dumppath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hFile == INVALID_HANDLE_VALUE)
					{
						_msg = L"Failed to create dump file (error " + std::to_wstring(::GetLastError()) + L"):\r\n'" + _dumppath + L"'";
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
							_msg = L"Successfully saved the dump to file:\r\n'" + _dumppath + L"'";
							retval = true;
						}
						else
						{
							_msg = L"Failed to write dump into file (error " + std::to_wstring(::GetLastError()) + L"):\r\n'" + _dumppath + L"'";
						}

						::CloseHandle(hFile);
					}
				}
			}
		}
		
		::FreeLibrary(hDll);
	}

	if (lstrlenW(_msg.c_str()) > 0) // do not use _msg.empty() here
		::MessageBoxW(NULL, _msg.c_str(), msgTitle, retval ? (MB_OK | MB_ICONINFORMATION) : (MB_OK | MB_ICONWARNING));

	return retval;
}
