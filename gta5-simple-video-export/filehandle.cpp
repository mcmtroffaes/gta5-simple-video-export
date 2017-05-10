#include "filehandle.h"
#include "logger.h"

FileHandle::FileHandle() : handle_(INVALID_HANDLE_VALUE), path_() {};

FileHandle::FileHandle(const std::wstring & path) : FileHandle() {
	LOG_ENTER;
	path_ = path;
	LOG->info("opening file {} for writing", wstring_to_utf8(path_));
	handle_ = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!IsValid()) {
		LOG->error("failed to create file");
	}
	LOG_EXIT;
};

FileHandle::~FileHandle() {
	LOG_ENTER;
	if (IsValid()) {
		LOG->info("closing {}", wstring_to_utf8(path_));
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

const std::wstring & FileHandle::Path() const
{
	return path_;
}
