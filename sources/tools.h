// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.


#ifndef WEBSERVER_TOOLS_H
#define WEBSERVER_TOOLS_H

#include <pthread.h>
#include <string>
#include <climits>
#include <unistd.h>


class mutex_locker
{
private:
	pthread_mutex_t* mutex;
public:
	explicit mutex_locker(pthread_mutex_t* mutex);

	~mutex_locker();
};


inline static consteval size_t static_strlen(const char* str)
{
	size_t len = 0;
	for (; *str; ++len, ++str);
	return len;
}


/// Return true if str starts with prefix
extern bool starts_with(const char* str, const char* prefix);

/// Get Current Working Directory
extern std::string getcwd();

/// Erase all seq occurrences in str
extern std::string erase_all(const std::string& str, const std::string& seq);

/// Remove .. subdirectories from path to prevent sandbox escape
extern std::string secure_path(const std::string& path);

/// Get parent directory name of entry at given path
extern std::string path_dirname(const std::string& path);

/// Get entry base name
extern std::string path_basename(std::string path);


#endif //WEBSERVER_TOOLS_H
