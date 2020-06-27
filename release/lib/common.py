import os, json, re


def get_version_from_appveyor_build_name(build_name):
    # For release builds, build name is:
    #    branch_name-<base-version>rc<build_number>
    # and need to return:
    #    <base-version>
    #
    # For non-release builds, build name is:
    #    branch_name-<base-version>a<build_number>
    # and need to return:
    #    <base-version>a<build_number>

    if re.match('^.+-[0-9].[0-9].[0-9]rc[0-9]+$', build_name):
        # Release build
        return re.sub('rc[0-9]+$', '', re.sub('^.+-', '', build_name))
    elif re.match('^.+-[0-9].[0-9].[0-9]a[0-9]+$', build_name):
        # Non-release build
        return re.sub('^.+-', '', build_name)

    return None


def get_security_config_file_path():
    if (os.path.exists('security.json')):
        return os.path.abspath('security.json')

    if (os.path.exists('release/security.json')):
        return os.path.abspath('release/security.json')

    return None


def get_appveyor_security():
    security_file = get_security_config_file_path()

    if security_file is not None:
        with open(security_file, 'r') as f:
            config = json.load(f)

        return config['APPVEYOR']

    return None


def get_testpypi_security():
    security_file = get_security_config_file_path()

    if security_file is not None:
        with open(security_file, 'r') as f:
            config = json.load(f)

        return config['TESTPYPI']

    return None


def get_pypi_security():
    security_file = get_security_config_file_path()

    if security_file is not None:
        with open(security_file, 'r') as f:
            config = json.load(f)

        return config['PYPI']

    return None


def get_github_security():
    security_file = get_security_config_file_path()

    if security_file is not None:
        with open(security_file, 'r') as f:
            config = json.load(f)

        return config['GITHUB']

    return None


def concat_urls(urls):

    result = ''

    for url in urls:
        result += url.strip('/') + '/'

    return result.strip('/')
