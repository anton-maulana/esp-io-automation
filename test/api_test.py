import json
import sys
import urllib.error
import urllib.request

BASE = 'http://192.168.1.25'

def request(path, method='GET', body=None, token=None):
    data = None if body is None else json.dumps(body).encode()
    headers = {'Content-Type': 'application/json'} if body is not None else {}
    if token:
        headers['X-Session-Token'] = token
    req = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            raw = response.read().decode()
            return response.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as error:
        return error.code, error.read().decode()

def check(name, expected, actual):
    ok = actual == expected
    print(('PASS' if ok else 'FAIL') + ' ' + name + ': ' + str(actual))
    return ok

status, login = request('/api/auth/login', 'POST', {'username': 'admin', 'password': 'admin'})
ok = check('login', 200, status)
if not ok:
    sys.exit(1)
token = login['token']
ok = check('unauthorized config rejected', 401, request('/api/config')[0]) and ok
ok = check('config read', 200, request('/api/config', token=token)[0]) and ok
ok = check('time endpoint', 200, request('/api/time')[0]) and ok
ok = check('status endpoint', 200, request('/api/status')[0]) and ok
ok = check('config validation', 200, request('/api/config/validate', 'POST', token=token)[0]) and ok
print('RESULT ' + ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
