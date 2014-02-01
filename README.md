conkeror-spawn-helper-win32
==========================

This is a mostly complete port of conkeror-spawn-helper to Windows environment. It uses [lcc-win32](http://www.cs.virginia.edu/~lcc-win32/) compiler to generate a windows executable that can be used to start applications from within [Conkeror browser](http://conkeror.org/) - most notably, an external editor.

Installation
------------

1. Install lcc
2. Execute lcc's make inside lcc directory
3. Copy generated conkeror-spawn-helper.exe to your conkeror directory
4. Modify conkeror's modules/spawn-process.js by applying the supplied diff file in _patches/spawn-process.js.diff_
5. Restart conkeror

_Note_: The code is mostly portable C with Win32 definitions. You should be able to use a different compiler to build it, including MS Visual C++. I chose lcc for its minimal impact on the system.

Downloading
-----------

You can download a [pre-compiled binary](https://drive.google.com/folderview?id=0B1Aic_7xzqoJVHZlQUtCcjFvb0U). It doesn't contain any external libraries it may need.

Tips
----

### Opening the deafult program for a file type

If you want to run the default program for a particular file type, after clicking or selecting the file's link, choose *o* (open) as the action, then type *start /wait "a"* as the external program.

Licensing
---------

Since conkeror uses a Mozilla Public License, this port is also published under the same license.
