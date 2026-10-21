(function(){var state={relays:[],groups:{},pending:{},ws:null,admin:false,token:document.cookie.split('; ').find(function(x){return x.indexOf('modu_session=')===0})?.split('=')[1]||'',config:null};console.log(state);function setSession(token){state.token=token;document.cookie='modu_session='+encodeURIComponent(token)+'; Max-Age=3600; Path=/; SameSite=Strict'}function clearSession(){state.token='';document.cookie='modu_session=; Max-Age=0; Path=/; SameSite=Strict'}var el=function(id){return document.getElementById(id)};function escapeHtml(s){return String(s||'').replace(/[&<>"']/g, function(c) {
            return {
                '&': '&amp;',
                '<': '&lt;',
                '>': '&gt;',
                '"': '&quot;',
                "'": '&#39;'
            } [c]
        })
    }

    function loadGpioPins() {
        fetch('/gpio-pins.json').then(function(r) {
            return r.json()
        }).then(function(d) {
            var board = 'esp8266_d1_mini',
                select = el('gpio-pin-select');
            select.innerHTML = '';
            (d[board].pins || []).forEach(function(p) {
                var option = document.createElement('option');
                option.value = p.gpio;
                option.textContent = p.label;
                select.appendChild(option)
            });
            select.dataset.pinCatalog = board
        }).catch(function() {
            el('gpio-pin-select').innerHTML = '<option value="">Daftar pin gagal dimuat</option>'
        })
    }

    function render() {
        var groups = {};
        state.relays.forEach(function(r) {
            var g = r.group || 'GPIO Switch';
            (groups[g] || (groups[g] = [])).push(r)
        });
        state.groups = groups;
        var root = el('switch-groups');
        root.innerHTML = '';
        Object.keys(groups).forEach(function(name) {
            var section = document.createElement('section');
            section.className = 'switch-group';
            section.innerHTML = '<h3>' + escapeHtml(name) + '</h3><div class="switch-grid"></div>';
            var grid = section.querySelector('.switch-grid');
            groups[name].forEach(function(r) {
                var pending = state.pending[r.id];
                var card = document.createElement('article');
                card.className = 'switch-card ' + (r.state ? 'on ' : '') + (pending ? 'pending' : '');
                var io = r.source === 'gpio' ? 'GPIO ' + r.gpio : (r.pcfId || 'Extend Switch') + ' · P' + r.pin,
                    active = r.state;
                card.innerHTML = '<div class="switch-main"><div class="switch-icon ' + (active ? 'active' : '') + '">' + (active ? '☀' : '○') + '</div><div><div class="switch-name">' + escapeHtml(r.name) + '</div><div class="switch-room">' + escapeHtml(io) + ' · ' + escapeHtml(r.room || 'Switch') + '</div></div></div><button class="toggle ' + (active ? 'active' : '') + '" ' + (pending ? 'disabled' : '') + '><span class="toggle-dot"></span>' + (pending ? 'Processing...' : active ? 'ON' : 'OFF') + '</button>';
                card.querySelector('button').onclick = function() {
                    toggle(r)
                };
                grid.appendChild(card)
            });
            root.appendChild(section)
        });
        var on = state.relays.filter(function(r) {
            return r.state
        }).length;
        el('summary').textContent = state.relays.length + ' action tersedia · ' + on + ' sedang menyala';
    }

    function load() {
        fetch('/api/public/dashboard').then(function(r) {
            return r.json()
        }).then(function(d) {
            el('device-name').textContent = d.deviceName || 'ModuSwitch';
            state.relays = d.relays || [];
            render();
            el('sync-status').textContent = 'Tersinkron'
        }).catch(function() {
            el('sync-status').textContent = 'Tidak dapat memuat'
        });
        fetch('/api/time').then(function(r) {
            return r.json()
        }).then(function(t) {
            el('clock-status').textContent = 'Waktu ' + (t.source || 'tidak diketahui').toUpperCase() + ': ' + String(t.iso || 'belum tersedia').trim()
        }).catch(function() {
            el('clock-status').textContent = 'Waktu tidak tersedia'
        })
        syncLoginUI();
    }

    function syncLoginUI() {
        if (state.token) {
            state.admin = true;
            loadConfig().catch(function() {
                el('login-error').textContent = 'Login berhasil, tetapi konfigurasi gagal dimuat.'
            });
            el('login-modal').classList.add('hidden');
            el('login-error').textContent = '';
            el('menu-button').classList.remove('hidden');
            el('login-button').classList.add('hidden')
        } else {
            el('menu-button').classList.add('hidden');
            el('login-button').classList.remove('hidden');
        }
        
    }
    function connect() {
        var url = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws';
        state.ws = new WebSocket(url);
        state.ws.onopen = function() {
            el('connection').textContent = 'Online';
            el('connection').classList.add('online')
        };
        state.ws.onclose = function() {
            el('connection').textContent = 'Menyambungkan ulang';
            el('connection').classList.remove('online');
            setTimeout(connect, 2500)
        };
        state.ws.onmessage = function(e) {
            try {
                var m = JSON.parse(e.data);
                if (m.type === 'relay.state') {
                    var r = state.relays.find(function(x) {
                        return x.id === m.relayId
                    });
                    if (r) {
                        r.state = m.actualState;
                        delete state.pending[m.relayId];
                        render()
                    }
                }
            } catch (_) {}
        }
    }

    function toggle(r) {
        if (state.pending[r.id]) return;
        state.pending[r.id] = true;
        render();
        fetch('/api/relay/toggle?id=' + encodeURIComponent(r.id), {
            method: 'POST',
            headers: state.token ? {
                'X-Session-Token': state.token
            } : {}
        }).then(function(response) {
            if (!response.ok) throw Error('toggle_failed');
            return load()
        }).catch(function() {
            alert('Switch command failed')
        }).finally(function() {
            delete state.pending[r.id];
            render()
        })
    }

    function showView(view) {
        document.querySelectorAll('.view').forEach(function(v) {
            v.classList.add('hidden')
        });
        el(view + '-view').classList.remove('hidden');
        el('admin-drawer').classList.remove('open');
        if (view === 'dashboard') load();
        if (view !== 'dashboard' && state.token && !state.config) loadConfig().catch(function() {})
    }

    function renderConfigLists() {
        if (!state.config) return;
        var pins = state.config.pins || [],
            gpio = el('gpio-list');
        gpio.innerHTML = '';
        pins.filter(function(p) {
            return p.source === 'gpio'
        }).forEach(function(p) {
            var item = document.createElement('div');
            item.className = 'config-item';
            item.innerHTML = '<div><strong>GPIO' + escapeHtml(String(p.gpio)) + ' · ' + escapeHtml(p.name) + '</strong><span class="config-meta">' + escapeHtml(p.mode) + ' · default ' + (p.initialState ? 'High' : 'Low') + '</span></div><div class="item-actions"><button type="button" class="button ghost edit-gpio">Edit</button><button type="button" class="button danger delete-gpio">Hapus</button></div>';
            item.querySelector('.edit-gpio').onclick = function() {
                showForm('gpio');
                var f = el('gpio-form');
                f.dataset.editId = p.id;
                f.gpio.value = p.gpio;
                f.mode.value = p.mode;
                f.name.value = p.name;
                f.defaultCondition.value = p.initialState ? 'high' : 'low'
            };
            item.querySelector('.delete-gpio').onclick = function() {
                if (!confirm('Hapus GPIO ini dan relay terkait?')) return;
                state.config.pins = state.config.pins.filter(function(x) {
                    return x.id !== p.id
                });
                state.config.relays = (state.config.relays || []).filter(function(x) {
                    return x.pinId !== p.id
                });
                configRequest('PUT', state.config).then(loadConfig).catch(function(e) {
                    alert('Gagal: ' + e.message)
                })
            };
            gpio.appendChild(item)
        });
        var extend = el('extend-list');
        extend.innerHTML = '';
        (state.config.hardware && state.config.hardware.pcfs || []).forEach(function(p) {
            var item = document.createElement('div');
            item.className = 'config-item';
            item.innerHTML = '<div><strong>' + escapeHtml(p.id) + '</strong><span class="config-meta">Address 0x' + Number(p.address).toString(16) + ' · PCF8574</span></div><div class="item-actions"><button type="button" class="button ghost edit-extend">Edit</button><button type="button" class="button danger delete-extend">Hapus</button></div>';
            item.querySelector('.edit-extend').onclick = function() {
                showForm('extend');
                var f = el('extend-form');
                f.dataset.editId = p.id;
                f.address.value = '0x' + Number(p.address).toString(16);
                f.name.value = p.name || p.id
            };
            item.querySelector('.delete-extend').onclick = function() {
                if (!confirm('Hapus Extend Switch ini? Pin dan relay yang terkait juga akan dihapus.')) return;
                var id = p.id,
                    removedPins = (state.config.pins || []).filter(function(x) {
                        return x.pcfId === id
                    }).map(function(x) {
                        return x.id
                    });
                state.config.hardware.pcfs = (state.config.hardware.pcfs || []).filter(function(x) {
                    return x.id !== id
                });
                state.config.pins = (state.config.pins || []).filter(function(x) {
                    return x.pcfId !== id
                });
                state.config.relays = (state.config.relays || []).filter(function(x) {
                    return removedPins.indexOf(x.pinId) < 0
                });
                configRequest('PUT', state.config).then(loadConfig).catch(function(e) {
                    alert('Gagal: ' + e.message)
                })
            };
            extend.appendChild(item)
        });
        var actions = el('action-list');
        actions.innerHTML = '';
        (state.config.automations || []).forEach(function(a) {
            var item = document.createElement('div');
            item.className = 'config-item';
            item.innerHTML = '<div><strong>' + escapeHtml(a.name || a.id) + '</strong><span class="config-meta">' + escapeHtml(a.trigger && a.trigger.type || 'action') + '</span></div><div class="item-actions"><button type="button" class="button ghost edit-action">Edit</button><button type="button" class="button danger delete-action">Hapus</button></div>';
            item.querySelector('.edit-action').onclick = function() {
                showForm('action');
                var f = el('action-form');
                f.dataset.editId = a.id;
                f.dataset.editName = a.name || a.id;
                f.targetType.value = a.targetType || 'gpio';
                f.condition.value = a.condition || 'low';
                f.trigger.value = a.trigger && a.trigger.type || 'input_change';
                f.action.value = a.actions && a.actions[0] && a.actions[0].type || 'toggle_relay';
                f.trigger.dispatchEvent(new Event('change'))
            };
            item.querySelector('.delete-action').onclick = function() {
                if (!confirm('Hapus Action ini?')) return;
                state.config.automations = (state.config.automations || []).filter(function(x) {
                    return x.id !== a.id
                });
                configRequest('PUT', state.config).then(loadConfig).catch(function(e) {
                    alert('Gagal menghapus Action: ' + e.message)
                })
            };
            actions.appendChild(item)
        })
    }

    function showForm(type) {
        var form = el(type + '-form');
        if (form) form.classList.remove('hidden')
    }

    function hideForm(type) {
        var form = el(type + '-form');
        if (form) form.classList.add('hidden')
    }

    function configRequest(method, body) {
        return fetch('/api/config', {
            method: method,
            headers: {
                'Content-Type': 'application/json',
                'X-Session-Token': state.token
            },
            body: body ? JSON.stringify(body) : undefined
        }).then(function(r) {
            return r.json().then(function(d) {
                if (!r.ok) throw Error(d.error || 'config_error');
                return d
            })
        })
    }

    function loadConfig() {
        return fetch('/api/config', {
            headers: {
                'X-Session-Token': state.token
            }
        }).then(function(r) {
            if (!r.ok) {
                clearSession();
                 window.location.reload();
                
                return new Promise(function() {});
            }
            return r.json()
        }).then(function(c) {
            state.config = c;
            var d = c.device || {},
                w = (c.network || {}).wifi || {},
                t = c.time || {},
                ds = t.ds3231 || {};
            var f = el('device-form');
            f.deviceName.value = d.name || '';
            f.deviceId.value = d.id || '';
            f.timezone.value = d.timezone || 'Asia/Jakarta';
            f.hostname.value = w.hostname || '';
            f.ssid.value = w.ssid || '';
            f.reconnectIntervalMs.value = w.reconnectIntervalMs || 5000;
            f.ds3231Address.value = '0x' + Number(ds.address || 104).toString(16);
            f.anonymousControl.checked = !!(c.access || {}).anonymousControl;
            el('device-status').textContent = 'Konfigurasi dimuat';
            renderConfigLists();
            return c
        })
    }

    function plannedSave(label) {
        return function(e) {
            e.preventDefault();
            if (!state.config) {
                alert('Konfigurasi belum dimuat');
                return
            }
            configRequest('PUT', state.config).then(function() {
                alert(label + ' tersimpan; reboot diperlukan.')
            }).catch(function(err) {
                alert('Gagal: ' + err.message)
            })
        }
    }

    function addIo() {
        var row = document.createElement('div');
        row.className = 'io-row';
        row.innerHTML = '<input name="pin" type="number" min="0" max="7" placeholder="Pin 0-7" required><select name="mode"><option value="output">Output</option><option value="input">Input</option></select><input name="name" placeholder="Nama I/O" required><select name="default"><option value="low">Low</option><option value="high">High</option></select><button type="button" class="plain remove-io">×</button>';
        row.querySelector('.remove-io').onclick = function() {
            row.remove()
        };
        el('extend-io-list').appendChild(row)
    }
    el('login-button').onclick = function() {
        el('login-modal').classList.remove('hidden')
    };
    el('close-login').onclick = function() {
        el('login-modal').classList.add('hidden')
    };
    el('menu-button').onclick = function() {
        el('admin-drawer').classList.add('open')
    };
    el('close-menu').onclick = function() {
        el('admin-drawer').classList.remove('open')
    };
    document.querySelectorAll('[data-view]').forEach(function(b) {
        b.onclick = function() {
            showView(b.dataset.view)
        }
    });
    document.querySelectorAll('[data-add]').forEach(function(b) {
        b.onclick = function() {
            showForm(b.dataset.add);
            if (b.dataset.add === 'extend' && !el('extend-io-list').children.length) addIo()
        }
    });
    document.querySelectorAll('[data-cancel]').forEach(function(b) {
        b.onclick = function() {
            hideForm(b.dataset.cancel)
        }
    });
    el('add-extend-io').onclick = addIo;
    el('gpio-form').onsubmit = function(e) {
        e.preventDefault();
        if (!state.config) {
            alert('Konfigurasi belum dimuat; login ulang.');
            return
        }
        var f = new FormData(e.target),
            gpio = Number(f.get('gpio')),
            mode = f.get('mode'),
            name = f.get('name'),
            level = f.get('defaultCondition') === 'high' ? 1 : 0,
            id = 'pin-gpio-' + gpio,
            allowed = Array.prototype.some.call(el('gpio-pin-select').options, function(o) {
                return Number(o.value) === gpio
            });
        if (!allowed) {
            alert('Pilih GPIO dari daftar pin aman.');
            return
        }
        state.config.pins = state.config.pins || [];
        if (state.config.pins.some(function(p) {
                return p.id === id || p.source === 'gpio' && Number(p.gpio) === gpio
            })) {
            alert('GPIO sudah terdaftar');
            return
        }
        state.config.pins.push({
            id: id,
            name: name,
            source: 'gpio',
            gpio: gpio,
            mode: mode,
            initialState: level,
            safeState: level,
            enabled: true,
            debounceMs: mode === 'input' ? 50 : 0
        });
        if (mode === 'output') {
            state.config.relays = state.config.relays || [];
            state.config.relays.push({
                id: 'relay-' + id,
                name: name,
                room: 'GPIO Switch',
                icon: 'bulb',
                pinId: id,
                activeLevel: 1,
                enabled: true,
                publicControl: true,
                startupPolicy: 'off'
            })
        }
        configRequest('PUT', state.config).then(function() {
            hideForm('gpio');
            alert('GPIO tersimpan. Reboot perangkat untuk menerapkan hardware.');
            return loadConfig()
        }).catch(function(err) {
            alert('Gagal menyimpan GPIO: ' + err.message)
        })
    };
    el('extend-form').onsubmit = function(e) {
        e.preventDefault();
        if (!state.config) {
            alert('Konfigurasi belum dimuat');
            return
        }
        var f = new FormData(e.target),
            id = e.target.dataset.editId || 'pcf-' + Date.now(),
            address = parseInt(String(f.get('address')).replace(/^0x/i, ''), 16),
            name = f.get('name');
        state.config.hardware = state.config.hardware || {};
        state.config.hardware.pcfs = state.config.hardware.pcfs || [];
        var existing = state.config.hardware.pcfs.find(function(x) {
            return x.id === id
        });
        if (existing) {
            existing.address = address;
            existing.name = name
        } else {
            state.config.hardware.pcfs.push({
                id: id,
                address: address,
                name: name,
                type: 'PCF8574',
                enabled: true
            })
        }
        configRequest('PUT', state.config).then(function() {
            delete e.target.dataset.editId;
            hideForm('extend');
            return loadConfig()
        }).catch(function(err) {
            alert('Gagal menyimpan Extend Switch: ' + err.message)
        })
    };
    el('action-form').onsubmit = function(e) {
        e.preventDefault();
        if (!state.config) {
            alert('Konfigurasi belum dimuat');
            return
        }
        var f = new FormData(e.target),
            id = e.target.dataset.editId || 'auto-' + Date.now(),
            trigger = f.get('trigger'),
            action = {
                id: id,
                name: e.target.dataset.editName || 'Action ' + id,
                enabled: true,
                trigger: {
                    type: trigger,
                    edge: f.get('condition') === 'high' ? 'rising' : 'falling'
                },
                actions: [{
                    type: f.get('action'),
                    relayId: (state.config.relays && state.config.relays[0] && state.config.relays[0].id) || 'relay-01'
                }]
            };
        if (trigger === 'schedule') {
            action.trigger.hour = Number(f.get('hour') || 0);
            action.trigger.minute = Number(f.get('minute') || 0)
        }
        if (trigger === 'request') action.trigger.url = f.get('requestUrl') || '';
        state.config.automations = state.config.automations || [];
        var index = state.config.automations.findIndex(function(x) {
            return x.id === id
        });
        if (index >= 0) state.config.automations[index] = action;
        else state.config.automations.push(action);
        configRequest('PUT', state.config).then(function() {
            delete e.target.dataset.editId;
            delete e.target.dataset.editName;
            hideForm('action');
            return loadConfig()
        }).catch(function(err) {
            alert('Gagal menyimpan Action: ' + err.message)
        })
    };
    el('device-form').onsubmit = function(e) {
        e.preventDefault();
        if (!state.config) {
            alert('Konfigurasi belum dimuat');
            return
        }
        var f = new FormData(e.target),
            c = state.config;
        c.device.name = f.get('deviceName');
        c.device.id = f.get('deviceId');
        c.device.timezone = f.get('timezone');
        c.network.wifi.hostname = f.get('hostname');
        c.network.wifi.ssid = f.get('ssid');
        if (f.get('password')) c.network.wifi.password = f.get('password');
        c.network.wifi.reconnectIntervalMs = Number(f.get('reconnectIntervalMs'));
        c.time = c.time || {};
        c.time.ds3231 = c.time.ds3231 || {};
        c.time.ds3231.address = parseInt(f.get('ds3231Address'));
        c.access = c.access || {};
        c.access.anonymousControl = f.anonymousControl.checked;
        configRequest('PUT', c).then(function() {
            el('device-status').textContent = 'Tersimpan; reboot diperlukan'
        }).catch(function(err) {
            el('device-status').textContent = 'Gagal: ' + err.message
        })
    };
    el('action-trigger').onchange = function() {
        var schedule = this.value === 'schedule',
            request = this.value === 'request';
        el('schedule-fields').classList.toggle('hidden', !schedule);
        el('request-field').classList.toggle('hidden', !request)
    };
    el('scan-wifi').onclick = function() {
        alert('Wi-Fi scan akan tersedia pada endpoint konfigurasi.')
    };
    el('upload-firmware').onclick = function() {
        if (confirm('Upload firmware memerlukan proses maintenance. Lanjutkan?')) alert('Fitur upload firmware akan tersedia pada endpoint maintenance.')
    };
    el('reboot-device').onclick = function() {
        if (confirm('Reboot perangkat sekarang?')) alert('Endpoint reboot belum diaktifkan.')
    };
    el('logout-button').onclick = function() {
        state.admin = false;
        clearSession();
        showView('dashboard');
        el('menu-button').classList.add('hidden');
        el('login-button').classList.remove('hidden')
    };
    el('login-form').onsubmit = function(e) {
        e.preventDefault();
        var f = new FormData(e.target);
        fetch('/api/auth/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                username: f.get('username'),
                password: f.get('password')
            })
        }).then(function(r) {
            if (!r.ok) throw Error();
            return r.json()
        }).then(function(d) {
            setSession(d.token);
            syncLoginUI();
        }).catch(function() {
            el('login-error').textContent = 'Username atau password salah.'
        })
    };
    loadGpioPins();
    load();
    connect();
})();