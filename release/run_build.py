import argparse, logging, os
from lib.appveyor import start_build, download_build_artifacts
from lib.pypi import upload_to_pypi_server
from lib.common import get_appveyor_security, get_testpypi_security


default_output_dir = os.path.abspath('download_dir')


def main():
    logging.basicConfig()
    logging.root.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Run an fpbinary build on Appveyor.')
    parser.add_argument('branch', type=str,
                        help='The fpbinary Github branch to run a build on.')
    parser.add_argument('--output-dir', type=str, default=None,
                        help='If specified, the artifacts of the build will be downloaded to this directory.')
    parser.add_argument('--upload-dest', type=str, default=None, choices=['pypi', 'testpypi'],
                        help='Specify to upload the artifacts to the respective package server.')
    parser.add_argument('--wait-complete', action='store_true',
                        help='If specified, will wait until the build is finished. Automatically set if'
                             '--upload-dest or --output-dir are set.')
    parser.add_argument('--release', action='store_true',
                        help='If specified, do the build without the \'a\' pre release specifier in the output files.')
    parser.add_argument('--install-from-testpypi', action='store_true',
                        help='If specified, the build will upload to test pypi and install from it.')

    args = parser.parse_args()


    if args.upload_dest is not None and args.output_dir is None:
        args.output_dir = default_output_dir

    if args.output_dir is not None:
        args.wait_complete = True

    appveyor_security_dict = get_appveyor_security()
    testpypi_security_dict = get_testpypi_security()

    result_tuple = start_build(appveyor_security_dict['token'], appveyor_security_dict['account'],
                               'fpbinary', args.branch,
                               install_from_testpypi=args.install_from_testpypi,
                               is_release_build=args.release, wait_for_finish=args.wait_complete)

    if not args.wait_complete:
        return

    if result_tuple is None:
        logging.error('Build didn\'t start')
        exit(1)

    build_success = result_tuple[0]
    build_id = result_tuple[1]

    if build_success is True and args.output_dir is not None:
        download_build_artifacts(appveyor_security_dict['token'], appveyor_security_dict['account'],
                                 'fpbinary', os.path.abspath(args.output_dir), build_id=build_id)

        if args.upload_dest is not None:
            upload_to_pypi_server(testpypi_security_dict['token'], args.output_dir, server='testpypi')



if __name__ == '__main__':
    main()