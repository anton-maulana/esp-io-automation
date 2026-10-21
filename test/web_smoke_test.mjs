import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const elements = new Map();
const requests = [];
const sockets = [];
function element() {
    return {
        classList: { add() {}, remove() {}, toggle() {} },
        dataset: {}, children: [], options: [], textContent: '', innerHTML: '',
        appendChild(child) { this.children.push(child); },
        querySelector() { return element(); }
    };
}
const document = {
    cookie: '',
    getElementById(id) {
        if (!elements.has(id)) elements.set(id, element());
        return elements.get(id);
    },
    querySelectorAll() { return []; },
    createElement: element
};
const responses = {
    '/api/public/dashboard': { deviceName: 'Test device', relays: [{ id: 'relay-1', name: 'Relay', state: false }] },
    '/api/time': { source: 'rtc', iso: '12:00' },
    '/api/status': { board: 'esp8266_d1_mini' },
    '/gpio-pins.json': { esp8266_d1_mini: { pins: [{ gpio: 12, label: 'D6' }] } }
};
const context = vm.createContext({
    document,
    location: { protocol: 'http:', host: 'device.test' },
    fetch: async (url) => {
        requests.push(url);
        assert.ok(Object.hasOwn(responses, url), `Unexpected request: ${url}`);
        return { ok: true, json: async () => responses[url] };
    },
    WebSocket: class { constructor(url) { this.url = url; sockets.push(this); } },
    setTimeout() {},
    alert(message) { throw Error(message); }
});
vm.runInContext(readFileSync(new URL('../data/app.js', import.meta.url), 'utf8'), context);
await new Promise((resolve) => setImmediate(resolve));
assert.deepEqual(requests.sort(), Object.keys(responses).sort());
assert.equal(document.getElementById('device-name').textContent, 'Test device');
assert.equal(document.getElementById('summary').textContent, '1 action tersedia · 0 sedang menyala');
assert.equal(sockets.length, 1);
assert.equal(sockets[0].url, 'ws://device.test/ws');
sockets[0].onmessage({ data: JSON.stringify({ type: 'relay.state', relayId: 'relay-1', actualState: true }) });
assert.equal(document.getElementById('summary').textContent, '1 action tersedia · 1 sedang menyala');
assert.equal(typeof document.getElementById('gpio-form').onsubmit, 'function');
assert.equal(typeof document.getElementById('login-form').onsubmit, 'function');
assert.equal(typeof document.getElementById('add-extend-io').onclick, 'function');
console.log('Web bundle smoke test passed: startup, API loading, bindings, and WebSocket state.');
