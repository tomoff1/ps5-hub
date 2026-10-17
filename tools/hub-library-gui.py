"""Assistant Windows natif pour le bot de bibliothèque PS5 Hub."""
import importlib.util
from pathlib import Path
import queue
import threading
import tkinter as tk
from tkinter import filedialog,messagebox,ttk

root_path=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('library_bot',root_path/'tools/hub-library-bot.py')
bot=importlib.util.module_from_spec(spec);spec.loader.exec_module(bot)

class App:
    def __init__(self,window):
        self.window=window;self.events=queue.Queue();self.busy=False;self.library=root_path/'.local/library'
        window.title('PS5 Hub — Bibliothèque PC');window.geometry('1020x760')
        window.configure(bg='#101b2c')
        style=ttk.Style();style.theme_use('clam');style.configure('TFrame',background='#101b2c');style.configure('TLabel',background='#101b2c',foreground='#eff6ff',font=('Segoe UI',11));style.configure('TButton',font=('Segoe UI',11),padding=8)
        frame=ttk.Frame(window,padding=24);frame.pack(fill='both',expand=True)
        ttk.Label(frame,text='PS5 Hub · Votre bibliothèque',font=('Segoe UI',22,'bold')).pack(anchor='w')
        ttk.Label(frame,text='Collez tous vos liens directs (un par ligne) ou ouvrez un CSV / JSON avec noms et jaquettes.').pack(anchor='w',pady=(8,10))
        self.text=tk.Text(frame,height=9,bg='#1c2c43',fg='white',insertbackground='white',font=('Consolas',11));self.text.pack(fill='x')
        bar=ttk.Frame(frame);bar.pack(fill='x',pady=10)
        ttk.Button(bar,text='Ouvrir TXT / CSV / JSON',command=self.open_list).pack(side='left')
        ttk.Button(bar,text='Préparer les liens collés',command=self.prepare_paste).pack(side='left',padx=10)
        ttk.Button(bar,text='Ouvrir le dossier privé',command=self.open_folder).pack(side='right')
        self.auto_covers=tk.BooleanVar(value=True)
        ttk.Checkbutton(frame,text='Chercher les jaquettes sur Steam (suggestions à vérifier, votre titre est envoyé à Steam)',variable=self.auto_covers).pack(anchor='w',pady=(0,8))
        self.table=ttk.Treeview(frame,columns=('id','title'),show='headings',height=9);self.table.heading('id',text='Identifiant');self.table.heading('title',text='Nom du jeu / contenu');self.table.column('id',width=220);self.table.column('title',width=600);self.table.pack(fill='both',expand=True)
        settings=ttk.Frame(frame);settings.pack(fill='x',pady=12)
        ttk.Label(settings,text='IP de la PS5 :').pack(side='left');self.ip=ttk.Entry(settings,width=20);self.ip.pack(side='left',padx=8)
        self.mode=tk.StringVar(value='ftp');ttk.Combobox(settings,textvariable=self.mode,values=('ftp','hyperdpi'),width=12,state='readonly').pack(side='left',padx=8)
        ttk.Button(settings,text='Synchroniser fiches + jaquettes',command=self.sync).pack(side='left',padx=6)
        ttk.Button(settings,text='Transférer le PKG sélectionné',command=self.transfer).pack(side='left',padx=6)
        self.status=tk.StringVar(value='Prêt. Aucun jeu téléchargé ou transféré automatiquement.');ttk.Label(frame,textvariable=self.status,wraplength=950).pack(anchor='w')
        ttk.Label(frame,text='FTP = copie du fichier. HyperDPI = demande d’installation après confirmation. Les PKG ne sont pas convertis.',wraplength=950).pack(anchor='w',pady=8)
        window.after(150,self.poll)
    def run(self,action):
        if self.busy:messagebox.showinfo('Opération en cours','Attendez la fin de l’opération actuelle.');return
        self.busy=True;self.status.set('Opération en cours… Pour un gros PKG, le téléchargement peut prendre du temps.')
        def worker():
            try:result=action();self.events.put(('ok',result))
            except Exception as error:self.events.put(('error',str(error) if isinstance(error,ValueError) else type(error).__name__))
        threading.Thread(target=worker,daemon=True).start()
    def poll(self):
        try:
            kind,result=self.events.get_nowait();self.busy=False
            if kind=='progress':
                self.busy=True;self.status.set(result);self.window.after(150,self.poll);return
            if kind=='error':self.status.set('Échec : '+result);messagebox.showerror('Opération interrompue',result)
            elif isinstance(result,list):
                self.table.delete(*self.table.get_children())
                for entry in result:self.table.insert('',tk.END,iid=entry['id'],values=(entry['id'],entry['title']))
                self.status.set(f'{len(result)} fiches prêtes. Synchronisez les jaquettes par FTP puis actualisez PS5 Hub avec Carré.')
            else:self.status.set(result or 'Opération terminée.')
        except queue.Empty:pass
        self.window.after(150,self.poll)
    def prepare_paste(self):
        content=self.text.get('1.0',tk.END)
        if not content.strip():messagebox.showinfo('Liste vide','Collez un lien direct par ligne.');return
        path=root_path/'.local/mes-liens.txt';bot.atomic(path,content.encode())
        auto=self.auto_covers.get();self.run(lambda:bot.import_list(path,self.library,auto))
    def open_list(self):
        path=filedialog.askopenfilename(filetypes=[('Listes','*.txt *.csv *.json *.tsv')])
        auto=self.auto_covers.get()
        if path:self.run(lambda:bot.import_list(path,self.library,auto))
    def open_folder(self):
        self.library.mkdir(parents=True,exist_ok=True)
        import os
        if hasattr(os,'startfile'):os.startfile(self.library)
    def sync(self):
        host=self.ip.get().strip()
        try:bot.local_host(host)
        except ValueError as error:messagebox.showerror('IP requise',str(error));return
        self.run(lambda:(bot.sync_library(self.library,host,2121),'Fiches et jaquettes transférées. Appuyez sur Carré sur la PS5.')[1])
    def transfer(self):
        selected=self.table.selection()
        if not selected:messagebox.showinfo('Choix requis','Choisissez un jeu dans la liste.');return
        host=self.ip.get().strip();mode=self.mode.get()
        try:bot.local_host(host);entry=bot.choose(self.library,selected[0])
        except ValueError as error:messagebox.showerror('Paramètres',str(error));return
        if entry['kind']!='pkg':messagebox.showerror('Type de contenu','Choisissez une fiche de type pkg.');return
        action='demander son installation à HyperDPI' if mode=='hyperdpi' else 'copier le fichier par FTP, sans installation'
        if not messagebox.askyesno('Confirmer',f'{entry["title"]}\nTélécharger ce PKG sur le PC puis {action} sur {host} ?'):return
        def work():
            file=bot.download(entry,self.library,progress=lambda message:self.events.put(('progress',message)))
            if mode=='hyperdpi':bot.hyperdpi_upload(file,host,9090);return 'Demande acceptée par HyperDPI. Vérifiez le résultat sur la console.'
            with bot.connect_ftp(host) as ftp:
                ftp.cwd('/data');bot.ensure_ftp_dir(ftp,'ps5-hub');bot.ensure_ftp_dir(ftp,'pkg')
                if file.name in {n for n,_ in ftp.mlsd()}:raise ValueError('Ce PKG existe déjà sur la console.')
                bot.upload_verified(ftp,file,file.name)
            return 'PKG transféré et contrôlé par FTP. Aucune installation demandée.'
        self.run(work)

if __name__=='__main__':
    import sys
    window=tk.Tk()
    if '--smoke-test' in sys.argv:window.withdraw()
    App(window)
    if '--smoke-test' in sys.argv:
        window.update_idletasks();window.destroy();print('Interface Windows construite avec succès.')
    else:window.mainloop()
