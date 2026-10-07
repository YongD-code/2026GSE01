"""Optional integration example. Run explicitly; never mixed into real data automatically."""
import argparse
import json
import os
from pathlib import Path
import time
import urllib.request
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:18765/api/snapshots')
parser.add_argument('--game-id', default='sample-game')
parser.add_argument('--source-id', default='sample-instance')
parser.add_argument('--once', action='store_true')
args = parser.parse_args()
snapshot = json.loads(Path(__file__).with_name('snapshot.example.json').read_text(encoding='utf-8'))
snapshot.update(game_id=args.game_id, source_id=args.source_id, session=str(uuid.uuid4()))
headers = {'Content-Type': 'application/json'}
if os.environ.get('MONITOR_TOKEN'):
    headers['Authorization'] = 'Bearer ' + os.environ['MONITOR_TOKEN']
try:
    while True:
        snapshot['timestamp_ms'] = int(time.time() * 1000)
        data = json.dumps(snapshot, ensure_ascii=False).encode('utf-8')
        request = urllib.request.Request(args.url, data=data, headers=headers, method='POST')
        with urllib.request.urlopen(request, timeout=3) as response:
            print(response.read().decode('utf-8'), flush=True)
        if args.once:
            break
        snapshot['sequence'] += 1
        time.sleep(1)
except KeyboardInterrupt:
    pass
