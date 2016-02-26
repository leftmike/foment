# File System API #

`(import (foment base))` to use these procedures.

## Windows and Unix ##

procedure: `(file-size` _filename_`)`

Return the size of _filename_ in bytes.
If _filename_ does not exist, an error that satifies `file-error?` is signaled.

procedure: `(file-regular?` _filename_`)`

Return `#t` if _filename_ is a regular file and `#f` otherwise.

procedure: `(file-directory?` _filename_`)`

Return `#t` if _filename_ is a directory and `#f` otherwise.

procedure: `(file-symbolic-link?` _filename_`)`

Return `#t` if _filename_ is a symbolic link and `#f` otherwise.

procedure: `(file-readable?` _filename_`)`

Return `#t` if _filename_ is readable and `#f` otherwise.

procedure: `(file-writable?` _filename_`)`

Return `#t` if _filename_ is writable and `#f` otherwise.

procedure: `(file-stat-mtime` _filename_`)`

Return when _filename_ was last modified in seconds since the beginning of time.
If _filename_ does not exist, an error that satifies `file-error?` is signaled.

procedure: `(file-stat-atime` _filename_`)`

Return when _filename_ was last accessed in seconds since the beginning of time.
If _filename_ does not exist, an error that satifies `file-error?` is signaled.

procedure: `(create-symbolic-link` _old\_path_ _new\_path_`)`

Create a symbolic link named _new\_path_ that points at _old\_path_.
If the operation fails, an error that satifies `file-error?` is signaled.

procedure: `(rename-file` _old\_path_ _new\_path_`)`

Rename file from _old\_path_ to _new\_path_, overwriting _new\_path_ if it already exists.
If the operation fails, an error that satifies `file-error?` is signaled.

procedure: `(create-directory` _path_`)`

Create a directory named _path_.
If the operation fails, an error that satifies `file-error?` is signaled.

procedure: `(delete-directory` _path_`)`

Delete the directory named _path_.
If the operation fails, an error that satifies `file-error?` is signaled.

procedure: `(list-directory` _path_`)`

Return the contents of the directory named _path_ as a list of strings, where each string is the
name of a file contained in the directory. `.` and `..` are not included in the list.
If the operation fails, an error that satifies `file-error?` is signaled.

procedure: `(current-directory)`
procedure: `(current-directory` _path_`)`

Without an argument, the current working directory is returned. With an argument, the current
working directory is set to _path_.
If the operation fails, an error that satifies `file-error?` is signaled.

## Windows Only ##

procedure: `(file-archive?` _filename_`)`

Return `#t` if _filename_ has the archive attribute and `#f` otherwise.

procedure: `(file-system?` _filename_`)`

Return `#t` if _filename_ has the system attribute and `#f` otherwise.

procedure: `(file-hidden?` _filename_`)`

Return `#t` if _filename_ has the hidden attribute and `#f` otherwise.

## Unix Only ##

procedure: `(file-executable?` _filename_`)`

Return `#t` if _filename_ is executable and `#f` otherwise.

