#!/usr/bin/python3

from tkinter import *
from tkinter import messagebox, filedialog
import os
import sys
import time
import random
from subprocess import PIPE, Popen, call
from threading  import Thread, Lock #, Condition
from queue import Queue, Empty

ON_POSIX = 'posix' in sys.builtin_module_names


###########################################
# Configuration
# TODO: save/load config
#

# server pseudo-nick for display
srvalias='*SRV*'

# transfer list update period
refresh_local_ms = 997
# peer list update period
refresh_remote_ms = 2011

# connection info
user = os.getenv('USER', "user%d"%(random.randint(100, 999)))
password = '********'
server = 'localhost'
port = '64740'

# frelay client incantation
client_path = [ '../frelayclt',
                '-c', './frelayclt.conf',
                '-u', user, '-p', password,
                server, port,
                '-w', './frelayclt.conf' ]

# default working directory for frelay client
cwd = '.'

# name of FIFO to additionally read commands from
pipe_name = '/tmp/frelayctl'

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

# Enable internal accept dialog?
notify_internal = True


###########################################
# Global state
#

is_connected = False
is_authed = False


###########################################
# Dialogs
# TODO: These should really be refactored into proper classes!
#

def dlg_make_entry(parent, caption, width=None, text=None, **options):
    Label(parent, text=caption).pack(side=TOP)
    entry = Entry(parent, **options)
    if width:
        entry.config(width=width)
        entry.pack(side=TOP, fill=BOTH)
    if text:
        entry.insert(0,text)
    return entry

# Connect dialog
def connectdlg():
    res=False
    def destroy():
        dlg.grab_release()
        dlg.destroy()
    def kenter(event=None):
        cbtn.invoke()
    def kescape(event=None):
        qbtn.invoke()
    def savesrv():
        global server
        global port
        nonlocal res
        server = srv.get()
        port = prt.get()
        res=True
        destroy()
    dlg = Toplevel(root)
    dlg.transient( root )
    dlg.grab_set()
    dlg.title('Connect')
    dlg.geometry("+%d+%d" % (root.winfo_pointerx(), root.winfo_pointery()))
    frame = Frame(dlg, padx=10, pady=10)
    frame.pack(fill=BOTH, expand=YES)
    srv = dlg_make_entry(frame, "Server:", 32, server)
    prt = dlg_make_entry(frame, "Password:", 16, port)
    cbtn = Button(dlg, text="Connect", padx=10, pady=10, command=savesrv)
    cbtn.pack(side=LEFT)
    qbtn = Button(dlg, text="Cancel", padx=10, pady=10, command=destroy)
    qbtn.pack(side=RIGHT)
    srv.focus_set()
    dlg.bind('<Return>', kenter)
    dlg.bind('<Escape>', kescape)
    root.wait_window(dlg)
    return res

# Login dialog
def logindlg():
    res=False
    def destroy():
        dlg.grab_release()
        dlg.destroy()
    def savecreds():
        global user
        global password
        nonlocal res
        user = usr.get()
        password = pwd.get()
        res=True
        destroy()
    def ukenter(event=None):
        pwd.focus_set()
        pwd.selection_range(0, END)
    def pkenter(event=None):
        lbtn.invoke()
    def kescape(event=None):
        qbtn.invoke()
    dlg = Toplevel(root)
    dlg.transient(root)
    dlg.grab_set()
    dlg.title('Login')
    dlg.geometry("+%d+%d" % (root.winfo_pointerx(), root.winfo_pointery()))
    dlg.geometry=('+' + str(root.winfo_pointerx()) + '+' + str(root.winfo_pointery()))
    frame = Frame(dlg, padx=10, pady=10)
    frame.pack(fill=BOTH, expand=YES)
    usr = dlg_make_entry(frame, "User name:", 16, user)
    pwd = dlg_make_entry(frame, "Password:", 16, password, show="*")
    lbtn = Button(dlg, text="Login", padx=10, pady=10, command=savecreds)
    lbtn.pack(side=LEFT)
    qbtn = Button(dlg, text="Cancel", padx=10, pady=10, command=destroy)
    qbtn.pack(side=RIGHT)
    usr.focus_set()
    usr.bind('<Return>', ukenter)
    pwd.bind('<Return>', pkenter)
    dlg.bind('<Escape>', kescape)
    root.wait_window(dlg)
    return res


