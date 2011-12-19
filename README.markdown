PQWX - PostgreSQL desktop client
================================

PQWX is a desktop application that provides access to PostgreSQL
databases, like the psql tool does from the command line.

It is written in C++ and based on the WxWidgets library. This means it
works as a GTK2 application on Linux et al, but should also be
buildable as a native application on MacOS X and Windows.

PQWX is in early development, with little in the way of actual working
features. It can connect to databases and show some schema details in
its Object Browser, indexes schema objects for use in a "conent
assist"-style quick finder, but not really anything more as yet.

Building
--------

PQWX builds on a Debian GNU/Linux system, and should build simply by
running "make" on any similar system.

* `wx-config` must be on the PATH to locate WxWidgets

  * `wxrc` must be on the PATH to compile WxWidgets xrc files. On
    Debian, install `wx-common` if necessary- this doesn't seem to be
    automatically pulled in.

* `pg_config` must be on the PATH to location libpq

* `perl` is used to pack some SQL used by the tool for catalogue exploration into a header file.

Usage
-----

If run with no arguments, PQWX will open its main window and a
"Connect To Server" dialogue. These connection properties are passed
to libpq, so environment variables and the `.pgpass` file are
automatically processed. So for many users, clicking "Connect" without
entering any parameters is actually sufficient.
