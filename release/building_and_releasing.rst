Building
========

fpbinary is simple to build locally using:

.. code-block:: bash

    python setup.py install

setuptools picks up the right compiler/linker tools for the local machine and runs them and places the object file in the right place.

Because Linux systems usually have a compiler pre installed, I only do a source distribution for Linux. It should just work.

Building for platforms like Windows and macOS is more complicated because a user ideally wouldn't need to find and install a compiler.
This is particularly important for Windows because Visual Studio is the recommeded compiler to use but it is
pretty bloaty and not installed by default. Also, you need the correct version of the VS C++ compiler.

The chosen solution is to use an online service to build and test the fpbinary library. We are currently using Appveyor.

Appveyor
--------

I have an account at `https://ci.appveyor.com/`_ with username *smlgit*. Appveyor builds Linux, Windows and macOS.

Appveyor has a decent REST API that the python scripts in /release use to start builds and download archive files.

A Github authorization has been granted to Appveyor for fpbinary for web hook access. This can be revoked at either
Github or Appveyor.

An Appveyor API token has been generated on the *smlgit->My Profile->API Keys* page for access via the REST API. A new
token can be generated at any time.

The Appveyor build is controlled via the appveyor.yml file in the repo root. Note that web hooks *are* required for this
to work.
