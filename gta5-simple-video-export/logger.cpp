#include "logger.h"

std::string wstring_to_utf8(const std::wstring & str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > myconv;
	return myconv.to_bytes(str);
}
