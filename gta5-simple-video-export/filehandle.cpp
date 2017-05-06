#include "filehandle.h"
#include "logger.h"
#include "settings.h"

#include <Shlwapi.h> // PathCombine
#pragma comment(lib, "Shlwapi.lib")

FileHandle::FileHandle() : handle_(INVALID_HANDLE_VALUE), path_() {};

FileHandle::FileHandle(const std::string & filename) : FileHandle() {
	LOG_ENTER;
	if (settings && !settings->output_folder_.empty()) {
		char path[MAX_PATH] = "";
		if (PathCombineA(path, settings->output_folder_.c_str(), filename.c_str()) == nullptr) {
			LOG->error("could not combine {} and {} to form path of output stream", settings->output_folder_, filename);
		}
		else {
			path_ = path;
			std::string pipe_prefix = "\\\\.\\pipe\\";
			if (path_.substr(0, pipe_prefix.length()) == pipe_prefix) {
				LOG->info("opening pipe {} for writing", path_);
				handle_ = CreateNamedPipeA(path_.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE, 1, 0, 0, 0, NULL);
				if (!IsValid()) {
					LOG->error("failed to create pipe");
				}
				else {
					if (!ConnectNamedPipe(handle_, NULL)) {
						LOG->error("failed to make connection on pipe");
						CloseHandle(handle_); // close the pipe
						handle_ = INVALID_HANDLE_VALUE;
					}
				}
			}
			else {
				LOG->info("opening file {} for writing", path_);
				handle_ = CreateFileA(path_.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (!IsValid()) {
					LOG->error("failed to create file");
				}
			}
		}
	}
	LOG_EXIT;
};

FileHandle::~FileHandle() {
	LOG_ENTER;
	if (IsValid()) {
		LOG->debug("closing {}", path_);
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
