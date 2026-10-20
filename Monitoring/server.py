"""JSON collector and SSE broadcaster. Python standard library only."""
import argparse
from collections import deque
import hmac
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import threading
import time
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent
MAX_BODY = 512 * 1024


def validate_snapshot(data):
    """Shared v1 contract; reject malformed messages before broadcasting."""
    def require(ok, message):
        if not ok:
            raise ValueError(message)

    def text(value):
        return isinstance(value, str) and 0 < len(value) <= 256

    def integer(value):
        return type(value) is int and 0 <= value <= 10**15

    def position(value):
        return value is None or (isinstance(value, dict) and all(
            type(value.get(k)) in (float, int) and math.isfinite(value[k]) for k in ('x', 'y')))

    require(isinstance(data, dict), 'object required')
    require(data.get('schema_version') == 1 and data.get('type') == 'world_snapshot', 'unsupported schema')
    for key in ('game_id', 'game_name', 'source_id', 'session', 'mode', 'scope'):
        require(text(data.get(key)), 'invalid ' + key)
    for key in ('sequence', 'timestamp_ms'):
        require(integer(data.get(key)), 'invalid ' + key)
    require(data['mode'] in ('title', 'opening', 'playing', 'paused', 'dialogue', 'dead'), 'invalid mode')
    require(data['scope'] in ('loaded_entities', 'all_entities'), 'invalid scope')
    counts = data.get('counts')
    require(isinstance(counts, dict), 'counts required')
    for key in ('npc_registered', 'npc_active', 'monsters_alive', 'monsters_combat', 'monsters_weakened'):
        require(integer(counts.get(key)), 'invalid count ' + key)
    regions, npcs, monsters = (data.get(k) for k in ('regions', 'npcs', 'monsters'))
    for rows in (regions, npcs, monsters):
        require(isinstance(rows, list) and len(rows) <= 4096, 'invalid entity list')
        ids = set()
        for item in rows:
            require(isinstance(item, dict) and text(item.get('id')), 'invalid entity id')
            require(item['id'] not in ids, 'duplicate entity id')
            ids.add(item['id'])
    region_ids = {region['id'] for region in regions}
    for region in regions:
        require(text(region.get('name')) and text(region.get('status')), 'invalid region')
        require(region.get('kind') in ('village', 'area'), 'invalid region kind')
        require(region.get('coverage') in ('loaded_only', 'complete'), 'invalid coverage')
        for key in ('npc_active', 'monsters_alive', 'monsters_combat'):
            require(integer(region.get(key)), 'invalid region count')
    for npc in npcs:
        require(text(npc.get('name')) and text(npc.get('status')), 'invalid npc')
        require(type(npc.get('active')) is bool, 'invalid active flag')
        require(position(npc.get('position')), 'invalid npc position')
        require(not npc['active'] or npc.get('region') in region_ids, 'unknown npc region')
        if npc.get('kind') == 'autonomous':
            require(npc.get('role') in ('farmer', 'merchant', 'artisan', 'hunter'), 'invalid NPC role')
            require(position(npc.get('destination')), 'invalid NPC destination')
            for key in ('health', 'maximum', 'stamina'):
                require(type(npc.get(key)) in (float, int) and math.isfinite(npc[key]) and npc[key] >= 0,
                        'invalid NPC ' + key)
            require(npc['health'] <= npc['maximum'] and npc['stamina'] <= 100, 'invalid NPC vitals')
            for key in ('gold', 'earned', 'goods', 'kills'):
                require(integer(npc.get(key)), 'invalid NPC ' + key)
    for mob in monsters:
        require(text(mob.get('status')) and mob.get('region') in region_ids, 'invalid monster')
        require(position(mob.get('position')), 'invalid monster position')
        for key in ('health', 'maximum'):
            require(type(mob.get(key)) in (float, int) and math.isfinite(mob[key]) and mob[key] >= 0,
                    'invalid monster health')
    require(counts['npc_registered'] == len(npcs), 'registered NPC count mismatch')
    require(counts['npc_active'] == sum(npc['active'] for npc in npcs), 'active NPC count mismatch')
    require(counts['monsters_alive'] == len(monsters), 'monster count mismatch')
    require(counts['monsters_combat'] == sum(mob['status'] == 'combat' for mob in monsters), 'combat count mismatch')
    require(counts['monsters_weakened'] == sum(mob['status'] in ('weakened', 'remnant') for mob in monsters), 'weakened count mismatch')
    residents = [npc for npc in npcs if npc.get('kind') == 'autonomous' and npc['active']]
    expected = {'npc_autonomous': len(residents),
                'npc_working': sum(n['status'] in ('working', 'trading') for n in residents),
                'npc_hunting': sum(n['status'] in ('hunting', 'fighting') for n in residents),
                'npc_recovering': sum(n['status'] == 'recovering' for n in residents),
                'npc_gold': sum(n['gold'] for n in residents),
                'npc_kills': sum(n['kills'] for n in residents)}
    for key, value in expected.items():
        if key in counts:
            require(integer(counts[key]) and counts[key] == value, 'invalid count ' + key)
    for region in regions:
        require(region['npc_active'] == sum(n['active'] and n.get('region') == region['id'] for n in npcs),
                'region NPC count mismatch')
        require(region['monsters_alive'] == sum(m['region'] == region['id'] for m in monsters),
                'region monster count mismatch')
        require(region['monsters_combat'] == sum(m['region'] == region['id'] and m['status'] == 'combat' for m in monsters),
                'region combat count mismatch')
    # Additive village-life fields: older games can keep sending the original v1 contract.
    npc_ids = {npc['id'] for npc in npcs}
    npc_by_id = {npc['id']: npc for npc in npcs}
    for key in ('houses', 'letters', 'roads'):
        if key in data:
            require(isinstance(data[key], list) and len(data[key]) <= 4096, 'invalid ' + key)
    if 'houses' in data:
        houses = data['houses']
        house_ids = set()
        occupied_owners = set()
        for house in houses:
            require(isinstance(house, dict) and text(house.get('id')), 'invalid house')
            require(house['id'] not in house_ids, 'duplicate house id')
            house_ids.add(house['id'])
            require(text(house.get('village')) and house['village'] in region_ids and position(house.get('position')),
                    'invalid house village or position')
            require(house.get('status') in ('occupied', 'vacant', 'reserved') and
                    text(house.get('address')), 'invalid house state')
            owner = house.get('owner')
            require((owner is None) == (house['status'] == 'vacant'), 'invalid house owner')
            if owner is not None:
                require(text(owner) and owner in npc_ids, 'unknown house owner')
            if house['status'] == 'occupied':
                require(owner not in occupied_owners and npc_by_id[owner].get('home_id') == house['id'],
                        'duplicate or inconsistent home ownership')
                occupied_owners.add(owner)
            elif house['status'] == 'reserved':
                require(npc_by_id[owner].get('migrating_to') == house['village'],
                        'invalid migration reservation')
        for npc in npcs:
            if 'home_id' in npc:
                require(text(npc['home_id']) and npc['home_id'] in house_ids, 'unknown home id')
        for region in regions:
            if 'houses' not in region:
                continue
            for key in ('houses', 'houses_occupied', 'houses_reserved', 'houses_vacant',
                        'residents', 'mail_in_transit', 'news_known'):
                require(integer(region.get(key)), 'invalid village field ' + key)
            local = [h for h in houses if h['village'] == region['id']]
            require(region['houses'] == len(local), 'village house count mismatch')
            for status, key in [('occupied', 'houses_occupied'), ('reserved', 'houses_reserved'),
                                ('vacant', 'houses_vacant')]:
                require(region[key] == sum(h['status'] == status for h in local),
                        'village housing state mismatch')
            require(region['residents'] == region['houses_occupied'], 'resident address count mismatch')
    if 'letters' in data:
        letter_ids = set()
        for letter in data['letters']:
            require(isinstance(letter, dict) and text(letter.get('id')), 'invalid letter')
            require(letter['id'] not in letter_ids, 'duplicate letter id')
            letter_ids.add(letter['id'])
            require(text(letter.get('from')) and text(letter.get('to')) and
                    letter['from'] in region_ids and letter['to'] in region_ids and
                    letter['from'] != letter['to'], 'invalid letter route')
            require(text(letter.get('carrier')) and letter['carrier'] in npc_ids and type(letter.get('delivered')) is bool and
                    text(letter.get('summary')), 'invalid letter carrier or content')
        for npc in npcs:
            if npc.get('mail_id') is not None:
                require(any(l['id'] == npc['mail_id'] and l['carrier'] == npc['id'] and
                            not l['delivered'] for l in data['letters']), 'invalid carried letter')
    for road in data.get('roads', []):
        require(isinstance(road, dict) and text(road.get('from')) and text(road.get('to')) and
                road['from'] in region_ids and road['to'] in region_ids and
                road['from'] != road['to'], 'invalid road')
        seconds = road.get('travel_seconds')
        require(type(seconds) in (int, float) and math.isfinite(seconds) and seconds > 0,
                'invalid road travel time')
    if 'society' in data:
        society = data['society']
        require(isinstance(society, dict), 'invalid society')
        for key in ('letters_delivered', 'migrations'):
            require(integer(society.get(key)), 'invalid society count')
        elapsed = society.get('elapsed_seconds')
        require(type(elapsed) in (int, float) and math.isfinite(elapsed) and elapsed >= 0,
                'invalid society time')
    return data


