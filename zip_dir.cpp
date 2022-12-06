// Copyright (c) 2022 Perets Dmytro
// Author: Perets Dmytro <imperator999mcpe@gmail.com>
//
// Personal usage is allowed only if this comment was not changed or deleted.
// Commercial usage must be agreed with the author of this comment.

#include "zip_dir.h"

#include <zip.h>
#include <stdexcept>
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>


static bool is_dir(const std::string& dir)
{
	struct stat st{ };
	::stat(dir.c_str(), &st);
	return S_ISDIR(st.st_mode);
}

static void walk_directory(const std::string& startdir, const std::string& inputdir, zip_t* zipper)
{
	DIR* dp = ::opendir(inputdir.c_str());
	if (dp == nullptr)
		throw std::runtime_error("Failed to open input directory: " + std::string(::strerror(errno)));
	
	struct dirent* dirp;
	while ((dirp = readdir(dp)) != nullptr)
	{
		if (dirp->d_name != std::string(".") && dirp->d_name != std::string(".."))
		{
			std::string fullname = inputdir + "/" + dirp->d_name;
			if (is_dir(fullname))
			{
				if (zip_dir_add(zipper, fullname.substr(startdir.length() + 1).c_str(), ZIP_FL_ENC_UTF_8) < 0)
				{
					throw std::runtime_error("Failed to add directory to zip: " + std::string(zip_strerror(zipper)));
				}
				walk_directory(startdir, fullname, zipper);
			}
			else
			{
				zip_source_t* source = zip_source_file(zipper, fullname.c_str(), 0, 0);
				if (source == nullptr)
				{
					throw std::runtime_error("Failed to add file to zip: " + std::string(zip_strerror(zipper)));
				}
				if (zip_file_add(zipper, fullname.substr(startdir.length() + 1).c_str(), source, ZIP_FL_ENC_UTF_8) < 0)
				{
					zip_source_free(source);
					throw std::runtime_error("Failed to add file to zip: " + std::string(zip_strerror(zipper)));
				}
			}
		}
	}
	::closedir(dp);
}

void zip_directory(const std::string& inputdir, const std::string& output_filename)
{
	int errorp;
	zip_t* zipper = zip_open(output_filename.c_str(), ZIP_CREATE | ZIP_EXCL, &errorp);
	if (zipper == nullptr)
	{
		zip_error_t ziperror;
		zip_error_init_with_code(&ziperror, errorp);
		throw std::runtime_error("Failed to open output file " + output_filename + ": " + zip_error_strerror(&ziperror));
	}
	
	try
	{
		walk_directory(inputdir, inputdir, zipper);
	}
	catch (...)
	{
		zip_close(zipper);
		throw;
	}
	
	zip_close(zipper);
}