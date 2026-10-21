import json
import time
import urllib.request

base = 'http://192.168.1.25'
def call(path, method='GET', body=None, token=None):
    headers = {'Content-Type': 'application/json'}
    if token: headers['X-Session-Token'] = token
    req = urllib.request.Request(base + path, data=None if body is None else json.dumps(body).encode(), headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=10) as response:
        return response.status, response.read().decode()
_, raw = call('/api/auth/login', 'POST', {'username': 'admin', 'password': 'admin'})
token = json.loads(raw)['token']
_, raw = call('/api/public/dashboard')
before = next(r for r in json.loads(raw)['relays'] if r['id'] == 'relay-02')['state']
status, _ = call('/api/relay/toggle?id=relay-02', 'POST', token=token)
time.sleep(1)
_, raw = call('/api/public/dashboard')
after = next(r for r in json.loads(raw)['relays'] if r['id'] == 'relay-02')['state']
print({'toggleStatus': status, 'before': before, 'after': after, 'changed': before != after})
if before != after:
    call('/api/relay/toggle?id=relay-02', 'POST', token=token)
