#!/usr/bin/python3

#
# frelay-gui.py
#
# Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer
#   in the documentation and/or other materials provided with the
#   distribution.
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#


###########################################
# Module imports
#

import os
import sys
import getopt
import time
import random
from subprocess import PIPE, Popen, call
from threading  import Thread, Lock
from queue import Queue, Empty
from configparser import RawConfigParser
from tkinter import *
from tkinter import messagebox, filedialog

ON_POSIX = 'posix' in sys.builtin_module_names


###########################################
# Configuration
#

# Connection configuration
server = 'localhost'
port = '64740'

# Client configuration
user = os.getenv('USER', "user%d"%(random.randint(100, 999)))
password = 'dummy'
auto_login = True
# Frelay client incantation
#client_cmd = [ '../frelayclt',
#                '-c', './frelayclt.conf',
#                '-u', user, '-p', password,
#                server, port,
#                '-w', './frelayclt.conf' ]
client_cmd = [ 'frelayclt', '-u', user, '-p', password, server, port ]
# Default working directory for frelay client
work_dir = '.'

# Control configuration
# Path of FIFO to additionally read commands from
cmd_pipe = '/tmp/frelayctl'
# Enable internal accept dialog?
notify_internal = True
# External tool to invoke for offer notification.
# This can also be used to call an auto-accept script.
# The following placeholders are substituted:
#   %o  full offer ID
#   %p  peer name
#   %n  file name
#   %s  file size
#   %%  literal '%' character
# Examples:
#   notifier = [ 'notify-send', 'New offer', '%o\n %p: %n (%s)' ]
#   notifier = [ './autoaccept.sh', '%o', '%p', '%n', '%s' ]
notifier = []

# Evaluate command line
cfg_filename = None
try:
  opts, args = getopt.getopt(sys.argv,"c:",[])
except getopt.GetoptError:
  print('frelay-gui -c <config_file>\n')
  sys.exit(2)
for opt, arg in opts:
  if opt == '-c':
     cfg_filename = arg

# Read config file
if not cfg_filename or not os.path.isfile(cfg_filename):
    home = os.getenv('HOME', '.')
    cfg_basename = 'frelay-gui.conf'
    cfg_filename = home + '/.config/frelay/' + cfg_basename
    if not os.path.isfile(cfg_filename):
        cfg_filename = home + '/.' + cfg_basename

class ListConfigParser(RawConfigParser):
    def getlist(self, section, option):
        value = self.get(section, option)
        return list(filter(None, (x.strip() for x in value.splitlines())))
#    def getlistint(self, section, option):
#        return [int(x) for x in self.getlist(section, option)]

cfg_config = ListConfigParser()
cfg_config.read(cfg_filename)
if 'connection' in cfg_config.sections():
    server = cfg_config['connection'].get('server', server)
    port = cfg_config['connection'].get('port', port)
if 'client' in cfg_config.sections():
    user = cfg_config['client'].get('user', user)
    password = cfg_config['client'].get('password', password)
    auto_login = cfg_config['client'].getboolean('auto_login', auto_login )
    client_path = cfg_config['client'].get('client_path', '')
    if client_path:
        client_cmd[0] = client_path
    work_dir = cfg_config['client'].get('work_dir', work_dir )
if 'control' in cfg_config.sections():
    cmd_pipe = cfg_config['control'].get('cmd_pipe', cmd_pipe)
    notify_internal = cfg_config['control'].getboolean('notify_internal', notify_internal)
    noti = cfg_config['control'].get('notifier')
    if noti:
        notifier = list(filter(None, (x.strip() for x in noti.splitlines())))

# Write config file
def cfg_write_file(event=None):
    global cfg_config
    cfg_config['connection'] = {
                'server' : server,
                'port' : port }
    cfg_config['client'] = {
                'user' : user,
                'password' : password,
                'auto_login' : str(auto_login),
                'client_path' : client_cmd[0],
                'work_dir' : work_dir }
    cfg_config['control'] = {
                'cmd_pipe' : cmd_pipe,
                'notify_internal' : str(notify_internal),
                'notifier' : '\n' + '\n'.join(notifier)}
    with open(cfg_filename, 'w') as cfg_file:
        cfg_config.write(cfg_file)

