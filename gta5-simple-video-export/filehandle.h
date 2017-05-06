#pragma once

#include <windows.h>
#include <string>

class FileHandle {
public:
	FileHandle();
	FileHandle(const std::string & filename);
	~FileHandle();
	bool IsValid() const;
	HANDLE Handle() const;
	const std::string & Path() const;
private:
	HANDLE handle_;
	std::string path_;
};
