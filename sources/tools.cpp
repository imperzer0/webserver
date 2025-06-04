// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <dmytroperets@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be approved by the author of this comment.

/// This file generally should contain functions that could be used
/// in any setting, not just this project
///
/// Ex: string operations, getcwd()...

#include "tools.h"

#include <ftw.h>
#include <pthread.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sys/stat.h>


//// Threads/Parallel execution ////

mutex_locker::mutex_locker(pthread_mutex_t* mutex) : mutex(mutex)
{
    pthread_mutex_lock(mutex);
}

mutex_locker::~mutex_locker()
{
    pthread_mutex_unlock(mutex);
}


//// Strings ////

bool starts_with(const char* str, const char* prefix)
{
    for (; *prefix; ++prefix, ++str)
        if (*prefix != *str)
            return false;
    return true;
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


//// Path/FS ////

std::string getcwd()
{
    char cwd[PATH_MAX]{};
    getcwd(cwd, PATH_MAX);
    cwd[PATH_MAX - 1] = 0;
    std::string cwdstr(cwd);
    return std::move(cwdstr);
}


bool rm_rf(const char* path)
{
    struct stat st{};
    if (stat(path, &st) < 0)
        return true;
    return nftw(path, [](const char* fpath, const struct stat*, int, struct FTW*) -> int
    {
        int rv = remove(fpath);
        if (rv) perror(fpath);
        return rv;
    }, 64, FTW_DEPTH | FTW_PHYS) == 0;
}


bool mkdir_p(const char* path)
{
    struct stat st{};
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return true;
        rm_rf(path);
    }

    char tmp[256]{};
    char* p = nullptr;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    return mkdir(tmp, S_IRWXU) == 0;
}

std::string secure_path(const std::string& path)
{
    std::string res = erase_all(path, "../"); // Erase ../
    return erase_all(res, "/.."); // Erase /..
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

std::string read_all(const std::string& file)
{
    FILE* f = fopen(file.c_str(), "rb");
    if (!f) return "";

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    std::string buf;
    buf.resize(len + 1);

    fread(buf.data(), 1, len, f);
    buf.data()[len] = '\0';
    fclose(f);
    return buf;
}
