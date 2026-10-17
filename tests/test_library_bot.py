"""Import privé en masse et protocole HyperDPI, sans console."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('library_bot',ROOT/'tools/hub-library-bot.py')
bot=importlib.util.module_from_spec(spec);spec.loader.exec_module(bot)

class LibraryBotTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'build').mkdir(exist_ok=True)
        self.tmp=tempfile.TemporaryDirectory(dir=ROOT/'build');self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def test_bulk_txt_deduplicates_and_generates_artwork(self):
        path=self.root/'links.txt';path.write_text('https://example.org/a.pkg\nhttps://example.org/a.pkg\nhttps://example.org/b.pkg\n')
        entries=bot.import_list(path,self.root/'out');self.assertEqual(len(entries),2)
        art=(self.root/'out/bundle/covers'/f'{entries[0]["id"]}.hubart').read_bytes()
        self.assertEqual(art[:12],b'HUBA'+struct.pack('<II',192,288));self.assertEqual(len(art),12+192*288*4)
    def test_french_csv_and_local_image(self):
        from PIL import Image
        Image.new('RGB',(100,100),'red').save(self.root/'cover.png')
        path=self.root/'links.csv';path.write_text('nom;lien;jaquette\nMon jeu;https://example.org/a.pkg;cover.png\n',encoding='utf-8')
        entries=bot.import_list(path,self.root/'out');self.assertEqual(entries[0]['title'],'Mon jeu')
        self.assertEqual(json.loads((self.root/'out/import-report.json').read_text())['erreurs'],[])
    def test_invalid_import_preserves_previous_library(self):
        path=self.root/'links.txt';path.write_text('https://example.org/a.pkg\n');bot.import_list(path,self.root/'out')
        previous=(self.root/'out/library.json').read_bytes();path.write_text('http://unsafe.example.org/a.pkg\n')
        with self.assertRaises(ValueError):bot.import_list(path,self.root/'out')
        self.assertEqual((self.root/'out/library.json').read_bytes(),previous)
    def test_hash_and_identity_validation(self):
        with self.assertRaises(ValueError):bot.normalize({'id':'../evil','url':'https://example.org/a.pkg'})
        with self.assertRaises(ValueError):bot.normalize({'sha256':'xyz','url':'https://example.org/a.pkg'})
        with self.assertRaises(ValueError):bot.safe_url('https://u:p@example.org/a.pkg')
    def test_hyperdpi_opener_hash_then_finalize(self):
        path=self.root/'personal.pkg';path.write_bytes(b'x'*(bot.CHUNK+35));calls=[]
        def api(base,url,body=None):calls.append((base,url,body));return {'ok':True,'name':'pkg-server','busy':False}
        with patch.object(bot,'api_json',side_effect=api):bot.hyperdpi_upload(path,'192.168.1.50',9090)
        self.assertEqual(calls[0][1],'/ping');self.assertIn('&head256=',calls[1][1])
        self.assertEqual(len(calls[1][2]),bot.CHUNK);self.assertEqual(len(calls[2][2]),35)
        self.assertIn('finalize=1',calls[-1][1]);self.assertEqual(calls[-1][2],b'')
    def test_unknown_service_never_receives_pkg(self):
        path=self.root/'personal.pkg';path.write_bytes(b'test')
        with patch.object(bot,'api_json',return_value={'ok':True,'name':'unknown'}) as api:
            with self.assertRaises(ValueError):bot.hyperdpi_upload(path,'192.168.1.50',9090)
            self.assertEqual(api.call_count,1)
    def test_public_destination_rejected(self):
        with self.assertRaises(ValueError):bot.local_host('8.8.8.8')
        with self.assertRaises(ValueError):bot.local_host('0.0.0.0')
if __name__=='__main__':unittest.main()
