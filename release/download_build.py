import argparse, logging, os
from lib.appveyor import get_build_from_name, get_last_build, download_build_artifacts
from lib.pypi import upload_to_pypi_server
from lib.common import get_appveyor_security, get_testpypi_security


def main():
    logging.basicConfig()
    logging.root.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Downloads the artifacts of a build on Appveyor.')
    parser.add_argument('buildname', type=str,
                        help='The build name (or \'version\' as Appveyor calls it) to download. '
                             'Enter a branch name if you want to download the latest build from that branch.')
    parser.add_argument('outputdir', type=str,
                        help='The artifacts of the build will be downloaded to this directory.')
    parser.add_argument('--upload-dest', type=str, default=None, choices=['pypi', 'testpypi'],
                        help='Specify to upload the artifacts to the respective package server.')
    args = parser.parse_args()

    security_dict = get_appveyor_security()

    build_name = None

    # First see if the build name exists as a build
    try:
        logging.info('Looking for build {}'.format(args.buildname))
        get_build_from_name(security_dict['token'], security_dict['account'],
                            'fpbinary', args.buildname)
        build_name = args.buildname
    except:
        # Then try a branch
        try:
            logging.info(('Looking for branch {}'.format(args.buildname)))
            build = get_last_build(security_dict['token'], security_dict['account'],
                                   'fpbinary', branch=args.buildname)
            build_name = build['version']
            logging.info('Found build {} on branch {}'.format(build['version'], args.buildname))
        except:
            pass


    if build_name is None:
        logging.error('Failed to find a build or branch called {}'.format(args.buildname))
        exit(1)

    logging.info('Downloading build {} ...'.format(build_name))
    download_build_artifacts(security_dict['token'], security_dict['account'],
                             'fpbinary', os.path.abspath(args.outputdir), build_name=build_name)

    if args.upload_dest is not None:
        upload_to_pypi_server(get_testpypi_security()['token'], args.outputdir, server=args.upload_dest)


if __name__ == '__main__':
    main()