class Registry:
    def __init__(self, snapshot_dir=None):
        self.changed = threading.Condition()
        self.sources = {}
        self.revision = 0
        self.events = deque(maxlen=64)
        self.listeners = 0
        self.snapshot_dir = snapshot_dir

    def accept(self, data):
        validate_snapshot(data)
        key = (data['game_id'], data['source_id'])
        with self.changed:
            previous = self.sources.get(key)
            if previous:
                old = previous['snapshot']
                if data['session'] == old['session'] and data['sequence'] <= old['sequence']:
                    return False  # Duplicates do not refresh freshness.
                if data['session'] != old['session'] and data['timestamp_ms'] < old['timestamp_ms']:
                    return False
            elif len(self.sources) >= 64:
                raise ValueError('source limit reached (64)')
            record = {'received_at_ms': int(time.time() * 1000), 'snapshot': data}
            if self.snapshot_dir is not None:
                self.snapshot_dir.mkdir(parents=True, exist_ok=True)
                identity = json.dumps(key, ensure_ascii=False).encode('utf-8')
                target = self.snapshot_dir / (hashlib.sha256(identity).hexdigest() + '.json')
                temporary = target.with_suffix('.tmp')
                temporary.write_text(json.dumps(record, ensure_ascii=False, allow_nan=False, indent=2), encoding='utf-8')
                os.replace(temporary, target)
                # Process IDs change between runs; bound retained latest files across restarts.
                saved = sorted(self.snapshot_dir.glob('*.json'), key=lambda p: p.stat().st_mtime, reverse=True)
                for expired in saved[64:]:
                    expired.unlink(missing_ok=True)
            self.sources[key] = record
            self.revision += 1
            self.events.append((self.revision, record))
            self.changed.notify_all()
            return True

    def state(self):
        # Call while holding changed.
        return {'server_time_ms': int(time.time() * 1000), 'sources': list(self.sources.values())}


