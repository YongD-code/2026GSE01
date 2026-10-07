import copy
import http.client
import json
from pathlib import Path
import threading
import unittest

from server import Collector, Registry, validate_snapshot


def sample():
    return json.loads(Path(__file__).with_name('snapshot.example.json').read_text(encoding='utf-8'))


class ContractTests(unittest.TestCase):
    def test_distinct_games_and_instances(self):
        registry = Registry()
        first = sample()
        registry.accept(first)
        second = copy.deepcopy(first)
        second['game_id'] = 'second-game'
        registry.accept(second)
        third = copy.deepcopy(first)
        third['source_id'] = 'second-instance'
        registry.accept(third)
        self.assertEqual(len(registry.sources), 3)
        self.assertEqual(sum(r['snapshot']['counts']['npc_active'] for r in registry.sources.values()), 3)

    def test_duplicate_and_out_of_order_do_not_refresh(self):
        registry = Registry()
        message = sample()
        message.update(sequence=5, timestamp_ms=500)
        self.assertTrue(registry.accept(message))
        record = next(iter(registry.sources.values()))
        self.assertFalse(registry.accept(copy.deepcopy(message)))
        late = copy.deepcopy(message)
        late.update(sequence=4, timestamp_ms=600)
        self.assertFalse(registry.accept(late))
        self.assertIs(next(iter(registry.sources.values())), record)
        late.update(session='new-session', sequence=1, timestamp_ms=700)
        self.assertTrue(registry.accept(late))
        self.assertFalse(registry.accept(message))

    def test_inconsistent_counts_and_unknown_regions_rejected(self):
        bad = sample()
        bad['counts']['npc_active'] = 5
        with self.assertRaises(ValueError):
            validate_snapshot(bad)
        bad = sample()
        bad['npcs'][0]['region'] = 'nonexistent'
        with self.assertRaises(ValueError):
            validate_snapshot(bad)

    def test_duplicate_npc_rejected(self):
        bad = sample()
        bad['npcs'].append(copy.deepcopy(bad['npcs'][0]))
        with self.assertRaises(ValueError):
            validate_snapshot(bad)


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.server = Collector(('127.0.0.1', 0), 'test-token')
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.clients = []

    def tearDown(self):
        for client in self.clients:
            client.close()
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def client(self):
        client = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=4)
        self.clients.append(client)
        return client

    def post(self, data, extra=None):
        headers = {'Content-Type': 'application/json', 'Authorization': 'Bearer test-token'}
        headers.update(extra or {})
        connection = self.client()
        connection.request('POST', '/api/snapshots', json.dumps(data, ensure_ascii=False).encode('utf-8'), headers)
        response = connection.getresponse()
        return response.status, json.loads(response.read())

    def test_post_read_and_auth(self):
        self.assertEqual(self.post(sample())[0], 202)
        self.assertEqual(self.post(sample(), {'Authorization': 'Bearer wrong'})[0], 401)
        connection = self.client()
        connection.request('GET', '/api/state', headers={'Authorization': 'Bearer test-token'})
        response = connection.getresponse()
        state = json.loads(response.read())
        self.assertEqual(state['sources'][0]['snapshot']['npcs'][0]['name'], '샘플 NPC')

    def test_broadcast_two_viewers(self):
        streams = []
        for _ in range(2):
            connection = self.client()
            connection.request('GET', '/api/events', headers={'Authorization': 'Bearer test-token'})
            response = connection.getresponse()
            self.assertEqual(response.status, 200)
            self.assertEqual(response.readline(), b'event: state\n')
            response.readline()
            response.readline()
            streams.append(response)
        self.assertEqual(self.post(sample())[0], 202)
        for stream in streams:
            self.assertEqual(stream.readline(), b'event: snapshot\n')
            record = json.loads(stream.readline().decode('utf-8')[6:])
            self.assertEqual(record['snapshot']['game_id'], 'sample-game')
        # A late subscriber receives the latest full state, not an empty stream.
        connection = self.client()
        connection.request('GET', '/api/events', headers={'Authorization': 'Bearer test-token'})
        response = connection.getresponse()
        response.readline()
        self.assertEqual(len(json.loads(response.readline().decode('utf-8')[6:])['sources']), 1)
        for stream in streams + [response]:
            stream.close()

    def test_invalid_payload_and_origin(self):
        self.assertEqual(self.post({'type': 'bad'})[0], 400)
        self.assertEqual(self.post(sample(), {'Origin': 'http://unrelated.invalid'})[0], 403)
        connection = self.client()
        connection.request('GET', '/../../SimpleGame/Data/Monitoring.ini', headers={'Authorization': 'Bearer test-token'})
        response = connection.getresponse()
        self.assertEqual(response.status, 404)
        response.read()


if __name__ == '__main__':
    unittest.main()