###########################################
# GUI initialization
#

# Root window and frames
root = Tk()
root.wm_title("Frelay")

btnframe = Frame(root)
btnframe.pack(fill=X)

listframe = Frame(root, height=8)
listframe.pack(fill=BOTH, expand=YES)
listframe.columnconfigure(0, weight=1)
listframe.columnconfigure(2, weight=4)
listframe.rowconfigure(1, weight=1)

logframe = Frame(root, height=15)
logframe.pack(fill=BOTH, expand=YES)

consframe = Frame(root)
consframe.pack(fill=X)

statframe = Frame(root)
statframe.pack(fill=X)

# Buttons
# Connect button
def do_connect(event=None):
    if is_connected:
        clt_write('disconnect')
    elif connectdlg():
        clt_write('connect ' + server + ' ' + port )
connbtn = Button(btnframe, text='Connect', width=12, state=NORMAL, command=do_connect)
connbtn.pack(side=LEFT)
# Login button
def do_login(event=None):
    if is_authed:
        clt_write('logout')
    elif logindlg():
        clt_write('login ' + user + ' ' + password )
loginbtn = Button(btnframe, text='Login', width=12, state=DISABLED, command=do_login)
loginbtn.pack(side=LEFT)
# Quit button
def do_quit(event=None):
    root.destroy()
def ask_quit():
    if messagebox.askokcancel("Quit", "Quit frelay?"):
        do_quit()
quitbtn = Button(btnframe, text='Quit', width=12, command=ask_quit)
quitbtn.pack(side=RIGHT)

# Lists
# Peerlist
Label(listframe, text="Peers").grid(row=0, column=0)
plscrollbar = Scrollbar(listframe, orient=VERTICAL, width=10)
peerlist = Listbox(listframe, yscrollcommand=plscrollbar.set)
peerlist.grid(row=1, column=0, sticky=W+E+N+S)
plscrollbar.grid(row=1, column=1, sticky=N+S)
plscrollbar.config(command=peerlist.yview)

# Transferlist
Label(listframe, text="Transfers").grid(row=0, column=2)
tlscrollbar = Scrollbar(listframe, orient=VERTICAL, width=10)
translist = Listbox(listframe, yscrollcommand=tlscrollbar.set)
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
scol_neut = "#ddd"
scol_conn = "#ccf"
scol_disc = "#fcc"
scol_auth = "#cfc"
connstat = Label(statframe, padx=10, text="Not connected", bg=scol_disc, fg="#000")
connstat.pack(side=LEFT, fill=X)
pingstat = Label(statframe, padx=10, text="", bg=scol_neut, fg="#000")
pingstat.pack(side=LEFT, fill=X)


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
root.bind('<Alt-c>', do_connect)
root.bind('<Alt-l>', do_login)

# Quit program with [X] or Alt-F4
root.bind('<Control-q>', do_quit)
root.protocol("WM_DELETE_WINDOW", ask_quit)


###########################################
# Peerlist operations
#

peerlist_lock = Lock()

def id2name(peerid):
    peerid = peerid.lstrip('0')
    if not peerid:
        return srvalias
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
        item = peerlist.get(int(peerlist.curselection()[0]))
        tok = item.split(None, 2)
        if tok[0].endswith('*'):
            return
        filename = filedialog.askopenfilename(parent=root,
                        initialdir=cwd,
                        title='Choose a file to offer @' + tok[1] + ':')
        if filename:
            clt_write('offer ' + tok[0] + ' ' + filename)

peerlist.bind('<<ListboxSelect>>', peerlist_select)


###########################################
# Log operations
#

def logadd(line):
    log.config(state=NORMAL)
    log.insert(END, time.strftime('%H:%M:%S ') + line.expandtabs(8) + "\n")
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

proc = Popen(client_path, stdin=PIPE, stdout=PIPE, bufsize=1, close_fds=ON_POSIX)
readq = Queue()
readthread = Thread(target=clt_read, args=(proc.stdout, readq, root))
readthread.daemon = True
readthread.start()