# Config dialog
def dlg_mkentry(parent, caption, width=None, text=None, **options):
    l = Label(parent, text=caption)
    l.grid(sticky=W)
    entry = Entry(parent, **options)
    if width:
        entry.config(width=width)
    entry.grid(row=l.grid_info()['row'], column=1, padx=6, pady=6, sticky=W+E)
    if text:
        entry.insert(0, text)
    return entry

def configdlg(parent):
    res=False
    def destroy():
        dlg.grab_release()
        dlg.destroy()
    def cfgsave():
        global server
        global port
        global user
        global password
        global auto_login
        global client_cmd
        global work_dir
        global cmd_pipe
        global notify_internal
        global notifier
        nonlocal res
        server = srv.get()
        port = prt.get()
        user = usr.get()
        password = pwd.get()
        auto_login = alog.get()
        client_cmd[0] = clp.get()
        work_dir = wdr.get()
        cmd_pipe = pip.get()
        notifier = ntf.get().strip("'").split("', '")
        notify_internal = nint.get()
        cfg_write_file()
        res=True
        destroy()
    def kenter(event=None):
        sbtn.invoke()
    def kescape(event=None):
        cbtn.invoke()
    #
    dlg = Toplevel(parent)
    dlg.transient(parent)
    dlg.grab_set()
    dlg.title('Configuration')
    geom = parent.winfo_geometry().split('+')
    dlg.geometry("+%d+%d" % (int(geom[1]) + 50, int(geom[2])))
    dlg.bind('<Return>', kenter)
    dlg.bind('<Escape>', kescape)
    frame = Frame(dlg, padx=10, pady=10)
    frame.pack(fill=BOTH, expand=YES)
    frame.columnconfigure(1, weight=1)
    #
    srv = dlg_mkentry(frame, 'Server:', 40, server)
    prt = dlg_mkentry(frame, 'Port:', 12, port)
    usr = dlg_mkentry(frame, 'User:', 12, user)
    pwd = dlg_mkentry(frame, 'Password:', 12, password, show='*')
    alog = BooleanVar()
    Checkbutton(frame, text='Auto-login on startup', variable=alog,
                onvalue = True, offvalue = False).grid(column=1, sticky=W)
    alog.set(auto_login)
    Label(frame, text=' ').grid()
    clp = dlg_mkentry(frame, 'Client Path:', 40, client_cmd[0] )
    wdr = dlg_mkentry(frame, 'Working dir:', 40, work_dir )
    pip = dlg_mkentry(frame, 'Commandd pipe:', 40, cmd_pipe )
    ntf = dlg_mkentry(frame, 'External Notifier:', 40,
                        "'" + "', '".join(notifier) + "'" )
    nint = BooleanVar()
    Checkbutton(frame, text='Internal notifier enabled', variable=nint,
                onvalue = True, offvalue = False).grid(column=1, sticky=W)
    nint.set(notify_internal)
    #
    sbtn = Button(dlg, text='Apply & Save', padx=10, pady=10, command=cfgsave)
    sbtn.pack(side=LEFT)
    sbtn.focus_set()
    cbtn = Button(dlg, text='Cancel', padx=10, pady=10, command=destroy)
    cbtn.pack(side=RIGHT)
    dlg.update()
    dlg.minsize(dlg.winfo_width(), dlg.winfo_height())


###########################################
# Internal configuration
#

# Transfer list update period
refresh_local_ms = 997
# Peer list update period
refresh_remote_ms = 2011
# Number of backscroll lines for log display
log_backscroll = 500

###########################################
# Global state
#

is_connected = False
is_authed = False


###########################################
# GUI initialization
#

# Root window and frames
root = Tk()
root.wm_title('Frelay')

btnframe = Frame(root)
btnframe.pack(fill=X)

listframe = Frame(root, height=8)
listframe.pack(fill=BOTH, expand=YES)
listframe.columnconfigure(0, weight=1)
listframe.columnconfigure(2, weight=10)
listframe.rowconfigure(1, weight=1)

logframe = Frame(root, height=15)
logframe.pack(fill=BOTH, expand=YES)

consframe = Frame(root)
consframe.pack(fill=X)

