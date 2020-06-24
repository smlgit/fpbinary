import os, subprocess


def upload_to_test_pypi(password, files_dir):
    result = subprocess.run(['python', '-m', 'twine', 'upload', '--repository',
                             'testpypi', '{}/*'.format(files_dir.rstrip('/')),
                             '-u', '__token__', '-p', '{}'.format(password),
                             '--verbose'])

    return result == 0
