/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#include <io.h>
#include <time.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
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
        RaiseExceptionC(R.Assertion, "delete-file", "unable to delete file", List(argv[0]));

    return(NoValueObject);
}

// ---- File System interface ----

Define("file-size", FileSizePrimitive)(int_t argc, FObject argv[])
{
// Function file-size filename
// Returns file size of filename in bytes. If filename does not exist, it raises &assertion condition.

    OneArgCheck("file-size", argc);
    StringArgCheck("file-size", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad)
            == 0 || (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        RaiseExceptionC(R.Assertion, "file-size", "not a file", List(argv[0]));
    return(MakeInteger(fad.nFileSizeHigh, fad.nFileSizeLow));
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(argv[0]);

    FAssert(BytevectorP(bv));

//    return(access((const char *) AsBytevector(bv)->Vector, 0) == 0 ? TrueObject : FalseObject);
    return(NoValueObject);
#endif // FOMENT_UNIX
}

Define("file-regular?", FileRegularPPrimitive)(int_t argc, FObject argv[])
{
// Function file-regular? filename

    OneArgCheck("file-regular?", argc);
    StringArgCheck("file-regular?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-regular?", "not a file or directory", List(argv[0]));
    return(((fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            (fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) ?
            TrueObject : FalseObject);
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

Define("file-directory?", FileDirectoryPPrimitive)(int_t argc, FObject argv[])
{
// Function file-directory? filename

    OneArgCheck("file-directory?", argc);
    StringArgCheck("file-directory?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-directory?", "not a file or directory", List(argv[0]));
    return((fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TrueObject : FalseObject);
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

Define("file-symbolic-link?", FileSymbolicLinkPPrimitive)(int_t argc, FObject argv[])
{
// Function file-symbolic-link? filename

    OneArgCheck("file-symbolic-link?", argc);
    StringArgCheck("file-symbolic-link?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-symbolic-link?", "not a file or directory",
                List(argv[0]));
    return((fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? TrueObject : FalseObject);
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

#ifdef FOMENT_UNIX
Define("file-readable?", FileReadablePPrimitive)(int_t argc, FObject argv[])
{
// Function file-readable? filename

    OneArgCheck("file-readable?", argc);
    StringArgCheck("file-readable?", argv[0]);

    return(NoValueObject);
}
#endif // FOMENT_UNIX

Define("file-writable?", FileWritablePPrimitive)(int_t argc, FObject argv[])
{
// Function file-writable? filename

    OneArgCheck("file-writable?", argc);
    StringArgCheck("file-writable?", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-writable?", "not a file or directory",
                List(argv[0]));
    return((fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? FalseObject : TrueObject);
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

#ifdef FOMENT_UNIX
Define("file-executable?", FileExecutablePPrimitive)(int_t argc, FObject argv[])
{
// Function file-executable? filename

    OneArgCheck("file-executable?", argc);
    StringArgCheck("file-executable?", argv[0]);

    return(NoValueObject);
}
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
Define("file-archive?", FileArchivePPrimitive)(int_t argc, FObject argv[])
{
// Function file-archive? filename

    OneArgCheck("file-archive?", argc);
    StringArgCheck("file-archive?", argv[0]);

    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-archive?", "not a file or directory", List(argv[0]));
    return((fad.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) ? FalseObject : TrueObject);
}

Define("file-system?", FileSystemPPrimitive)(int_t argc, FObject argv[])
{
// Function file-system? filename

    OneArgCheck("file-system?", argc);
    StringArgCheck("file-system?", argv[0]);

    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-system?", "not a file or directory", List(argv[0]));
    return((fad.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? FalseObject : TrueObject);
}

Define("file-hidden?", FileHiddenPPrimitive)(int_t argc, FObject argv[])
{
// Function file-hidden? filename

    OneArgCheck("file-hidden?", argc);
    StringArgCheck("file-hidden?", argv[0]);

    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-hidden?", "not a file or directory", List(argv[0]));
    return((fad.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? FalseObject : TrueObject);
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

Define("file-stat-ctime", FileStatCtimePrimitive)(int_t argc, FObject argv[])
{
// Function file-stat-ctime filename
// The file-stat-ctime procedure returns last change time of filename.

    OneArgCheck("file-stat-ctime", argc);
    StringArgCheck("file-stat-ctime", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-directory?", "not a file or directory", List(argv[0]));
    return(MakeIntegerU(ConvertTime(&fad.ftCreationTime)));
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

Define("file-stat-mtime", FileStatMtimePrimitive)(int_t argc, FObject argv[])
{
// Function file-stat-mtime filename
// Returns file statistics time in nano sec.
// The file-stat-mtime returns last modified time of filename.

    OneArgCheck("file-stat-mtime", argc);
    StringArgCheck("file-stat-mtime", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-directory?", "not a file or directory", List(argv[0]));
    return(MakeIntegerU(ConvertTime(&fad.ftLastWriteTime)));
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

Define("file-stat-atime", FileStatAtimePrimitive)(int_t argc, FObject argv[])
{
// Function file-stat-atime filename
// Returns file statistics time in nano sec.
// The file-stat-atime returns last accesse time of filename.

    OneArgCheck("file-stat-atime", argc);
    StringArgCheck("file-stat-atime", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (GetFileAttributesExW((FCh16 *) AsBytevector(bv)->Vector, GetFileExInfoStandard, &fad) == 0)
        RaiseExceptionC(R.Assertion, "file-directory?", "not a file or directory", List(argv[0]));
    return(MakeIntegerU(ConvertTime(&fad.ftLastAccessTime)));
#endif // FOMENT_WINDOWS
    return(NoValueObject);
}

Define("create-symbolic-link", CreateSymbolicLinkPrimitive)(int_t argc, FObject argv[])
{
// Function create-symbolic-link old-filename new-filename
// Creates symbolic link of old-filename as new-filename.

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
        RaiseExceptionC(R.Assertion, "create-symbolic-link", "not a file or directory",
                List(argv[1]));

    if (CreateSymbolicLinkW((FCh16 *) AsBytevector(bv2)->Vector,
            (FCh16 *) AsBytevector(bv1)->Vector,
            (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)
            == 0)
        RaiseExceptionC(R.Assertion, "create-symbolic-link", "unable to create symbolic link",
                List(argv[0], argv[1], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

    return(NoValueObject);
}

Define("rename-file", RenameFilePrimitive)(int_t argc, FObject argv[])
{
// Function rename-file old-filename new-filename
// Renames given old-filename to new-filename.
// If old-filename does not exist, it raises &assertion.
//
// If new-filename exists, it overwrite the existing file.

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
        RaiseExceptionC(R.Assertion, "rename-file", "unable to rename file",
                List(argv[0], argv[1], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

    return(NoValueObject);
}

Define("create-directory", CreateDirectoryPrimitive)(int_t argc, FObject argv[])
{
// Function create-directory path
// Creates given directory. If it fails, it raises condition &assertion.

    OneArgCheck("create-directory", argc);
    StringArgCheck("create-directory", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    if (CreateDirectoryW((FCh16 *) AsBytevector(bv)->Vector, 0) == 0)
        RaiseExceptionC(R.Assertion, "create-directory", "unable to create directory",
                List(argv[0], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

    return(NoValueObject);
}

Define("delete-directory", DeleteDirectoryPrimitive)(int_t argc, FObject argv[])
{
// Function delete-directory path
// Creates/deletes given directory. If it fails, it raises condition &assertion.

    OneArgCheck("delete-directory", argc);
    StringArgCheck("delete-directory", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);

    FAssert(BytevectorP(bv));

    if (RemoveDirectoryW((FCh16 *) AsBytevector(bv)->Vector) == 0)
        RaiseExceptionC(R.Assertion, "delete-directory", "unable to delete directory",
                List(argv[0], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

    return(NoValueObject);
}

Define("list-directory", ListDirectoryPrimitive)(int_t argc, FObject argv[])
{
// Function list-directory path

    OneArgCheck("list-directory", argc);
    StringArgCheck("list-directory", argv[0]);

#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(argv[0]);
    FObject lst = EmptyListObject;

    FAssert(BytevectorP(bv));

    WIN32_FIND_DATAW wfd;
    HANDLE dh = FindFirstFileW((FCh16 *) AsBytevector(bv)->Vector, &wfd);
    if (dh == INVALID_HANDLE_VALUE)
        RaiseExceptionC(R.Assertion, "list-directory", "unable to list directory",
                List(argv[0], MakeIntegerU(GetLastError())));
    do
    {
        lst = MakePair(ConvertUtf16ToString(wfd.cFileName, lstrlenW(wfd.cFileName)), lst);
    }
    while (FindNextFileW(dh, &wfd) != 0);

    FindClose(dh);

    return(lst);
#endif // FOMENT_WINDOWS
}

Define("copy-file", CopyFilePrimitive)(int_t argc, FObject argv[])
{
// Function copy-file src dst :optional overwrite
// src and dst must be string and indicating existing file path.
// Copies given src file to dst and returns #t if it's copied otherwise #f.
//
// If optional argument overwrite is #t then it will over write the file even if it exists.

    TwoOrThreeArgsCheck("copy-file", argc);
    StringArgCheck("copy-file", argv[0]);
    StringArgCheck("copy-file", argv[1]);

#ifdef FOMENT_WINDOWS
    FObject bv1 = ConvertStringToUtf16(argv[0]);
    FObject bv2 = ConvertStringToUtf16(argv[1]);

    FAssert(BytevectorP(bv1));
    FAssert(BytevectorP(bv2));

    if (CopyFileExW((FCh16 *) AsBytevector(bv1)->Vector, (FCh16 *) AsBytevector(bv2)->Vector,
            0, 0, 0, (argc == 3 && argv[2] == TrueObject) ? 0 : COPY_FILE_FAIL_IF_EXISTS) == 0)
        return(FalseObject);
#endif // FOMENT_WINDOWS

    return(TrueObject);
}

Define("current-directory", CurrentDirectoryPrimitive)(int_t argc, FObject argv[])
{
// Function current-directory :optional path
// Returns current working directory.
// If optional argument path is given, the current-directory sets current working directory to path and returns unspecified value.

    ZeroOrOneArgsCheck("current-directory", argc);
    if (argc == 1)
    {
        StringArgCheck("current-directory", argv[0]);

#ifdef FOMENT_WINDOWS
        FObject bv = ConvertStringToUtf16(argv[0]);

        FAssert(BytevectorP(bv));

        if (SetCurrentDirectoryW((FCh16 *) AsBytevector(bv)->Vector) == 0)
            RaiseExceptionC(R.Assertion, "current-directory", "unable to set current directory",
                    List(argv[0], MakeIntegerU(GetLastError())));
#endif // FOMENT_WINDOWS

        return(NoValueObject);
    }

#ifdef FOMENT_WINDOWS
    DWORD sz = GetCurrentDirectoryW(0, 0);

    FAssert(sz > 0);

    FObject b = MakeBytevector(sz * sizeof(FCh16));
    sz = GetCurrentDirectoryW(sz, (FCh16 *) AsBytevector(b)->Vector);

    return(ConvertUtf16ToString((FCh16 *) AsBytevector(b)->Vector, sz));
#endif // FOMENT_WINDOWS
}

static FPrimitive * Primitives[] =
{
    &FileExistsPPrimitive,
    &DeleteFilePrimitive,
    &FileSizePrimitive,
    &FileRegularPPrimitive,
    &FileDirectoryPPrimitive,
    &FileSymbolicLinkPPrimitive,
#ifdef FOMENT_UNIX
    &FileReadablePPrimitive,
#endif // FOMENT_UNIX
    &FileWritablePPrimitive,
#ifdef FOMENT_UNIX
    &FileExecutablePPrimitive,
#endif // FOMENT_UNIX
#ifdef FOMENT_WINDOWS
    &FileArchivePPrimitive,
    &FileSystemPPrimitive,
    &FileHiddenPPrimitive,
#endif // FOMENT_WINDOWS
    &FileStatCtimePrimitive,
    &FileStatMtimePrimitive,
    &FileStatAtimePrimitive,
    &CreateSymbolicLinkPrimitive,
    &RenameFilePrimitive,
    &CreateDirectoryPrimitive,
    &DeleteDirectoryPrimitive,
    &ListDirectoryPrimitive,
    &CopyFilePrimitive,
    &CurrentDirectoryPrimitive
};

void SetupFileSys()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}