statframe = Frame(root)
statframe.pack(fill=X)

# Buttons
# Connect button
def helpbtn_cb(event=None):
    print('TODO: Help dialog\n')
helpbtn = Button(btnframe, text='?', state=NORMAL, command=helpbtn_cb)
helpbtn.pack(side=LEFT)

# Connect button
def do_connect(event=None):
    clt_write('connect ' + server + ' ' + port )
def connectbtn_cb(event=None):
    if is_connected:
        clt_write('disconnect')
    else:
        do_connect()
connbtn = Button(btnframe, text='Connect', width=12, state=NORMAL, command=connectbtn_cb)
connbtn.pack(side=LEFT)

# Login button
def do_login(event=None):
    clt_write('login ' + user + ' ' + password )
def loginbtn_cb(event=None):
    if is_authed:
        clt_write('logout')
    else:
        do_login()
loginbtn = Button(btnframe, text='Login', width=12, state=DISABLED, command=loginbtn_cb)
loginbtn.pack(side=LEFT)

# Cwd button
def cwdbtn_cb(event=None):
    global work_dir
    new_dir = filedialog.askdirectory(parent=root, initialdir=work_dir,
                        title='Choose a new working directory')
    if new_dir:
        work_dir = new_dir
        clt_write('cd ' + work_dir)
cwdbtn = Button(btnframe, text='Change Dir', width=12, state=NORMAL, command=cwdbtn_cb)
cwdbtn.pack(side=LEFT)

# Quit button
def do_quit(event=None):
    os.remove(cmd_pipe)
    root.destroy()
def ask_quit():
    if messagebox.askokcancel('Quit', 'Quit frelay?'):
        do_quit()
quitbtn = Button(btnframe, text='Quit', width=12, command=ask_quit)
quitbtn.pack(side=RIGHT)

# Config button
def confbtn_cb(event=None):
    configdlg(root)
confbtn = Button(btnframe, text='Config', width=12, command=confbtn_cb)
confbtn.pack(side=RIGHT)


# Lists
# Peerlist
Label(listframe, text='Peers').grid(row=0, column=0)
plscrollbar = Scrollbar(listframe, orient=VERTICAL, width=10)
peerlist = Listbox(listframe, yscrollcommand=plscrollbar.set)
peerlist.grid(row=1, column=0, sticky=W+E+N+S)
plscrollbar.grid(row=1, column=1, sticky=N+S)
plscrollbar.config(command=peerlist.yview)

# Transferlist
Label(listframe, text='Transfers').grid(row=0, column=2)
tlscrollbar = Scrollbar(listframe, orient=VERTICAL, width=10)
translist = Listbox(listframe, width=100, yscrollcommand=tlscrollbar.set)
translist.grid(row=1, column=2, sticky=W+E+N+S)
tlscrollbar.grid(row=1, column=3, sticky=N+S)
tlscrollbar.config(command=translist.yview)

# Log display
ft_courier=('courier', 10,)
logscrollbar = Scrollbar(logframe, orient=VERTICAL, width=10)
log = Text(logframe, width=100, height=12, font=ft_courier, state=DISABLED,
            yscrollcommand=logscrollbar.set)
log.pack(side=LEFT, fill=BOTH, expand=YES)
logscrollbar.pack(side=RIGHT, fill=Y)
logscrollbar.config(command=log.yview)

# Command input
cmdinput = Entry(consframe, font=ft_courier)
cmdinput.pack(side=LEFT, fill=X, expand=YES)
cmdinput.focus_set()

# Status line
scol_neut = '#ddd'
scol_conn = '#ccf'
scol_disc = '#fcc'
scol_auth = '#cfc'
connstat = Label(statframe, padx=10, text='Not connected', bg=scol_disc, fg='#000')
connstat.pack(side=LEFT, fill=X)
pingstat = Label(statframe, padx=10, text='', bg=scol_neut, fg='#000')
pingstat.pack(side=LEFT, fill=X)
cwdstat = Label(statframe, padx=10, text='', bg=scol_neut, fg='#000')
cwdstat.pack(side=RIGHT, fill=X)


###########################################
# Global bindings
#

