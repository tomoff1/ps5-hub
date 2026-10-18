"""Tests HTTPS locaux du vrai téléchargeur C++, sans console ni jeu."""
import hashlib
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import unittest

PAYLOAD = bytes(range(256)) * 1200
DIGEST = hashlib.sha256(PAYLOAD).hexdigest()


@unittest.skipUnless(os.environ.get('NATIVE_DOWNLOAD_RUNNER'), 'Lancer tools/hub-download-tests.sh')
class NativeDownload(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.ca = cls.root/'cert.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                        '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost',
                        '-keyout', str(cls.root/'key.pem'), '-out', str(cls.ca)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cls.requests = []

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                cls.requests.append((self.path, self.headers.get('Range'), self.headers.get('If-Range')))
                if self.path in ['/401', '/403', '/429']:
                    self.send_response(int(self.path[1:])); self.end_headers(); return
                html = self.path == '/html'
                body = b'<html>verification required</html>' if html else PAYLOAD
                range_header = self.headers.get('Range')
                offset = int(range_header.split('=')[1].split('-')[0]) if range_header else 0
                ranged = bool(offset and self.path != '/ignore-range')
                self.send_response(206 if ranged else 200)
                self.send_header('Content-Type', 'text/html' if html else 'application/octet-stream')
                self.send_header('ETag', '"fixture-v1"')
                if ranged:
                    self.send_header('Content-Range', f'bytes {offset if self.path != "/bad-range" else 0}-{len(body)-1}/{len(body)}')
                    body = body[offset:]
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try:
                    if self.path == '/interrupt' and not range_header:
                        self.wfile.write(body[:50000]); self.wfile.flush(); self.close_connection = True
                    else:
                        self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                    pass

        cls.server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(cls.ca), str(cls.root/'key.pem'))
        cls.server.socket = context.wrap_socket(cls.server.socket, server_side=True)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f'https://localhost:{cls.server.server_port}'

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join(); cls.temp.cleanup()

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(dir=self.root)
        self.path = Path(self.folder.name)/'file.zip'
        self.requests.clear()

    def tearDown(self):
        self.folder.cleanup()

    def run_download(self, route='/file', digest=DIGEST, size=len(PAYLOAD)):
        return subprocess.run([os.environ['NATIVE_DOWNLOAD_RUNNER'], self.base+route, str(self.path),
                               digest, str(self.ca), str(size)], capture_output=True, text=True, timeout=30)

    def seed(self, route):
        url = self.base+route
        identity = hashlib.sha256(f'{url}\n{DIGEST}\n{len(PAYLOAD)}'.encode()).hexdigest()
        Path(str(self.path)+'.part').write_bytes(PAYLOAD[:30000])
        Path(str(self.path)+'.resume').write_text(identity+'\n"fixture-v1"\n')

    def test_verified_download(self):
        result = self.run_download()
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertEqual(self.path.read_bytes(), PAYLOAD)

    def test_resume_206(self):
        self.seed('/file'); result = self.run_download()
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertEqual(self.path.read_bytes(), PAYLOAD)
        self.assertEqual(self.requests[0][1:], ('bytes=30000-', '"fixture-v1"'))

    def test_ignored_range_restarts_from_zero(self):
        self.seed('/ignore-range'); result = self.run_download('/ignore-range')
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertEqual(self.path.read_bytes(), PAYLOAD)
        self.assertEqual([r[1] for r in self.requests], ['bytes=30000-', None])

    def test_bad_range_is_rejected(self):
        self.seed('/bad-range'); result = self.run_download('/bad-range')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.path.exists())
        self.assertEqual(Path(str(self.path)+'.part').read_bytes(), PAYLOAD[:30000])

    def test_network_interruption_can_resume(self):
        first = self.run_download('/interrupt')
        self.assertNotEqual(first.returncode, 0)
        self.assertTrue(Path(str(self.path)+'.part').is_file())
        second = self.run_download('/interrupt')
        self.assertEqual(second.returncode, 0, second.stdout+second.stderr)
        self.assertEqual(self.path.read_bytes(), PAYLOAD)

    def test_invalid_hash_never_replaces_previous_file(self):
        self.path.write_bytes(b'previous')
        result = self.run_download(digest='a'*64)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.path.read_bytes(), b'previous')
        self.assertFalse(Path(str(self.path)+'.part').exists())

    def test_authentication_and_html_are_not_downloads(self):
        for route in ['/401', '/403', '/429', '/html']:
            with self.subTest(route=route):
                result = self.run_download(route, size=0)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.path.exists())

    def test_expected_size_mismatch_is_rejected(self):
        result = self.run_download(size=len(PAYLOAD)+1)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.path.exists())
