import requests
from . import common


def _github_rest_api_build_headers(auth_token, customer_headers={}):
    customer_headers['Authorization'] = 'token {}'.format(auth_token)
    customer_headers['Content-type'] = 'application/json'
    return customer_headers


def _github_get_full_url(relative_url):
    return common.concat_urls(['https://api.github.com', relative_url])


def create_light_tag(auth_token, owner, repository, sha, tagname):
    r = requests.post(
        _github_get_full_url('/repos/{}/{}/git/refs'.format(owner, repository)),
        headers=_github_rest_api_build_headers(auth_token),
        json={'ref': 'refs/tags/{}'.format(tagname),'sha': sha})

    # 201 == "created"
    if r.status_code != 201:
        r.raise_for_status()