# Triggered to process client data
def proc_clt(event=None):
    root.after(0, subproc_clt) # Not called directly to avoid flickering!
root.bind('<<cltdata>>', proc_clt)

# Triggered by hitting ENTER key
def send_cmd(event=None):
    clt_write(cmdinput.get())
    cmdinput.selection_range(0, END)
root.bind('<Return>', send_cmd)

# Key assignments
root.bind('<Alt-h>', helpbtn_cb)
root.bind('<F1>', helpbtn_cb)

root.bind('<Alt-c>', connectbtn_cb)
root.bind('<F2>', connectbtn_cb)

root.bind('<Alt-l>', loginbtn_cb)
root.bind('<F3>', loginbtn_cb)

root.bind('<Alt-d>', cwdbtn_cb)
root.bind('<F4>', cwdbtn_cb)

root.bind('<Alt-g>', confbtn_cb)
root.bind('<F5>', confbtn_cb)

root.bind('<Control-q>', do_quit)

# Quit program with [X] or Alt-F4
root.protocol('WM_DELETE_WINDOW', do_quit)


###########################################
# Listbox operations
#

peerlist_lock = Lock()

def id2name(peerid):
    peerid = peerid.lstrip('0')
    if not peerid:
        return server
    with peerlist_lock:
        items = peerlist.get(0,END)
        for item in items:
            tok = item.split(None, 2)
            if tok[0].rstrip('*') == peerid:
                return tok[1]
        return peerid

def name2id(peername):
    peername = peername.strip()
    with peerlist_lock:
        items = peerlist.get(0,END)
        for item in items:
            tok = item.split(None, 2)
            if tok[1] == peername:
                return tok[0].rstrip('*')
        return 'ffffffffffffffff'

def peerlist_update(line):
    if peerlist_lock.acquire(False):
        line = line.lstrip('0')
        if not line:
            peerlist.delete(0, END)
        else:
            peerlist.insert(END, line)
        peerlist_lock.release()

def peerlist_select(event=None):
    with peerlist_lock:
        if peerlist.size() > 0:
            item = peerlist.get(int(peerlist.curselection()[0]))
            tok = item.split(None, 2)
            if tok[0].endswith('*'):
                return
            filename = filedialog.askopenfilename(parent=root,
                            initialdir=work_dir,
                            title='Choose a file to offer @' + tok[1] + ':')
            if filename:
                clt_write('offer ' + tok[0] + ' "' + filename + '"')

peerlist.bind('<<ListboxSelect>>', peerlist_select)


translist_lock = Lock()

def translist_update(line):
    if translist_lock.acquire(False):
        if not line:
            translist.delete(0, END)
        else:
            if 'D' == line[0]:
                tdir = '[from '
            else:
                tdir = '[to '
            progress = int(line.split("' ", 1)[1].split('.',1)[0])
            if progress == 0:
                col = '#ffc'
            else:
                col = '#cfc'
            line = tdir + id2name(line.split(',',2)[1]) + '] ' + line
            translist.insert(END, line)
            translist.itemconfig(END, bg=col)
        translist_lock.release()

def translist_select(event=None):
    with translist_lock:
        if translist.size() > 0:
            item = translist.get(int(translist.curselection()[0])).split('] ',1)[1]
            tok = item.split(" '", 1)
            offerid = tok[0]
            offertype = offerid[0]
            peername = id2name(offerid.split(',')[1])
            tok = tok[1].split("' ", 1)
            filename = tok[0]
            tok = tok[1].split()
            filesize = tok[0].split('/')[1]
            if offertype == 'D':
                r = messagebox.askyesnocancel('Download', peername
                            + ' offered file ' + filename + ' (' + filesize
                            + ')\nStart download? (No will remove it!)')
                if r == True:
                    clt_write( 'accept ' + offerid )
                elif r == False:
                    clt_write( 'remove ' + offerid )
            elif offertype == 'O':
                if messagebox.askyesno('Upload', filename + ' ('
                            + filesize + ') for ' + peername
                            + '.\nRemove upload? (No will keep it.)'):
                    clt_write( 'remove ' + offerid )

translist.bind('<<ListboxSelect>>', translist_select)


###########################################
# Log operations
#

