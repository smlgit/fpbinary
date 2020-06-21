#/bin/bash

# ==================================================================
# The following need to be installed for this script to run:
#     - pyenv
#     - for each pyenv python version installed, virtualenv


# Exit on error
set -e

# Commandline args
args=("$@")

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
    
    pyenv local $v
    virtualenv --clear venv$v
    source venv$v/bin/activate

    # Install fpbinary - method depends on cmdline args

    if [ "${args[0]}" = "appveyor" ]; then
    
	if [ ${#args[@]} -le 2 ]; then
	    echo "You must specify a build name for Appveyor"
	    (exit 1);
	fi

	echo "======================================================"
	echo "Installing from Appveyor build ${args[1]}"
	echo "======================================================"

	rm -rf appveyor_dload
	mkdir appveyor_dload

	# Need requests module
	pip install requests
	
	python release/download_build.py ${args[1]} appveyor_dload
	pip install fpbinary --no-cache-dir --no-index --find-links appveyor_dload
	rm -rf appveyor_dload
	
    elif [ "${args[0]}" = "pypi" ]; then
	
	# Install via pip
	if [ ${#args[@]} -le 1 ]; then
	    echo "You must specify either 'pip' or 'test' as the pypi install location."
	    (exit 1);
	fi

        location=${args[1]}
	
	# Install via pip
	if [ "$location" != "pip" ] && [ "$location" != "test" ]; then
	    echo "You must specify either 'pip' or 'test' as the pypi install location."
	    (exit 1);
	fi

	version=""
	if [ ${#args[@]} -ge 3 ]; then
	    version=${args[2]}
	fi
	
	if [ "$version" == "" ]; then
	    package="fpbinary"
	else
	    package="fpbinary==$version"
	fi
	
	echo "======================================================"
	echo "Installing $package via pypi site $location"
	echo "======================================================"
	
	if [ "$location" == "test" ]; then
	    pip install --no-cache-dir --index-url https://test.pypi.org/simple/ --no-deps $package
	else
	    pip install --no-cache-dir fpbinary $package
	fi
	
    else
	
	# Default to local repository build
        python setup.py install
	
    fi

    # Run fpbinary tests
    python -m unittest discover -s tests -p testFpBinary*

    # Deactivate virtualenv
    deactivate
    rm -rf venv$v
done

echo Success!

