/*
A simple wrapper around CreateFile, which automatically closes the handle,
and which logs when a file is created and closed.
*/

#pragma once

#include <windows.h>
#include <string>

class FileHandle {
public:
	FileHandle();
	FileHandle(const std::wstring & path);
	~FileHandle();
	bool IsValid() const;
	HANDLE Handle() const;
private:
	HANDLE handle_;
	std::wstring path_;
};
