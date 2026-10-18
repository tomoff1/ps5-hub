"""Tests HTTPS locaux du vrai téléchargeur C++, sans console ni jeu."""
import hashlib
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time
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
                if self.path in ['/401', '/403', '/404', '/410', '/429']:
                    self.send_response(int(self.path[1:]))
                    if self.path == '/429': self.send_header('Retry-After', '23')
                    self.end_headers(); return
                if self.path == '/no-location':
                    self.send_response(302); self.send_header('Content-Length', '0'); self.end_headers(); return
                if self.path in ['/redirect', '/to-html', '/loop', '/downgrade']:
                    self.send_response(302)
                    target = {'/redirect': '/file?token=secret-fixture', '/to-html': '/html',
                              '/loop': '/loop', '/downgrade': 'http://127.0.0.1:1/private'}[self.path]
                    self.send_header('Location', target)
                    self.send_header('Content-Length', '0')
                    self.end_headers(); return
                html = self.path in ['/html', '/lying-html', '/xhtml', '/fragmented-html']
                body = b'<html>verification required</html>' if html else PAYLOAD
                if self.path == '/pkg': body = b'\x7fCNT' + PAYLOAD[4:]
                range_header = self.headers.get('Range')
                offset = int(range_header.split('=')[1].split('-')[0]) if range_header else 0
                ranged = bool(offset and self.path != '/ignore-range')
                self.send_response(206 if ranged else 200)
                self.send_header('Content-Type', 'application/xhtml+xml' if self.path == '/xhtml' else 'text/html' if self.path == '/html' else 'application/octet-stream')
                self.send_header('ETag', '"fixture-v1"')
                if ranged:
                    self.send_header('Content-Range', f'bytes {offset if self.path != "/bad-range" else 0}-{len(body)-1}/{len(body)}')
                    body = body[offset:]
                if self.path == '/unknown-size': self.close_connection = True
                else: self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try:
                    if self.path == '/interrupt' and not range_header:
                        self.wfile.write(body[:50000]); self.wfile.flush(); self.close_connection = True
                    elif self.path == '/fragmented-html':
                        for byte in body:
                            self.wfile.write(bytes([byte])); self.wfile.flush(); time.sleep(0.005)
                    elif self.path == '/slow':
                        for start in range(0, len(body), 8192):
                            self.wfile.write(body[start:start+8192]); self.wfile.flush()
                            time.sleep(0.01)
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

    def run_download(self, route='/file', digest=DIGEST, size=len(PAYLOAD), stop_after=None, ca=None, expected_format=None):
        args = [os.environ['NATIVE_DOWNLOAD_RUNNER'], self.base+route, str(self.path),
                digest, str(ca or self.ca), str(size)]
        if stop_after is not None or expected_format:
            args.append(str(stop_after) if stop_after is not None else '-')
        if expected_format:
            args.append(expected_format)
        return subprocess.run(args, capture_output=True, text=True, timeout=30)

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

    def test_explicit_pause_keeps_partial_and_can_resume(self):
        first = self.run_download('/slow', stop_after=40000)
        self.assertNotEqual(first.returncode, 0, first.stdout+first.stderr)
        self.assertIn('suspendu', first.stdout)
        self.assertFalse(self.path.exists())
        partial = Path(str(self.path)+'.part')
        size = partial.stat().st_size
        self.assertGreater(size, 0)
        self.assertLess(size, len(PAYLOAD))
        second = self.run_download('/slow')
        self.assertEqual(second.returncode, 0, second.stdout+second.stderr)
        self.assertEqual(self.path.read_bytes(), PAYLOAD)
        self.assertEqual(self.requests[-1][1], f'bytes={size}-')

    def test_https_redirect_downloads_without_browser_or_token_in_diagnostics(self):
        result = self.run_download('/redirect')
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertEqual(self.path.read_bytes(), PAYLOAD)
        self.assertIn('redirects=1', result.stdout)
        self.assertIn('chain=302,200', result.stdout)
        self.assertNotIn('secret-fixture', result.stdout)
        self.assertNotIn('https://', result.stdout)

    def test_redirect_to_html_is_not_a_file(self):
        result = self.run_download('/to-html', size=0)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('HTML_PAGE', result.stdout)
        self.assertFalse(self.path.exists())
        self.assertFalse(Path(str(self.path)+'.part').exists())

    def test_html_with_false_mime_is_rejected(self):
        for route in ['/lying-html', '/xhtml', '/fragmented-html']:
            with self.subTest(route=route):
                result = self.run_download(route, size=0)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn('HTML_PAGE', result.stdout)
                self.assertFalse(self.path.exists())
                self.assertFalse(Path(str(self.path)+'.part').exists())

    def test_explicit_404_410_and_retry_after(self):
        for code in [404, 410, 429]:
            result = self.run_download(f'/{code}', size=0)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(f'HTTP_{code}', result.stdout)
            if code == 429: self.assertIn('23 secondes', result.stdout)

    def test_redirect_loop_and_downgrade_are_bounded(self):
        for route, reason in [('/loop', 'CURL_47'), ('/downgrade', 'CURL_1')]:
            result = self.run_download(route, size=0)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(reason, result.stdout)
            self.assertFalse(self.path.exists())
        self.assertLessEqual(len(self.requests), 8)

    def test_space_preflight_rejects_before_payload(self):
        result = self.run_download(stop_after='no-space')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('NO_SPACE', result.stdout)
        self.assertFalse(self.path.exists())
        self.assertEqual(Path(str(self.path)+'.part').stat().st_size, 0)

    def test_storage_creation_error_has_errno(self):
        self.path = self.path.parent/'missing'/'file.zip'
        result = self.run_download()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('STORAGE_SETUP', result.stdout)
        self.assertIn('errno=2', result.stdout)
        self.assertEqual(self.requests, [])

    def test_invalid_ca_does_not_disable_tls(self):
        result = self.run_download(ca=self.path.parent/'missing-ca.pem')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('CURL_77', result.stdout)
        self.assertFalse(self.path.exists())

    def test_pkg_signature_is_checked_before_accepting_a_large_body(self):
        wrong = self.run_download(expected_format='PKG')
        self.assertNotEqual(wrong.returncode, 0)
        self.assertIn('NOT_PKG', wrong.stdout)
        self.assertFalse(self.path.exists())
        self.assertFalse(Path(str(self.path)+'.part').exists())
        pkg = b'\x7fCNT' + PAYLOAD[4:]
        right = self.run_download('/pkg', digest=hashlib.sha256(pkg).hexdigest(), expected_format='PKG')
        self.assertEqual(right.returncode, 0, right.stdout+right.stderr)
        self.assertEqual(self.path.read_bytes(), pkg)

    def test_html_mime_is_reported_before_size_mismatch(self):
        result = self.run_download('/html')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('HTML_PAGE', result.stdout)
        self.assertFalse(self.path.exists())

    def test_redirect_without_location_is_not_a_download(self):
        result = self.run_download('/no-location', size=0)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('HTTP_302', result.stdout)
        self.assertIn('aucune réponse fichier', result.stdout)

    def test_hostname_mismatch_keeps_certificate_verification_enabled(self):
        self.base = type(self).base.replace('localhost', '127.0.0.1')
        result = self.run_download()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('CURL_60', result.stdout)
        self.assertIn('Certificat HTTPS refusé', result.stdout)
        self.assertFalse(self.path.exists())

    def test_stream_without_length_cannot_exceed_download_limit(self):
        result = self.run_download('/unknown-size', size=0, stop_after='small-limit')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SIZE_LIMIT', result.stdout)
        self.assertFalse(self.path.exists())
        self.assertLessEqual(Path(str(self.path)+'.part').stat().st_size, 100000)
