import argparse, logging, os
from lib.appveyor import start_build, download_build_artifacts
from lib.common import get_appveyor_security


def main():
    logging.basicConfig()
    logging.root.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Run an fpbinary build on Appveyor.')
    parser.add_argument('branch', type=str,
                        help='The fpbinary Github branch to run a build on.')
    parser.add_argument('--output-dir', type=str, default=None,
                        help='If specified, the artifacts of the build will be downloaded to this directory.')
    parser.add_argument('--wait-complete', action='store_true',
                        help='If specified, will wait until the build is finished.')
    args = parser.parse_args()

    security_dict = get_appveyor_security()
    build_id = start_build(security_dict['token'], security_dict['account'], 'fpbinary',
                           args.branch, wait_for_finish=args.wait_complete)

    if build_id is not None:
        if args.output_dir is not None or args.wait_complete:
            download_build_artifacts(security_dict['token'], security_dict['account'],
                                     'fpbinary', os.path.abspath(args.output_dir), build_id=build_id)
    else:
        logging.error('Build didn\'t start')


if __name__ == '__main__':
    main()