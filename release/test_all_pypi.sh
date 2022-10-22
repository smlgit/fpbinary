#!/bin/bash

# ==================================================================
# The following need to be installed for this script to run:
#     - pyenv
#     - for each pyenv python version installed, virtualenv
# ==================================================================
#
# Will run the library tests on all pyenv versions by installing
# from either test pypi or pypi proper.
#
# Command syntax:
#    ./test_all_pypi.sh server_name [version]
#        where server_name is either test or pypi and
#        version is the version as shown on the server. If
#        not specified, will install the latest.
#
# The following need to be installed for this script to run:
#     - pyenv
#     - for each pyenv python version installed, virtualenv


# Exit on error
set -e

# Commandline args
args=("$@")

if [ ${#args[@]} -lt 1 ]; then
    echo "You must specify either 'pypi' or 'test' as the pypi install location."
    (exit 1);
fi

location=${args[0]}
if [ "$location" != "pypi" ] && [ "$location" != "test" ]; then
    echo "You must specify either 'pypi' or 'test' as the pypi install location."
    (exit 1);
fi

version=""
if [ ${#args[@]} -gt 1 ]; then
    version=${args[1]}
fi

if [ "$version" == "" ]; then
    package="fpbinary"
else
    package="fpbinary==$version"
fi


# Read pyenv versions installed
declare -a pyenv_versions
readarray -t pyenv_versions < <(pyenv versions --bare --skip-aliases)

# Remove build and dist dirs to force a clear install if compiling from source
rm -rf build dist

# For each pyenv version, create a virtualenv and run the install
for v in ${pyenv_versions[@]}; do
    
    echo "======================================================"
    echo $v
    echo "======================================================"
    
    eval "$(pyenv init -)"
    pyenv shell $v

    virtualenv --clear venv$v
    source venv$v/bin/activate

    # Need numpy for tests
    pip install numpy
    pip install scipy
	
    echo "======================================================"
    echo "Installing $package via pypi site $location"
    echo "======================================================"
	
    if [ "$location" == "test" ]; then
	pip install -I --pre --no-cache-dir --index-url https://test.pypi.org/simple/ --no-deps $package
    else
	pip install -I --pre --no-cache-dir --no-deps $package
    fi
	
    # Run fpbinary tests
    python -m unittest discover -s tests -p testFpBinary*

    # Deactivate virtualenv
    deactivate
    rm -rf venv$v
done


