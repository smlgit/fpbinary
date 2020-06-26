import argparse, logging, os, re, time, subprocess
from lib.appveyor import download_build_artifacts, get_build_from_name, build_is_successful, get_build_summary
from lib.pypi import upload_to_pypi_server
from lib.common import get_appveyor_security, get_pypi_security, get_version_from_appveyor_build_name


default_output_dir = os.path.abspath('download_dir')
pypi_upload_delay_minutes = 15


def main():
    logging.basicConfig()
    logging.root.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Will download build artifacts from Appveyor, '
                                                 'upload to PyPi, run install tests and add tag to GitHub.')
    parser.add_argument('buildname', type=str,
                        help='The build name/version of the Appveyor build to release.')

    args = parser.parse_args()

    version = get_version_from_appveyor_build_name(args.buildname)
    if version is None:
        raise ValueError('Couldn\'t parse build name {} to a valid version.'.format(args.buildname))

    appveyor_security_dict = get_appveyor_security()
    pypi_security_dict = get_pypi_security()

    build_dict = get_build_from_name(appveyor_security_dict['token'], appveyor_security_dict['account'],
                                     'fpbinary', args.buildname)

    if build_is_successful(build_dict) is False:
        raise ValueError('The build for version {} was not successful. Let\'s not release it...'.format(
            args.buildname
        ))

    logging.info('Releasing build:')
    logging.info(get_build_summary(build_dict))
    logging.info('Version: {}'.format(version))
    ans = input('Are you sure you want to continue? (y/n)')

    if ans != 'y':
        return

    # Download from Appveyor
    download_dir_abs = os.path.abspath(default_output_dir)
    download_build_artifacts(appveyor_security_dict['token'], appveyor_security_dict['account'],
                             'fpbinary', download_dir_abs, build_name=args.buildname)

    # Upload to PyPi

    logging.info('Uploading to PyPi...')
    upload_to_pypi_server(pypi_security_dict['token'], download_dir_abs, server='pypi')

    logging.info('Sleeping to give PyPi time to get sorted...')
    time.sleep(pypi_upload_delay_minutes * 60)

    # Run test scripts
    test_script_abs = os.path.abspath('release/test_all_pypi.sh')
    subprocess.run([str(test_script_abs), 'pypi', version], check=True)


if __name__ == '__main__':
    main()