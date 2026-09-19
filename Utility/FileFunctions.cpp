#include "FileFunctions.h"

bool isPathInsideGameFolder(const std::string &path)
{
    std::filesystem::path parts(path);
    if (parts.is_absolute() || parts.has_root_name() || parts.has_root_directory())
        return false;

    for (const std::filesystem::path& part : parts)
    {
        if (part == "..")
            return false;
    }

    return true;
}

bool okayFilePath(const std::string &path)
{
    //Greater than 2 characters
    //Not start with a . or /
    //Can only contain a-z A-Z 0-9 _ . /
    //Can not have a . or / as the first character
    //Can only have one .
    //Can not be longer than 48 characters

    if (path.length() < 2)
        return false;

    if (path.length() > 64)
        return false;

    if (path[0] == '.')
        return false;

    if (path[0] == '/')
        return false;

    if (path[0] == '\\')
        return false;

    //This is a redundant safety check since we should be explicitly checking if file is actually a valid content file elsewhere i.e. wav/png/bls
    if (path.find(".exe") != std::string::npos)
        return false;

    if (path.find(".dll") != std::string::npos)
        return false;

    bool onePeriod = false;

    for (int i = 0; path[i]; i++)
    {
        if (path[i] >= 'a' && path[i] <= 'z')
            continue;
        if (path[i] >= 'A' && path[i] <= 'Z')
            continue;
        if (path[i] >= '0' && path[i] <= '9')
            continue;
        if (path[i] == '/' || path[i] == '_')
            continue;
        if (path[i] == '-')
            continue;
        if (path[i] == '.')
        {
            if (onePeriod)
                return false;
            else
            {
                onePeriod = true;
                continue;
            }
        }

        return false;
    }

    return true;
}

unsigned int getFileChecksum(const char* filePath)
{
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return 0;

    const unsigned int bufSize = 2048;
    char buffer[bufSize];
    unsigned int ret = 0;
    bool firstRun = true;
    while (!file.eof())
    {
        file.read(buffer, bufSize);
        if (firstRun)
        {
            ret = CRC::Calculate(buffer, file.gcount(), CRC::CRC_32());
            firstRun = false;
        }
        else
            ret = CRC::Calculate(buffer, file.gcount(), CRC::CRC_32(), ret);
    }

    file.close();
    return ret;
}

unsigned int getBufferChecksum(const char* data, size_t length)
{
    if (!data || length < 1)
        return 0;

    return CRC::Calculate(data, length, CRC::CRC_32());
}

long GetFileSize(const std::string &filename)
{
    if (!doesFileExist(filename))
        return 0;
        
    
    std::ifstream in(filename.c_str(), std::ifstream::ate | std::ifstream::binary);
    return in.tellg(); 
    
    //struct stat stat_buf;
    //int rc = stat(filename.c_str(), &stat_buf);
    //return rc == 0 ? stat_buf.st_size : -1;
}

std::string getFileFromPath(const std::string &in)
{
    /*while (in.find("/") != std::string::npos)
        in = in.substr(in.find("/") + 1, (in.length() - in.find("/")) + 1);
    while (in.find("\\") != std::string::npos)
        in = in.substr(in.find("\\") + 1, (in.length() - in.find("\\")) + 1);
    return in;*/

    return std::filesystem::path(in).filename().string();
}

std::string getFolderFromPath(const std::string &in)
{
    /*if (in.find("/") == std::string::npos)
        return "";
    std::string file = getFileFromPath(in);
    if (in.length() > file.length())
        in = in.substr(0, in.length() - file.length());
    return in;*/

    std::string ret = std::filesystem::path(in).parent_path().string();

    //Make sure we end with a slash, for consistancy
    if (ret.length() < 1 || ret[ret.length() - 1] != '/')
        ret += "/";

    return ret;
}

std::string addSuffixToFile(std::string in, std::string suffix)
{
    size_t lastPeriod = in.find(".", 0);
    while (in.find(".", lastPeriod + 1) != std::string::npos)
        lastPeriod = in.find(".", lastPeriod + 1);
    return in.substr(0, lastPeriod) + suffix + in.substr(lastPeriod, in.length() - lastPeriod);
}

bool doesFileExist(const std::string &filePath)
{
    std::ifstream test(filePath.c_str());
    if (test.is_open())
    {
        test.close();
        return true;
    }
    return false;
}