def logadd(line):
    log.config(state=NORMAL)
    nlines = int(log.index('end-1c').split('.')[0])
    if nlines > log_backscroll:
        log.delete('0.0', '10.0')
    log.insert(END, time.strftime('%H:%M:%S ') + line.expandtabs(8) + '\n')
    log.config(state=DISABLED)

def logclear():
    log.config(state=NORMAL)
    log.delete(1.0, END)
    log.config(state=DISABLED)


###########################################
# Processing
#

# Run client instance and start reader thread
def clt_read(out, queue, root):
    for line in iter(out.readline, b''):
        queue.put(line)
        root.event_generate('<<cltdata>>') # trigger GUI processing

proc = None
try:
    proc = Popen(client_cmd, stdin=PIPE, stdout=PIPE, bufsize=1, close_fds=ON_POSIX)
except Exception as e:
    logadd('[GUI] Unable to start client process: Popen(' + client_cmd[0] + '): ' + str(e))
else:
    readq = Queue()
    readthread = Thread(target=clt_read, args=(proc.stdout, readq, root))
    readthread.daemon = True
    readthread.start()

# Create a named pipe and a thread to read from it
def pipe_read():
    # need to first open fd as rw to avoid EOF on read
    try:
        pipein = os.fdopen(os.open(cmd_pipe, os.O_RDWR), 'r')
    except Exception as e:
        logadd('[GUI] Warning: cannot open command pipe: fdopen(' + cmd_pipe + '): ' + str(e))
    else:
        for line in iter(pipein.readline, b''):
            clt_write(line.strip())

if not os.path.exists(cmd_pipe):
    try:
        os.mkfifo(cmd_pipe)
    except Exception as e:
        logadd('[GUI] Warning: unable to create command pipe: mkfifo(' + cmd_pipe + '): ' + str(e))
    pipethread = Thread(target=pipe_read)
    pipethread.daemon = True
    pipethread.start()
else:
    logadd("[GUI] Warning: Will not open command pipe, path '" + cmd_pipe + "' already exists!")
    logadd("[GUI] Warning: Is there another instance running?")

# Write one single line (command) to the client
def clt_write_raw(line):
    if proc:
        proc.stdin.write(bytes(line + '\n', 'utf-8'))
        proc.stdin.flush()

# Send a command to the client, after some preprocessing
clt_write_lock = Lock()
def clt_write(line):
    with clt_write_lock:
        line = line.strip()
        tok = line.split(None, 2)
        cmd = tok[0].lower()
        if cmd == 'quit':
            do_quit()
        elif cmd == 'offer' and len(tok) > 2 and tok[1][0] == '@':
            line = 'offer ' + name2id(tok[1][1:]) + ' ' + tok[2]
        elif cmd == 'ping' and len(tok) > 1:
            if tok[1].startswith('@'):
                tok[1] = name2id(tok[1][1:])
            if len(tok) > 2:
                lines = tok[2].splitlines()
                for l in lines: # split multi-line PINGs
                    clt_write_raw(cmd + ' ' + tok[1] + ' "' + l + '"')
                return
            else:
                line = tok[0] + ' ' + tok[1]
        # Eliminate all line breaks.
        line = ''.join(line.splitlines())
        clt_write_raw(line)

# External offer notification
def notify(offerid, peername, filename, filesize):
    if notifier:
        call([w.replace('%o', offerid).replace('%p', peername)
                .replace('%n', filename).replace('%s', filesize)
                .replace('%%', '%') for w in notifier])
    if notify_internal:
        r = messagebox.askyesnocancel('Offer received', peername
                        + ' offered file ' + filename + ' (' + filesize
                        + ')\nStart Download? (No will remove it!)')
        if r == True:
            clt_write( 'accept ' + offerid )
        elif r == False:
            clt_write( 'remove ' + offerid )

