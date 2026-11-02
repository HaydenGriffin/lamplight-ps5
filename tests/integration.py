#!/usr/bin/env python3
"""Lamplight for PS5 - The real native websocket client against a disposable Home Assistant.

Copyright (C) 2026 Hayden Griffin
SPDX-License-Identifier: GPL-3.0-or-later
"""
import base64
import email.utils
import hashlib
import json
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time

fixture = json.loads(Path('tests/fixture.json').read_text())
# A real install's get_states is one message of several MB; the client must accept it.
filler = [{'entity_id': f'sensor.filler_{i}', 'state': '21.5', 'attributes': {'friendly_name': 'x' * 900},
           'last_changed': '2026-11-02T14:00:00.123456+00:00'} for i in range(3200)]
errors = []
commands = []
connections = 0
reject_auth = False
stop = threading.Event()
server = socket.socket()
server.bind(('127.0.0.1', 0))
server.listen()
server.settimeout(.2)

def exact(conn, count):
    data = b''
    while len(data) < count:
        block = conn.recv(count - len(data))
        if not block:
            raise EOFError()
        data += block
    return data

def receive(conn):
    a, b = exact(conn, 2)
    n = b & 127
    if n == 126:
        n = struct.unpack('!H', exact(conn, 2))[0]
    elif n == 127:
        n = struct.unpack('!Q', exact(conn, 8))[0]
    assert n < 2 * 1024 * 1024
    assert b & 128, 'client frame must be masked'
    mask = exact(conn, 4)
    data = exact(conn, n)
    payload = bytes(value ^ mask[i % 4] for i, value in enumerate(data))
    if a & 15 == 8:
        raise EOFError()
    if a & 15 == 10:
        return {}
    assert a & 15 == 1
    return json.loads(payload)

def frame(conn, data, opcode=1, final=True):
    header = bytes([(128 if final else 0) | opcode])
    n = len(data)
    if n < 126:
        header += bytes([n])
    elif n < 65536:
        header += bytes([126]) + struct.pack('!H', n)
    else:
        header += bytes([127]) + struct.pack('!Q', n)
    conn.sendall(header + data)

def send(conn, value, fragmented=False):
    data = json.dumps(value, ensure_ascii=False).encode()
    if fragmented:
        middle = len(data) // 2
        frame(conn, data[:middle], final=False)
        frame(conn, b'alive', opcode=9)
        frame(conn, data[middle:], opcode=0)
    else:
        frame(conn, data)

def session(conn):
    global connections
    connections += 1
    conn.settimeout(12)
    request = b''
    while b'\r\n\r\n' not in request:
        request += exact(conn, 1)
    assert request.startswith(b'GET /api/websocket HTTP/1.1')
    headers = dict(line.split(b': ', 1) for line in request.split(b'\r\n')[1:] if b': ' in line)
    key = headers[b'Sec-WebSocket-Key']
    accept = base64.b64encode(hashlib.sha1(key + b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest())
    # The server's clock is an hour ahead of this one, as a console's can be out.
    date = email.utils.formatdate(time.time() + 3600, usegmt=True).encode()
    conn.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nDate: ' + date +
                 b'\r\nSec-WebSocket-Accept: ' + accept + b'\r\n\r\n')
    send(conn, {'type': 'auth_required', 'ha_version': 'test'})
    assert receive(conn) == {'type': 'auth', 'access_token': 'fixture-token'}
    if reject_auth:
        send(conn, {'type': 'auth_invalid', 'message': 'fixture rejection'})
        return
    send(conn, {'type': 'auth_ok'})
    states = fixture['states']['a']
    while not stop.is_set():
        message = receive(conn)
        if not message:
            continue
        kind, ident = message['type'], message['id']
        answer = {'id': ident, 'type': 'result', 'success': True, 'result': None}
        if kind == 'get_states':
            answer['result'] = fixture['catalog'] + filler
        elif kind == 'subscribe_entities':
            wanted = message['entity_ids']
            assert 'sun.sun' in wanted
            send(conn, answer)
            # The initial event may be fragmented, with a websocket ping interleaved.
            send(conn, {'id': ident, 'type': 'event', 'event': {'a': {k: v for k, v in states.items() if k in wanted}}},
                 fragmented=True)
            continue
        elif kind == 'unsubscribe_events':
            pass
        elif kind == 'call_service':
            commands.append(message)
            call = message['domain'] + '.' + message['service']
            assert call in ('weather.get_forecasts', 'calendar.get_events', 'view_assist.set_timer',
                            'view_assist.cancel_timer', 'view_assist.set_state'), call
            if call == 'weather.get_forecasts':
                assert message['return_response'] is True
                answer['result'] = {'context': {}, 'response': fixture['forecasts'][message['service_data']['type']]}
            if message['service_data'].get('timer_id') == 'drop':
                # Drop before acknowledgement. This command must not be replayed.
                return
        elif kind == 'conversation/process':
            commands.append(message)
            answer['result'] = fixture['reply']
        elif kind == 'ping':
            send(conn, {'id': ident, 'type': 'pong'})
            continue
        else:
            raise AssertionError(kind)
        send(conn, answer)

def run():
    while not stop.is_set():
        try:
            conn, _ = server.accept()
        except socket.timeout:
            continue
        with conn:
            try:
                session(conn)
            except (EOFError, ConnectionResetError, BrokenPipeError):
                pass
            except Exception as exc:
                errors.append(repr(exc))
                return

thread = threading.Thread(target=run)
thread.start()
try:
    result = subprocess.run(['build/tests/lamplight-tests', '--integration', str(server.getsockname()[1])], timeout=25)
    assert result.returncode == 0
    reject_auth = True
    before = connections
    rejected = subprocess.run(['build/tests/lamplight-tests', '--auth-rejected', str(server.getsockname()[1])], timeout=10)
    assert rejected.returncode == 0
    assert connections == before + 1, 'rejected token retried automatically'
finally:
    stop.set()
    thread.join(timeout=3)
    server.close()
assert not errors, errors
assert connections >= 2
assert sum(c.get('service_data', {}).get('timer_id') == 'drop' for c in commands) == 1, 'a dropped command was replayed'
print('fake Home Assistant: auth, large get_states, fragmented state, forecast response, assistant, resubscribe, reconnect and no replay passed')
