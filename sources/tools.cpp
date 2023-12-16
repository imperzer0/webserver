// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.

#include "tools.h"


mutex_locker::mutex_locker(pthread_mutex_t* mutex) : mutex(mutex)
{
	pthread_mutex_lock(mutex);
}

mutex_locker::~mutex_locker()
{
	pthread_mutex_unlock(mutex);
}


bool starts_with(const char* str, const char* prefix)
{
	for (; *prefix; ++prefix, ++str)
		if (*prefix != *str)
			return false;
	return true;
}

std::string getcwd()
{
	char cwd[PATH_MAX] { };
	getcwd(cwd, PATH_MAX);
	cwd[PATH_MAX - 1] = 0;
	std::string cwdstr(cwd);
	return std::move(cwdstr);
}


std::string erase_all(const std::string& str, const std::string& seq)
{
	std::string res(str, 0, seq.size());
	res.reserve(str.size()); // optional, avoids buffer reallocations in the loop
	for (size_t i = seq.size(); i < str.size(); ++i)
	{
		bool ok = false;
		for (int j = seq.size() - 1, k = i; j >= 0; --j, --k)
			if (seq[j] != str[k])
			{
				ok = true;
				break;
			}
		if (ok) res += str[i];
	}
	return std::move(res);
}

std::string secure_path(const std::string& path)
{
	std::string res = erase_all(path, "../");
	return erase_all(res, "/..");
}


std::string path_dirname(const std::string& path)
{
	if (path.empty()) return "";
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos) return "";
	return std::move(path.substr(0, slash));
}


std::string path_basename(std::string path)
{
	if (path.empty()) return "";
	while (path.ends_with('/')) path.pop_back();
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos) return "";
	return std::move(path.substr(slash + 1));
}
