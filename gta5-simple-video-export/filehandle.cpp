/*
A simple wrapper around CreateFile and CreateNamedPipe, which automatically
closes the handle.
*/

#include "filehandle.h"
#include "logger.h"
#include "settings.h"

FileHandle::FileHandle() : handle_(INVALID_HANDLE_VALUE), path_() {};

FileHandle::FileHandle(const std::string & path, bool pipe) : FileHandle() {
	LOG_ENTER;
	path_ = path;
	if (pipe) {
		LOG->info("opening pipe {} for writing", path_);
		handle_ = CreateNamedPipeA(path_.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE, 1, 0, 0, 0, NULL);
		if (!IsValid()) {
			LOG->error("failed to create pipe");
		}
	}
	else {
		LOG->info("opening file {} for writing", path_);
		handle_ = CreateFileA(path_.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (!IsValid()) {
			LOG->error("failed to create file");
		}
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
	LOG_ENTER;
	auto is_valid = (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE);
	LOG_EXIT;
	return is_valid;
}

HANDLE FileHandle::Handle() const {
	LOG_ENTER;
	LOG_EXIT;
	return handle_;
}

const std::string & FileHandle::Path() const
{
	LOG_ENTER;
	LOG_EXIT;
	return path_;
}
