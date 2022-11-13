import subprocess


def upload_to_pypi_server(password, files_dir, server='testpypi'):
    """
    :param server: 'testpypi' or 'pypi'
    :return: True if successful
    """
    result = subprocess.run(['python', '-m', 'twine', 'upload', '--repository',
                             '{}'.format(server), '-u', '__token__', '-p',
                             '{}'.format(password), '--verbose',
                             '{}/*'.format(files_dir.rstrip('/'))],
                            check=True)

    return result == 0

