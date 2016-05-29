/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#include <io.h>
#include <time.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif // FOMENT_UNIX

#include <stdio.h>
#include "foment.hpp"
#include "unicode.hpp"

// ---- System interface ----

Define("file-exists?", FileExistsPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-exists?", argc);
    StringArgCheck("file-exists?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    return(_waccess((FCh16 *) AsBytevector(bv)->Vector, 0) == 0 ? TrueObject : FalseObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    return(access((const char *) AsBytevector(bv)->Vector, 0) == 0 ? TrueObject : FalseObject);
#endif // FOMENT_UNIX
}

Define("delete-file", DeleteFilePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("delete-file", argc);
    StringArgCheck("delete-file", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    if (_wremove((FCh16 *) AsBytevector(bv)->Vector) != 0)
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    if (remove((const char *) AsBytevector(bv)->Vector) != 0)
#endif // FOMENT_UNIX
        RaiseExceptionC(Assertion, "delete-file", FileErrorSymbol, "unable to delete file",
                List(argv[0]));

    return(NoValueObject);
}

// ---- File System interface ----

Define("file-size", FileSizePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-size", argc);
    StringArgCheck("file-size", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad)
            == 0 || (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        RaiseExceptionC(Assertion, "file-size", FileErrorSymbol, "not a file", List(argv[0]));
    return(MakeInteger(fad.nFileSizeHigh, fad.nFileSizeLow));
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    struct stat st;

    if (stat((const char *) AsBytevector(bv)->Vector, &st) != 0 || S_ISREG(st.st_mode) == 0)
        RaiseExceptionC(Assertion, "file-size", FileErrorSymbol, "not a file", List(argv[0]));

    return(MakeIntegerU(st.st_size));
#endif // FOMENT_UNIX
}

Define("file-regular?", FileRegularPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-regular?", argc);
    StringArgCheck("file-regular?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) ||
            (attr & FILE_ATTRIBUTE_REPARSE_POINT) ? FalseObject : TrueObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    struct stat st;

    if (stat((const char *) AsBytevector(bv)->Vector, &st) != 0)
        return(FalseObject);

    return(S_ISREG(st.st_mode) ? TrueObject : FalseObject);
#endif // FOMENT_UNIX
}

Define("file-directory?", FileDirectoryPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-directory?", argc);
    StringArgCheck("file-directory?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ? TrueObject
            : FalseObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    struct stat st;

    if (stat((const char *) AsBytevector(bv)->Vector, &st) != 0)
        return(FalseObject);

    return(S_ISDIR(st.st_mode) ? TrueObject : FalseObject);
#endif // FOMENT_UNIX
}

Define("file-symbolic-link?", FileSymbolicLinkPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-symbolic-link?", argc);
    StringArgCheck("file-symbolic-link?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT) ? TrueObject
            : FalseObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    struct stat st;

    if (lstat((const char *) AsBytevector(bv)->Vector, &st) != 0)
        return(FalseObject);

    return(S_ISLNK(st.st_mode) ? TrueObject : FalseObject);
#endif // FOMENT_UNIX
}

Define("file-readable?", FileReadablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-readable?", argc);
    StringArgCheck("file-readable?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) ||
            (attr & FILE_ATTRIBUTE_REPARSE_POINT) ? FalseObject : TrueObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    return(access((const char *) AsBytevector(bv)->Vector, R_OK) == 0 ? TrueObject : FalseObject);
#endif // FOMENT_UNIX
}

Define("file-writable?", FileWritablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-writable?", argc);
    StringArgCheck("file-writable?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_READONLY) ?
            FalseObject : TrueObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    return(access((const char *) AsBytevector(bv)->Vector, W_OK | F_OK) == 0 ? TrueObject :
            FalseObject);
#endif // FOMENT_UNIX
}

#ifdef FOMENT_UNIX
Define("file-executable?", FileExecutablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-executable?", argc);
    StringArgCheck("file-executable?", argv[0]);

    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    return(access((const char *) AsBytevector(bv)->Vector, X_OK) == 0 ? TrueObject : FalseObject);
}
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
Define("file-archive?", FileArchivePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-archive?", argc);
    StringArgCheck("file-archive?", argv[0]);

    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr != INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_ARCHIVE) ? TrueObject
            : FalseObject);
}

Define("file-system?", FileSystemPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-system?", argc);
    StringArgCheck("file-system?", argv[0]);

    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr != INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_SYSTEM) ? TrueObject :
            FalseObject);
}

Define("file-hidden?", FileHiddenPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-hidden?", argc);
    StringArgCheck("file-hidden?", argv[0]);

    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    DWORD attr = GetFileAttributesW((FCh16 *) AsBytevector(bv)->Vector);
    return(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_HIDDEN) ? TrueObject :
            FalseObject);
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_WINDOWS
static __time64_t ConvertTime(FILETIME * ft)
{
    SYSTEMTIME st;
    struct tm tm;

    FileTimeToSystemTime(ft, &st);

    memset(&tm, 0, sizeof(struct tm));
    tm.tm_mday = st.wDay;
    tm.tm_mon  = st.wMonth - 1;
    tm.tm_year = st.wYear - 1900;
    tm.tm_sec  = st.wSecond;
    tm.tm_min  = st.wMinute;
    tm.tm_hour = st.wHour;

    return(_mktime64(&tm));
}
#endif // FOMENT_WINDOWS

Define("file-stat-mtime", FileStatMtimePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-stat-mtime", argc);
    StringArgCheck("file-stat-mtime", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(Assertion, "file-stat-mtime", FileErrorSymbol,
                "not a file or directory", List(argv[0]));
    return(MakeIntegerU(ConvertTime(&fad.ftLastWriteTime)));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    struct stat st;

    if (stat((const char *) AsBytevector(bv)->Vector, &st) != 0)
        RaiseExceptionC(Assertion, "file-stat-mtime", FileErrorSymbol,
                "not a file or directory", List(argv[0]));

    return(MakeIntegerU(st.st_mtime));
#endif // FOMENT_UNIX
}

Define("file-stat-atime", FileStatAtimePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("file-stat-atime", argc);
    StringArgCheck("file-stat-atime", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(Assertion, "file-stat-atime", FileErrorSymbol,
                "not a file or directory", List(argv[0]));
    return(MakeIntegerU(ConvertTime(&fad.ftLastAccessTime)));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    struct stat st;

    if (stat((const char *) AsBytevector(bv)->Vector, &st) != 0)
        RaiseExceptionC(Assertion, "file-stat-atime", FileErrorSymbol,
                "not a file or directory", List(argv[0]));

    return(MakeIntegerU(st.st_atime));
#endif // FOMENT_UNIX
}

Define("create-symbolic-link", CreateSymbolicLinkPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("create-symbolic-link", argc);
    StringArgCheck("create-symbolic-link", argv[0]);
    StringArgCheck("create-symbolic-link", argv[1]);

#ifdef FOMENT_WINDOWS
    FObject bv1 = ConvertStringToUtf16(argv[0]);
    FObject bv2 = ConvertStringToUtf16(argv[1]);

    FAssert(BytevectorP(bv1));
    FAssert(BytevectorP(bv2));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv1)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(Assertion, "create-symbolic-link", FileErrorSymbol,
                "not a file or directory", List(argv[1]));

    if (CreateSymbolicLinkW((FCh16 *) AsBytevector(bv2)->Vector,
            (FCh16 *) AsBytevector(bv1)->Vector,
            (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)
            == 0)
        RaiseExceptionC(Assertion, "create-symbolic-link", FileErrorSymbol,
                "unable to create symbolic link", List(argv[0], argv[1],
                MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv1 = ConvertStringToUtf8(argv[0]);
    FObject bv2 = ConvertStringToUtf8(argv[1]);

    FAssert(BytevectorP(bv1));
    FAssert(BytevectorP(bv2));

    if (symlink((const char *) AsBytevector(bv1)->Vector, (const char *) AsBytevector(bv2)->Vector)
            != 0)
        RaiseExceptionC(Assertion, "create-symbolic-link", FileErrorSymbol,
                "unable to create symbolic link", List(argv[0], argv[1], MakeFixnum(errno)));
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("rename-file", RenameFilePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("rename-file", argc);
    StringArgCheck("rename-file", argv[0]);
    StringArgCheck("rename-file", argv[1]);

#ifdef FOMENT_WINDOWS
    FObject bv1 = ConvertStringToUtf16(argv[0]);
    FObject bv2 = ConvertStringToUtf16(argv[1]);

    FAssert(BytevectorP(bv1));
    FAssert(BytevectorP(bv2));

    if (MoveFileExW((FCh16 *) AsBytevector(bv1)->Vector, (FCh16 *) AsBytevector(bv2)->Vector,
            MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) == 0)
        RaiseExceptionC(Assertion, "rename-file", FileErrorSymbol, "unable to rename file",
                List(argv[0], argv[1], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv1 = ConvertStringToUtf8(argv[0]);
    FObject bv2 = ConvertStringToUtf8(argv[1]);

    FAssert(BytevectorP(bv1));
    FAssert(BytevectorP(bv2));

    if (rename((const char *) AsBytevector(bv1)->Vector, (const char *) AsBytevector(bv2)->Vector)
            != 0)
        RaiseExceptionC(Assertion, "rename-file", FileErrorSymbol, "unable to rename file",
                List(argv[0], argv[1], MakeFixnum(errno)));
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("create-directory", CreateDirectoryPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("create-directory", argc);
    StringArgCheck("create-directory", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    if (CreateDirectoryW((FCh16 *) AsBytevector(bv)->Vector, 0) == 0)
        RaiseExceptionC(Assertion, "create-directory", FileErrorSymbol,
                "unable to create directory", List(argv[0], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    if (mkdir((const char *) AsBytevector(bv)->Vector, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
        RaiseExceptionC(Assertion, "create-directory", FileErrorSymbol,
                "unable to create directory", List(argv[0], MakeFixnum(errno)));
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("delete-directory", DeleteDirectoryPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("delete-directory", argc);
    StringArgCheck("delete-directory", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    if (RemoveDirectoryW((FCh16 *) AsBytevector(bv)->Vector) == 0)
        RaiseExceptionC(Assertion, "delete-directory", FileErrorSymbol,
                "unable to delete directory", List(argv[0], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

    if (rmdir((const char *) AsBytevector(bv)->Vector) != 0)
        RaiseExceptionC(Assertion, "delete-directory", FileErrorSymbol,
                "unable to delete directory", List(argv[0], MakeFixnum(errno)));
#endif // FOMENT_UNIX
    return(NoValueObject);
}

Define("list-directory", ListDirectoryPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("list-directory", argc);
    StringArgCheck("list-directory", argv[0]);

#ifdef FOMENT_WINDOWS
    uint_t sl = StringLength(argv[0]);
    FObject bv = ConvertStringToUtf16(AsString(argv[0])->String, sl, 1, 2);
    FObject lst = EmptyListObject;

    FAssert(BytevectorP(bv));

    FCh16 * us = (FCh16 *) AsBytevector(bv)->Vector;
    uint_t usl = lstrlenW(us);
    if (AsString(argv[0])->String[sl - 1] == '\\' || AsString(argv[0])->String[sl - 1] == '/')
    {
        us[usl] = '*';
        us[usl + 1] = 0;
    }
    else
    {
        us[usl] = '\\';
        us[usl + 1] = '*';
        us[usl + 2] = 0;
    }

    WIN32_FIND_DATAW wfd;
    HANDLE dh = FindFirstFileW(us, &wfd);
    if (dh == INVALID_HANDLE_VALUE)
        RaiseExceptionC(Assertion, "list-directory", FileErrorSymbol,
                "unable to list directory", List(argv[0], MakeIntegerU(GetLastError())));
    do
    {
        if ((wfd.cFileName[0] == '.' && (wfd.cFileName[1] == 0 ||
                (wfd.cFileName[1] == '.' && wfd.cFileName[2] == 0))) == 0)
            lst = MakePair(ConvertUtf16ToString(wfd.cFileName, lstrlenW(wfd.cFileName)), lst);
    }
    while (FindNextFileW(dh, &wfd) != 0);

    FindClose(dh);

    return(lst);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);
    FObject lst = EmptyListObject;

    FAssert(BytevectorP(bv));

    DIR * dh;
    dh = opendir((const char *) AsBytevector(bv)->Vector);
    if (dh == 0)
        RaiseExceptionC(Assertion, "list-directory", FileErrorSymbol,
                "unable to list directory", List(argv[0], MakeFixnum(errno)));
    for (;;)
    {
        struct dirent * de = readdir(dh);
        if (de == 0)
            break;
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
            lst = MakePair(ConvertUtf8ToString((FByte *) de->d_name, strlen(de->d_name)), lst);
    }
    closedir(dh);

    return(lst);
#endif // FOMENT_UNIX
}

Define("current-directory", CurrentDirectoryPrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("current-directory", argc);

    if (argc == 1)
    {
        StringArgCheck("current-directory", argv[0]);

#ifdef FOMENT_WINDOWS
        FObject bv = ConvertStringToUtf16(argv[0]);

        FAssert(BytevectorP(bv));

        if (SetCurrentDirectoryW((FCh16 *) AsBytevector(bv)->Vector) == 0)
            RaiseExceptionC(Assertion, "current-directory", FileErrorSymbol,
                    "unable to set current directory",
                    List(argv[0], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        FObject bv = ConvertStringToUtf8(argv[0]);

        FAssert(BytevectorP(bv));

        if (chdir((const char *) AsBytevector(bv)->Vector) != 0)
            RaiseExceptionC(Assertion, "current-directory", FileErrorSymbol,
                    "unable to set current directory", List(argv[0], MakeFixnum(errno)));
#endif // FOMENT_UNIX
        return(NoValueObject);
    }

#ifdef FOMENT_WINDOWS
    DWORD sz = GetCurrentDirectoryW(0, 0);

    FAssert(sz > 0);

    FObject b = MakeBytevector(sz * sizeof(FCh16));
    sz = GetCurrentDirectoryW(sz, (FCh16 *) AsBytevector(b)->Vector);

    return(ConvertUtf16ToString((FCh16 *) AsBytevector(b)->Vector, sz));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    char cd[MAXPATHLEN];
    getcwd(cd, MAXPATHLEN);
    return(ConvertUtf8ToString((FByte *) cd, strlen(cd)));
#endif // FOMENT_UNIX
}

static FPrimitive * Primitives[] =
{
    &FileExistsPPrimitive,
    &DeleteFilePrimitive,
    &FileSizePrimitive,
    &FileRegularPPrimitive,
    &FileDirectoryPPrimitive,
    &FileSymbolicLinkPPrimitive,
    &FileReadablePPrimitive,
    &FileWritablePPrimitive,
#ifdef FOMENT_UNIX
    &FileExecutablePPrimitive,
#endif // FOMENT_UNIX
#ifdef FOMENT_WINDOWS
    &FileArchivePPrimitive,
    &FileSystemPPrimitive,
    &FileHiddenPPrimitive,
#endif // FOMENT_WINDOWS
    &FileStatMtimePrimitive,
    &FileStatAtimePrimitive,
    &CreateSymbolicLinkPrimitive,
    &RenameFilePrimitive,
    &CreateDirectoryPrimitive,
    &DeleteDirectoryPrimitive,
    &ListDirectoryPrimitive,
    &CurrentDirectoryPrimitive
};

void SetupFileSys()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
