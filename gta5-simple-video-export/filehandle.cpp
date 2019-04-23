#include "filehandle.h"
#include "logger.h"

std::wstring wstring_from_utf8(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > myconv;
	return myconv.from_bytes(str);
}

FileHandle::FileHandle() : handle_(INVALID_HANDLE_VALUE), path_() {};

FileHandle::FileHandle(const std::string & path) : FileHandle() {
	LOG_ENTER;
	LOG->info("opening file {} for writing", path);
	path_ = path;
	auto wpath = wstring_from_utf8(path);
	handle_ = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
