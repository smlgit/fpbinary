#!/bin/bash

# ==================================================================
#
# Will run the library tests on all pyenv versions by doing local
# builds.
#
# The following need to be installed for this script to run:
#     - pyenv
#     - for each pyenv python version installed, virtualenv


# Exit on error
set -e

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

    python setup.py install
    python -m unittest discover -s tests -p testFpBinary*

    # Deactivate virtualenv
    deactivate
    rm -rf venv$v
done


