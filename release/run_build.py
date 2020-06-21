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
    parser.add_argument('--init-build-num', action='store_true',
                        help='Must be set if no build has yet been done on branch.')
    args = parser.parse_args()

    if args.output_dir is not None:
        args.wait_complete = True

    security_dict = get_appveyor_security()

    build_number = 1 if args.init_build_num else None
    build_id = start_build(security_dict['token'], security_dict['account'], 'fpbinary',
                           args.branch, build_number=build_number, wait_for_finish=args.wait_complete)

    if build_id is not None:
        if args.output_dir is not None and args.wait_complete:
            download_build_artifacts(security_dict['token'], security_dict['account'],
                                     'fpbinary', os.path.abspath(args.output_dir), build_id=build_id)
    else:
        logging.error('Build didn\'t start')


if __name__ == '__main__':
    main()