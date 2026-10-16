"""Test du protocole FTP sans connexion à une console."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('hub_ftp',ROOT/'tools/hub-ftp.py')
ftp_tool=importlib.util.module_from_spec(spec)
spec.loader.exec_module(ftp_tool)

class FakeFTP:
    def __init__(self,corrupt=False,exists=False):
        self.corrupt=corrupt;self.exists=exists;self.files={};self.renames=[];self.path='/data/homebrew'
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def connect(self,*args,**kwargs):pass
    def login(self,*args):pass
    def cwd(self,path):self.path=path if path.startswith('/') else self.path+'/'+path
    def pwd(self):return self.path
    def mlsd(self):return [('PPSA99841',{})] if self.exists else []
    def mkd(self,path):pass
    def storbinary(self,command,source):self.files[command[5:]]=source.read()
    def retrbinary(self,command,callback):callback(b'corruption' if self.corrupt else self.files[command[5:]])
    def rename(self,source,target):self.renames.append((source,target))

class HubFTPTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        (self.root/'sce_sys').mkdir();(self.root/'sce_module').mkdir()
        (self.root/'sce_sys/param.json').write_text(json.dumps({'titleId':'PPSA99841'}))
        (self.root/'eboot.bin').write_bytes(bytes.fromhex('4f153d1d')+b'test fixture')
        (self.root/'sce_sys/icon0.png').write_bytes(b'icon fixture')
        (self.root/'sce_module/libc.prx').write_bytes(b'runtime fixture')
    def tearDown(self):self.temp.cleanup()
    def run_upload(self,fake):
        with patch.object(ftp_tool.ftplib,'FTP',return_value=fake),patch('sys.argv',['hub-ftp',str(self.root),'--host','127.0.0.1','--yes']),patch('sys.stdout',new_callable=io.StringIO):
            ftp_tool.main()
    def test_verified_upload_publishes_metadata_last(self):
        fake=FakeFTP();self.run_upload(fake)
        self.assertEqual(len(fake.renames),2)
        self.assertEqual(fake.renames[-1],('PPSA99841/sce_sys/param.json.pending','PPSA99841/sce_sys/param.json'))
        self.assertTrue(any(name.endswith('/param.json.pending') for name in fake.files))
        self.assertFalse(any(name.endswith('/param.json') for name in fake.files))
    def test_corrupt_transfer_never_promotes_title(self):
        fake=FakeFTP(corrupt=True)
        with self.assertRaises(RuntimeError):self.run_upload(fake)
        self.assertEqual(fake.renames,[])
    def test_existing_title_is_not_overwritten(self):
        fake=FakeFTP(exists=True)
        with self.assertRaises(SystemExit):self.run_upload(fake)
        self.assertEqual(fake.files,{})
    def test_invalid_identity_is_rejected_before_network(self):
        (self.root/'sce_sys/param.json').write_text(json.dumps({'titleId':'../outside'}))
        with self.assertRaises(ValueError):ftp_tool.inspect(self.root)

if __name__=='__main__':unittest.main()
