import os, json


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


def concat_urls(urls):

    result = ''

    for url in urls:
        result += url.strip('/') + '/'

    return result.strip('/')