class Collector(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, token=''):
        super().__init__(address, Handler)
        self.registry = Registry(ROOT / 'snapshots')
        self.token = token


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def log_message(self, fmt, *args):
        if len(args) >= 2 and str(args[1]) not in ('200', '202'):
            super().log_message(fmt, *args)

    def respond(self, status, data, mime='application/json; charset=utf-8'):
        body = data if isinstance(data, bytes) else json.dumps(data, ensure_ascii=False, allow_nan=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; style-src 'self'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'")
        self.send_header('Connection', 'close')
        self.end_headers()
        self.close_connection = True
        self.wfile.write(body)

    def authorized(self):
        # No cross-origin browser access; remote publishers use bearer authentication.
        origin = self.headers.get('Origin')
        if origin and urlsplit(origin).netloc != self.headers.get('Host'):
            self.respond(403, {'error': 'cross-origin request rejected'})
            return False
        if self.server.token and not hmac.compare_digest(
                self.headers.get('Authorization', ''), 'Bearer ' + self.server.token):
            self.respond(401, {'error': 'token required'})
            return False
        return True

    def do_POST(self):
        if urlsplit(self.path).path != '/api/snapshots':
            self.respond(404, {'error': 'not found'})
            return
        if not self.authorized():
            return
        if self.headers.get_content_type() != 'application/json':
            self.respond(415, {'error': 'application/json required'})
            return
        try:
            size = int(self.headers.get('Content-Length', '0'))
            if not 0 < size <= MAX_BODY or self.headers.get('Transfer-Encoding'):
                self.respond(413, {'error': 'body must be 1..524288 bytes with Content-Length'})
                return
            raw = self.rfile.read(size)
            if len(raw) != size:
                raise ValueError('incomplete body')
            data = json.loads(raw.decode('utf-8'), parse_constant=lambda _: (_ for _ in ()).throw(ValueError('nonfinite JSON')))
            accepted = self.server.registry.accept(data)
            self.respond(202, {'accepted': accepted})
        except (ValueError, UnicodeError, RecursionError, OverflowError) as exc:
            self.respond(400, {'error': str(exc)[:200]})
        except (TimeoutError, ConnectionError):
            self.close_connection = True
        except OSError:
            self.respond(503, {'error': 'snapshot storage unavailable'})

    def do_GET(self):
        path = urlsplit(self.path).path
        static = {'/': ('index.html', 'text/html; charset=utf-8'),
                  '/dashboard.js': ('dashboard.js', 'text/javascript; charset=utf-8'),
                  '/activity.css': ('activity.css', 'text/css; charset=utf-8'),
                  '/style.css': ('style.css', 'text/css; charset=utf-8')}
        if path in static:
            name, mime = static[path]
            self.respond(200, (ROOT / name).read_bytes(), mime)
            return
        if not self.authorized():
            return
        if path == '/api/state':
            with self.server.registry.changed:
                state = self.server.registry.state()
            self.respond(200, state)
        elif path == '/api/events':
            self.events()
        else:
            self.respond(404, {'error': 'not found'})

    def events(self):
        registry = self.server.registry
        with registry.changed:
            if registry.listeners >= 32:
                self.respond(503, {'error': 'viewer limit reached (32)'})
                return
            registry.listeners += 1
            revision, state = registry.revision, registry.state()
        try:
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Accel-Buffering', 'no')
            self.send_header('Connection', 'close')
            self.end_headers()

            def send(event, data):
                payload = json.dumps(data, ensure_ascii=False, allow_nan=False, separators=(',', ':'))
                self.wfile.write(('event: ' + event + '\ndata: ' + payload + '\n\n').encode('utf-8'))
                self.wfile.flush()

            send('state', state)
            while True:
                with registry.changed:
                    registry.changed.wait_for(lambda: registry.revision > revision, timeout=2)
                    current = registry.revision
                    overflow = registry.events and revision < registry.events[0][0] - 1
                    updates = [record for seq, record in registry.events if seq > revision]
                    state = registry.state() if overflow else None
                if state is not None:
                    send('state', state)
                else:
                    for record in updates:
                        send('snapshot', record)
                send('heartbeat', {'server_time_ms': int(time.time() * 1000)})
                revision = current
        except (ConnectionError, TimeoutError, OSError):
            pass
        finally:
            self.close_connection = True
            with registry.changed:
                registry.listeners -= 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', default=18765, type=int)
    args = parser.parse_args()
    token = os.environ.get('MONITOR_TOKEN', '')
    if args.host not in ('127.0.0.1', 'localhost', '::1') and not token:
        parser.error('Set MONITOR_TOKEN before binding to a network interface.')
    server = Collector((args.host, args.port), token)
    print(f'Monitor: http://{args.host}:{server.server_port} (Ctrl+C to stop)', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
