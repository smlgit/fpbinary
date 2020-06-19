import os, configparser


def get_security_config_file_path():
    if (os.path.exists('security.ini')):
        return os.path.abspath('security.ini')

    if (os.path.exists('release/security.ini')):
        return os.path.abspath('release/security.ini')

    return None


def get_fpbinary_version_file_path():
    # Find version file
    if (os.path.exists('VERSION')):
        return os.path.abspath('VERSION')

    if (os.path.exists('../VERSION')):
        return os.path.abspath('../VERSION')

    return None


def get_fpbinary_version():
    result = None
    fpath = get_fpbinary_version_file_path()

    if fpath is not None:
        with open(fpath, mode='r') as f:
            result = f.readline()

    return result


def get_appveyor_security():
    security_file = get_security_config_file_path()

    if security_file is not None:
        config = configparser.ConfigParser()
        config.read(security_file)

        return config['APPVEYOR']

    return None


def concat_urls(urls):

    result = ''

    for url in urls:
        result += url.strip('/') + '/'

    return result.strip('/')
