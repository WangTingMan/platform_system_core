#include <utils/direct.h>

#include <windows.h>
#include <base/strings/sys_string_conversions.h>

#include <filesystem>

#include <direct.h>

std::string GetCurrentDir()
{
    wchar_t exeFullPath[MAX_PATH]; // Full path
    std::wstring strPath;

    GetModuleFileName( NULL, exeFullPath, MAX_PATH );
    strPath = (std::wstring)exeFullPath;    // Get full path of the file
    int pos = strPath.find_last_of( '\\', strPath.length() );
    std::filesystem::path pth( strPath.substr( 0, pos ) );
    std::string ret = pth.generic_string();
    ret.push_back( '\\' );
    return ret;  // Return the directory without the file name
}

int mkdir( const char* path, mode_t mode )
{
    return _mkdir( path );
}

DIR* opendir( const wchar_t* name)
{
    std::wstring dir_name( name );
    std::string dir = base::SysWideToNativeMB( dir_name );
    return opendir( dir.c_str() );
}