# Create a named pipe and a thread to read from it
if not os.path.exists(pipe_name):
    os.mkfifo(pipe_name)

def pipe_read(): # need to first open fd as rw to avoid EOF on read
    pipein = os.fdopen(os.open(pipe_name, os.O_RDWR), 'r')
    for line in iter(pipein.readline, b''):
        clt_write(line.strip())

pipethread = Thread(target=pipe_read)
pipethread.daemon = True
pipethread.start()

# Send a command to the client, after some preparation
clt_write_lock = Lock()
def clt_write(line):
    with clt_write_lock:
        line = line.strip()
        tok = line.split(None, 2)
        cmd = tok[0].lower()
        if cmd == 'quit':
            do_quit()
        elif cmd == 'offer' and tok[1][0] == '@':
            line = 'offer ' + name2id(tok[1][1:]) + ' ' + tok[2]
        elif cmd == 'ping' and tok[1][0] == '@':
            line = cmd + ' ' + name2id(tok[1][1:])
            if len(tok) > 2:
                line = line + ' ' + tok[2]
        b = bytes(line + "\n", "utf-8")
        proc.stdin.write(b)
        proc.stdin.flush()

# External offer notification
def notify(offerid, peername, filename, filesize):
    if notifier:
        call([w.replace('%o', offerid).replace('%p', peername)
                .replace('%n', filename).replace('%s', filesize)
                .replace('%%', '%') for w in notifier])
    if notify_internal and messagebox.askyesno('Offer received',
            peername + ' offered file ' + filename + ' (' + filesize
             + ')\n Accept offer?'):
        clt_write( 'accept ' + offerid )

# Process queued client output
# Called via root.after() from proc_clt(), which in turn
# is bound to the <<cltdata>> virtual event. Sigh.
def subproc_clt():
    global is_connected
    global is_authed
    try:  b = readq.get_nowait()
    except Empty:
        return
    else:
        logscrl = True
        line = b.decode(encoding='utf-8').strip()
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
                logadd("[" + peername + "] " + line[cidx+2:].strip("'"))
        elif pfx == 'RPNG':
            cidx = line.find(':')
            if cidx != -1:
                peername=id2name(line[:cidx][-16:])
                pingstat.config(text='<' + peername + '> ' + line[cidx+2:])
    # Server messages
        elif pfx == 'SMSG':
            logadd('[' + srvalias + '] ' + line)
    # Informational messages
        elif pfx == 'IMSG':
            logadd(line)
    # Current directory info
        elif pfx == 'WDIR':
            global cwd
            cwd = line
            logadd('CWD is: ' + cwd)
    # External command output
        elif pfx == 'COUT':
            logadd(line)
    # Peer list item
        elif pfx == 'PLST':
            peerlist_update(line)
            logscrl = False
    # Transfer list item
        elif pfx == 'TLST':
            if not line:
                translist.delete(0, END)
            else:
                translist.insert(END, line)
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
            logadd("Command error: " + line)
        elif pfx == 'LERR':
            logadd("Local error: " + line)
        elif pfx == 'SERR':
            logadd("Server error: " + line)
    # Unhandled
        else:
            if not pfx: # MotD hack!
                logadd('[' + srvalias + '] ' + line)
            else:
                logadd(pfx + ':' + line)
        if logscrl:
            log.see("end")

# Scheduled list refreshing
def subrefresh_local():
    if is_authed:
        clt_write("list");
    else:
        translist.delete(0, END)
    root.after(refresh_local_ms, subrefresh_local)

def subrefresh_remote():
    if is_authed:
        clt_write("ping 0");
        clt_write("peerlist");
    else:
        peerlist.delete(0, END)
    root.after(refresh_remote_ms, subrefresh_remote)

# Main loop
# Determine and lock root window's minimum size
root.update()
root.minsize(root.winfo_width(), root.winfo_height())
root.after(0, subrefresh_local)
root.after(0, subrefresh_remote)
clt_write( 'cd ' + cwd )
root.mainloop()


# EOF
