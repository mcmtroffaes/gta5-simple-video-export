/*
A simple wrapper around CreateFile, which automatically closes the handle.
*/

#include "filehandle.h"
#include "logger.h"
#include "settings.h"

FileHandle::FileHandle() : handle_(INVALID_HANDLE_VALUE), path_() {};

FileHandle::FileHandle(const std::string & path) : FileHandle() {
	LOG_ENTER;
	path_ = path;
	LOG->info("opening file {} for writing", path_);
	handle_ = CreateFileA(path_.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!IsValid()) {
		LOG->error("failed to create file");
	}
	LOG_EXIT;
};

FileHandle::~FileHandle() {
	LOG_ENTER;
	if (IsValid()) {
		LOG->info("closing {}", path_);
		CloseHandle(handle_);
	}
	LOG_EXIT;
}

bool FileHandle::IsValid() const
{
	return (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE);
}

HANDLE FileHandle::Handle() const {
	return handle_;
}

const std::string & FileHandle::Path() const
{
	return path_;
}
