import logging, ntpath, os
import requests
from . import common
import time


appveyor_api_url_prefix = 'https://ci.appveyor.com/api/'
appveyor_ok_codes = [200, 204]
max_time_to_wait_for_build_secs = 300


def _appveyor_rest_api_build_headers(auth_token, customer_headers={},
                                     content_type='json'):
    customer_headers['Authorization'] = 'Bearer {}'.format(auth_token)

    if content_type == 'json':
        customer_headers['Content-type'] = 'application/json'

    return customer_headers


def _appveyor_get_full_url(relative_url):
    return common.concat_urls([appveyor_api_url_prefix, relative_url])


def _get_projects(auth_token):
    r = requests.get(_appveyor_get_full_url('projects'),
                     headers=_appveyor_rest_api_build_headers(auth_token))
    r.raise_for_status()
    return r.json()


def get_project_id(auth_token, project_name):
    js = _get_projects(auth_token)
    for project in js:
        if project['slug'] == project_name:
            return project['projectId']

    return None


def get_last_build(auth_token, account_name, project_name, branch='master'):
    """
    Returns the 'build' dict of the last build on branch.
    """
    r = requests.get(
        _appveyor_get_full_url('projects/{}/{}/branch/{}'.format(
            account_name, project_name, branch)),
        headers=_appveyor_rest_api_build_headers(auth_token))

    if r.status_code == 404:
        # Not found error
        # Assume there has been no build for this branch and return None.
        return None

    r.raise_for_status()
    return r.json()['build']


def get_build_from_id(auth_token, account_name, project_name, build_id):
    """
        Returns the 'build' dict of the build with id build_id.
    """

    # Note that this url doesn't seem to be documented. I found it at
    # https://help.appveyor.com/discussions/problems/17648-build-api-seems-to-have-changed-to-buildsbuildid
    r = requests.get(
        _appveyor_get_full_url('projects/{}/{}/builds/{}'.format(
            account_name, project_name, build_id)),
        headers=_appveyor_rest_api_build_headers(auth_token))
    r.raise_for_status()
    return r.json()['build']


def get_build_from_name(auth_token, account_name, project_name, build_name):
    """
        Returns the 'build' dict of the build with build_name.
    """
    r = requests.get(
        _appveyor_get_full_url('projects/{}/{}/build/{}'.format(
            account_name, project_name, build_name)),
        headers=_appveyor_rest_api_build_headers(auth_token))
    r.raise_for_status()
    return r.json()['build']


def set_build_number(auth_token, account_name, project_name, build_number):
    """
        Sets the next build number to build_number.
    """
    r = requests.put(
        _appveyor_get_full_url('projects/{}/{}/settings/build-number'.format(
            account_name, project_name)),
        headers=_appveyor_rest_api_build_headers(auth_token),
        json={'nextBuildNumber': build_number})
    r.raise_for_status()


def get_artifact_list(auth_token, job_id):
    """
        Returns a list of artifacts dicts for job_id.
    """
    r = requests.get(
        _appveyor_get_full_url('buildjobs/{}/artifacts'.format(job_id)),
        headers=_appveyor_rest_api_build_headers(auth_token))
    r.raise_for_status()
    return r.json()


def download_job_artifacts(auth_token, job_id, output_dir_path):
    artifacts = get_artifact_list(auth_token, job_id)

    for artifact in artifacts:
        # Artifact file names can have directory paths in them, including
        # windows format, so need to use ntpath instead of os.path
        filename = ntpath.basename(artifact['fileName'])

        logging.info('Downloading file {} to {}'.format(
            filename, output_dir_path
        ))

        r = requests.get(
            _appveyor_get_full_url('buildjobs/{}/artifacts/{}'.format(
                job_id, artifact['fileName'])),
            headers=_appveyor_rest_api_build_headers(auth_token, content_type='content'))
        r.raise_for_status()

        with open(os.path.join(output_dir_path, filename), mode='wb') as f:
            f.write(r.content)


def download_build_artifacts(auth_token, account_name, project_name, output_dir_path,
                             build_name=None, build_id=None):

    if build_name is not None:
        build = get_build_from_name(auth_token, account_name, project_name, build_name)
    elif build_id is not None:
        build = get_build_from_id(auth_token, account_name, project_name, build_id)

    for job in build['jobs']:
        download_job_artifacts(auth_token, job['jobId'], output_dir_path)


def start_build(auth_token, account_name, project_name, branch, wait_for_finish=False):
    """
    If successfully started, returns the build id.
    Else, returns None.
    """

    logging.info('Retrieving last build for branch {}...'.format(branch))
    last_build = get_last_build(auth_token, account_name, project_name, branch)
    if last_build is None:
        logging.warning('No builds for branch {} were found. Assuming this is the first...'.format(
            branch
        ))
        build_number = 1
    else:
        build_number = last_build['buildNumber'] + 1

    if build_number is not None:
        logging.info('Setting build number to {}'.format(build_number))
        set_build_number(auth_token, account_name, project_name, build_number)

        logging.info('Starting build...')

        r = requests.post(
            _appveyor_get_full_url('builds'),
            headers=_appveyor_rest_api_build_headers(auth_token),
            json={'accountName': account_name, 'projectSlug': project_name, 'branch': branch})
        build_id = r.json()['buildId']

        accum_secs = 0
        sleep_secs = 30

        logging.info('Started build: {}'.format(build_id))
        if wait_for_finish:
            logging.info('Waiting for build to finish...')
            while accum_secs < max_time_to_wait_for_build_secs:

                time.sleep(sleep_secs)
                accum_secs += sleep_secs

                build = get_build_from_id(auth_token, account_name, project_name, build_id)

                if 'finished' in build:
                    logging.info('Build {} completed with status {}'.format(
                        build['version'], build['status']))
                    return build_id

            logging.error('Build completion timed out')

        return build_id

    return None


