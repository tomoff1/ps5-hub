"""Local DPIv2 simulator. No console, real game, or installer is contacted."""
import hashlib
import http.server
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import unittest

IDENTITY = b"<title>etaHEN DPIv2</title><form action='/upload'><input name='file'></form>"


@unittest.skipUnless(os.environ.get('NATIVE_INSTALLER_RUNNER'), 'native installer runner required')
class NativeInstaller(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.posts = []
        cls.mode = 'ok'

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_GET(self):
                body = IDENTITY if cls.mode != 'foreign' else b'pkg-server'
                self.send_response(302 if cls.mode == 'redirect' else 200)
                if cls.mode == 'redirect':
                    self.send_header('Location', 'http://127.0.0.1:1/')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_POST(self):
                body = self.rfile.read(int(self.headers['Content-Length']))
                cls.posts.append((self.path, self.headers['Content-Type'], body))
                if cls.mode == 'disconnect':
                    self.close_connection = True
                    return
                reply = (b'SUCCESS: PKG installation started from file: hub-package.pkg'
                         if cls.mode == 'ok' else b'FAILED: Install failed, code -2136801278 (0x8000)')
                if cls.mode == 'unexpected':
                    reply = b'<html>unexpected intermediary</html>'
                self.send_response(200)
                self.send_header('Content-Length', str(len(reply)))
                self.end_headers()
                self.wfile.write(reply)

        cls.server = http.server.ThreadingHTTPServer(('127.0.0.1', 12800), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def setUp(self):
        type(self).mode = 'ok'
        type(self).posts = []
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.pkg = Path(self.tmp.name) / 'fixture.pkg'
        data = bytearray(0x100000)
        data[:4] = b'\x7fCNT'
        struct.pack_into('>I', data, 0x10, 1)
        struct.pack_into('>I', data, 0x18, 0x5a0)
        self.pkg.write_bytes(data)
        self.sha = hashlib.sha256(data).hexdigest()

    def run_native(self, probe=False, sha=None):
        args = ['probe'] if probe else ['install', str(self.pkg), sha or self.sha]
        result = subprocess.run([os.environ['NATIVE_INSTALLER_RUNNER'], *args],
                                capture_output=True, text=True, timeout=20,
                                env={**os.environ, 'http_proxy': 'http://127.0.0.1:1'})
        self.assertEqual(result.returncode, 0, result.stderr)
        flags, message = result.stdout.split('\n', 1)
        return flags, message

    def test_identifies_service_without_post_or_proxy(self):
        self.assertEqual(self.run_native(probe=True)[0], '1 0 0')
        self.assertEqual(self.posts, [])

    def test_refuses_foreign_service(self):
        type(self).mode = 'foreign'
        self.assertEqual(self.run_native()[0], '0 0 0')
        self.assertEqual(self.posts, [])

    def test_refuses_redirect(self):
        type(self).mode = 'redirect'
        self.assertEqual(self.run_native(probe=True)[0], '0 0 0')

    def test_multipart_contains_exact_verified_bytes(self):
        flags, message = self.run_native()
        self.assertEqual(flags, '1 1 0')
        self.assertIn('non confirmés', message)
        self.assertEqual(len(self.posts), 1)
        path, mime, body = self.posts[0]
        self.assertEqual(path, '/upload')
        self.assertIn('multipart/form-data; boundary=', mime)
        self.assertIn(b'name="file"; filename="hub-package.pkg"', body)
        self.assertIn(self.pkg.read_bytes(), body)

    def test_bad_hash_never_posts(self):
        self.assertEqual(self.run_native(sha='a' * 64)[0], '0 0 0')
        self.assertEqual(self.posts, [])

    def test_invalid_package_never_posts(self):
        self.pkg.write_bytes(b'not a package')
        self.assertEqual(self.run_native()[0], '0 0 0')
        self.assertEqual(self.posts, [])

    def test_eta_error_is_not_success(self):
        type(self).mode = 'refuse'
        flags, message = self.run_native()
        self.assertEqual(flags, '1 0 0')
        self.assertIn('-2136801278', message)

    def test_unexpected_reply_is_unknown_not_safe_to_retry(self):
        type(self).mode = 'unexpected'
        self.assertEqual(self.run_native()[0], '1 0 1')

    def test_lost_reply_is_unknown_not_safe_to_retry(self):
        type(self).mode = 'disconnect'
        flags, message = self.run_native()
        self.assertEqual(flags, '1 0 1')
        self.assertIn('inconnu', message)


if __name__ == '__main__':
    unittest.main()