# Process queued client output
# Called via root.after() from proc_clt(), which in turn
# is bound to the <<cltdata>> virtual event. Sigh.
last_pfx = ''
def subproc_clt():
    global is_connected
    global is_authed
    global work_dir
    global last_pfx
    try:
        b = readq.get_nowait()
    except Empty:
        return
    else:
        logscrl = True
        line = b.decode(encoding='utf-8').strip()
    # Note: We are vulnerable to a 'Notice of Death' attack, but there
    # is no clean solution, apart from crippling the frelayclt code.
        if len(line) > 4 and line[4] == ':' and line[:4].isupper():
            pfx = line[:4]
            line = line[5:]
        else:
            pfx = ''
    # Triage lines based on prefix
    # Connection and login status
        if pfx == 'CONN':
            is_authed = False
            is_connected = True
            connstat.config(bg=scol_conn, text=line)
            connbtn.config(text='Disconnect')
            loginbtn.config(state=NORMAL)
            logadd(line)
        elif pfx == 'DISC':
            is_authed = False
            is_connected = False
            connstat.config(bg=scol_disc, text=line)
            connbtn.config(text='Connect')
            loginbtn.config(state=DISABLED, text='Login')
            logadd(line)
        elif pfx == 'AUTH':
            is_authed = True
            connstat.config(bg=scol_auth, text=line)
            loginbtn.config(text='Logout')
            logadd(line)
        elif pfx == 'NAUT':
            is_authed = False
            connstat.config(bg=scol_conn, text=line)
            loginbtn.config(text='Login')
            logadd(line)
    # Pings
        elif pfx == 'QPNG':
            cidx = line.find(':')
            if cidx != -1:
                peername=id2name(line[:cidx][-16:])
                last_pfx = '[' + peername + '] '
                logadd(last_pfx + line[cidx+2:].strip("'"))
        elif pfx == 'RPNG':
            cidx = line.find(':')
            if cidx != -1:
                peername=id2name(line[:cidx][-16:])
                pingstat.config(text='<' + peername + '> ' + line[cidx+2:])
                logscrl = False
    # Server messages
        elif pfx == 'SMSG':
            last_pfx = '[' + server + '] '
            logadd( last_pfx + line)
    # Informational messages
        elif pfx == 'IMSG':
            logadd(line)
    # Current directory info
        elif pfx == 'WDIR':
            global work_dir
            work_dir = line
            cwdstat.config(text=work_dir)
            logadd('CWD: ' + work_dir)
    # External command output
        elif pfx == 'COUT':
            logadd(line)
    # Peer list item
        elif pfx == 'PLST':
            peerlist_update(line)
            logscrl = False
    # Transfer list item
        elif pfx == 'TLST':
            translist_update(line)
            logscrl = False
    # Offer received
        elif pfx == 'OFFR':
            tok = line.split()
            offerid = tok[0]
            peername = id2name(offerid.split(',')[1])
            filename = tok[1]
            filesize = tok[2]
            notify(offerid, peername, filename, filesize)
        elif pfx == 'DSTA' or pfx == 'DFIN' or pfx == 'DERR' or pfx == 'UFIN':
            tok = line.split(None,1)
            peername = id2name(tok[0].split(',')[1])
            logadd('<' + peername + '>: ' + tok[1])
    # General errors
        elif pfx == 'CERR':
            logadd('Command error: ' + line)
        elif pfx == 'LERR':
            logadd('Local error: ' + line)
        elif pfx == 'SERR':
            logadd('Server error: ' + line)
    # Continuation
        elif not pfx:
            logadd(last_pfx + line)
    # Unhandled
        else:
            logadd(pfx + ':' + line)
        if logscrl:
            log.see('end')

# Scheduled list refreshing
def subrefresh_local():
    if is_authed:
        clt_write('list');
    else:
        translist.delete(0, END)
    root.after(refresh_local_ms, subrefresh_local)

def subrefresh_remote():
    if is_authed:
        clt_write('ping 0');
        clt_write('peerlist');
    else:
        peerlist.delete(0, END)
        pingstat.config(text='')
    root.after(refresh_remote_ms, subrefresh_remote)

# Main loop
# Determine and lock root window's minimum size
root.update()
root.minsize(root.winfo_width(), root.winfo_height())
# Change working directory
clt_write( 'cd ' + work_dir )
# Initialize periodic refreshes
root.after(3000, subrefresh_local)
root.after(1500, subrefresh_remote)
# Perform auto login
if auto_login:
    root.after(500, do_connect)
    root.after(1000, do_login)
root.mainloop()


# EOF
