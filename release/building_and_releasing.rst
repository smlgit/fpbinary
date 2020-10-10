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
`This <https://wiki.python.org/moin/WindowsCompilers>`_ page is very helpful re the compiler versions required for
windows builds. I currently only build for >= Python 3.5 because the required older versions of the VS C++ compiler don't
fully support C99. Specifically, it craps out on use of `<stdboo.h>` and the dot notation in struct definitions. We
could use the MinGW compiler for older versions, but that is a low priority job.

Appveyor
--------

I have an account at `<https://ci.appveyor.com>`_ with username *smlgit*. Appveyor builds Linux, Windows and macOS.

Appveyor has a decent REST API that the python scripts in /release use to start builds and download archive files.

A Github authorization has been granted to Appveyor for fpbinary for web hook access. This can be revoked at either
Github or Appveyor.

An Appveyor API token has been generated on the *smlgit->My Profile->API Keys* page for access via the REST API. A new
token can be generated at any time.

The Appveyor build is controlled via the appveyor.yml file in the repo root. Note that web hooks **are** required for this
to work. The only setting that is really used *outside* the .yml file or REST API is the 'Next build number' setting in
fpbinary settings on the Appveyor website. This number is untouched and is incremented every build automatically.

Powershell scripting is used in the .yml file whenever flow needs to be controlled (Powershell is the only
scripting available in the .yml for **every** OS. 'cmd:' lines run Windows-only commands.

The cibuildwheel tool is used for building windows and macOS wheels because it installs the Python runtimes needed for
supported of older macOS versions and it was cleaner to also use it for Windows. Because we only do a source dist for
Linux, it has its own code.

The run_build.py makes it easy to start a build for a branch and download the resultant files. It can also upload the
files to test.pypi for testing the release. A build can also be started in the web interface, but you'll need to set the
branch in the project settings page. An environment variable also needs to be set if it is a release build, so best to
stick with the script.

Build names and release numbers
-------------------------------

It seems that semantic versioning has become big and most python packages seem to use it. So I decided to use
a MAJOR.MINOR.PATCH format for released packages. The patch number is incremented after each release (in the
VERSION file manually). So packages that are released to the public have no build number information in them.

I want development builds to have build number info in them for easy distinguishing. But note that PEP 440
is rather strict on the formats of public packages, which test.pypi adheres to. I could have used
MAJOR.MINOR.PATCH.BUILD_NUMBER but that makes the final release the oldest version among same patch builds.
The only possible alternative was to use the MAJOR.MINOR.PATCHaBUILD_NUMER 'alpha' format.

A package version number is set in setup.py in the setup function:

.. code-block:: python

    setup(name='fpbinary', version=version, ...)

The build number comes from Appveyor. In order to get this information into setup.py, the build process can
write to a file called 'ALPHA' in the root of fpbinary. The text written will be appended to the MAJOR.MINOR.PATCH
obtained from the 'VERSION' file. See setup.py. Note that this is only done on a development (non-release)
build. On a release build, the `is_release_build` env variable must be created (and set to anything) in Appveyor.
This is easy to do via the REST API. This variable prevents the Appveyor yaml from creating the 'ALPHA' file.

There is also code in appveyor.yml that sets the build name (or version as Appveyor calls it). I've made all
build names have the build number in it. See the comment in release.lib.common.get_version_from_appveyor_build_name()
for the build name format I use.

The run_build.py script has an option to upload binaries/source to test.pypi and install from there during
the Appveyor build. Note that while test.pypi.org is useful for testing a new package, it (ridiculously)
prevents you from uploading the same file (by name) twice for a given version (forever - you can't even remove a
release and redo it). But it does recognize the alpha release format, so it isn't a problem unless a
*release* build is uploaded and you need to re-do it. In that case, you would have to increment the PATCH number
if you wanted to upload to the test server with another release build. This applies also to the online pypi.org server
too. So best to do a non-release build with upload to the test server, make sure it works and then do the same with the
release build (without any code changes).

In order to upload to test.pypi, you need a password token. The Appveyor yaml file has an *encrypted* version of the
test.pypi API token. The mechanism used is called a 'secure variable'. A secure variable can be created in the
*Account->Encrypt YAML* page.

Test PYPI and PYPI
------------------

test.pypi.org is a clone of pypi.org and allows you to test your release file uploads before uploading to the real
pypi.org server. I have an account with username *smlgit*. Access isn't via an API. We just use `twine` to upload to the
server as we would to pypi.org, but with a url option. Both test.pypi.org and pypi.org require an API token that must be
passed in via the `twine -p` option. The tokens are generated on the *Settings* page of the fpbinary project. Note that
test.pypi.org and pypi.org have their own distinct settings, they don't share these credentials.

Github
------

Aside from the usual operations on Github, the only thing we do on Github for release is to generate a tag for a public
release. This is done in the release.py script via the Github API. This needs an access token. I've used a *Personal
Access Token*. These are lightweight tokens that are used for authorization over the https REST API. They are generated
on the *Profile Picture->Settings->Developer Settings->Personal Access Tokens* page.

Security File
-------------

For the release/build scripts to get access to the various online services, a file named *security.json* file must be
placed in the release directory with the following structure:

.. code-block:: python

    {
        "APPVEYOR": {"token": <appveyor-token>, "account": "smlgit"},
        "TESTPYPI": {"token": <testpypi-token>},
        "PYPI": {"token": <pypi-token>},
        "GITHUB": {"token": <github-personal-access-token>}
    }

Releasing
=========

A release comprises the following steps:

#. Make sure the MAJOR.MINOR.PATCH version is set correctly in the VERSION file
#. Make sure CHANGELOG.rst is updated with the new release enhancements and fixes
#. Do a non-release build, preferably with installation from test.pypi.org, so that tests are run on all possible platforms:

   .. code-block:: bash

       python release/run_build.py --install-from-testpypi <branch>

#. If everything passes, do the same as a release build:

   .. code-block:: bash

       python release/run_build.py --install-from-testpypi --release <branch>

   The only difference here is that the version in the packaging info won't have an `a<build-number>` appendage.

#. If everything is ok, run the release script:

   .. code-block:: bash

       python release/release.py <appveyor-build-name>

   This should download the package files from Appveyor, upload them to pypi.org, run the `test_all_pypi.sh` script
   (which just tests that you can install the package in virtualenvs of each pyenv version on the local PC) and finally
   creates a release tag on the commit that Appveyor reports the build was done on.

.. note::

    * This process can be done on any branch but we should be releasing off of the master branch

Documentation
=============

readthedocs
-----------

We have a readthedocs account under the username *smlgit*.

.rst files are used to add documentation for the library that readthedocs can build to produce
`<https://fpbinary.readthedocs.io/en/latest>`_. The .rst files are in the `doc` directory. The html files that will be
produced by readthedocs can be generated locally (after `sphinx` and its `napoleon` and `autodoc` extensions are
installed) by running:

.. code-block:: bash

    make html

in the `doc` directory. The resultant html will located in the `_build/html` directory.

Currently, readthedocs will automatically re-build the docs whenever there is a commit on the master branch. This
requires Github web hook access to readthedocs.

help() docstrings
-----------------

The main documentation for fpbinary is written in the source code itself via docstrings. The format follows the numpy
documentation standard (as far as possible) (see `<https://numpydoc.readthedocs.io/en/latest/format.html>`_ ).

Not only does this give the user access to the documentation in the interpreter shell, but rst/html files are
generated from the interpreter help via the `sphinx` tool and the `autodoc` extension. This is done by readthedocs to
produce the page `<https://fpbinary.readthedocs.io/en/latest/objects.html>`_.





