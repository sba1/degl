The degl tool
=============

Introduction
------------

The tool decl is a simple refactoring tool based on libclang that takes
C sources as input and replaces every access to a global variable with
an access to a field of a dedicated context structure that is passed to
each of the functions. It's purpse is hence to eliminate global
variables. This is, for instance, interesting when porting standard
programming libraries that extensively use global variables and assume
that they are not shared to AmigaOS shared libraries.

Copyright
---------

The program degl is written by Sebastian Bauer and released under the
terms of the AGPL. See the License file for more details.

Usage
-----

This tool is in a very experimental state. It is not yet possible to use
it.

Contact
-------

You can reach the author by writting an email to
 mail@sebastianbauer.info 

How to build
------------

Invoke the makefile. Note that llvm paths are hardcoded for now. You
need a relatively recent version of libclang, e.g., version 3.5.
