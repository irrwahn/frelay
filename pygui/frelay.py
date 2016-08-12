#!/usr/bin/python3

from tkinter import *
#from bwidget import *
import sys
import time
from subprocess import PIPE, Popen
from threading  import Thread, Lock, Condition
from queue import Queue, Empty

ON_POSIX = 'posix' in sys.builtin_module_names


###########################################
# Configuration
#

client_path = [ '../frelayclt' ]    # frelay client incantation
srvalias='*SRV*'                    # server pseudo-nick for display

refresh_local_ms = 997     # transfer list update period
refresh_remote_ms = 2011   # peer list update period


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
root.wm_title("Frelay")

btnframe = Frame(root)
btnframe.pack(fill=X)

listframe = Frame(root)
listframe.pack(fill=BOTH, expand=YES)
listframe.columnconfigure(0, weight=1)
listframe.columnconfigure(1, weight=3)
listframe.rowconfigure(1, weight=1)

consframe = Frame(root)
consframe.pack(fill=BOTH, expand=YES)

# Buttons
# Connect button
def do_connect(event=None):
    if is_connected:
        clt_write('disconnect')
    else:
        clt_write('connect')
connbtn = Button(btnframe, text='Connect', width=12, state=NORMAL, command=do_connect)
connbtn.pack(side=LEFT)
# Login button
def do_login(event=None):
    if is_authed:
        clt_write('logout')
    else:
        clt_write('login')
loginbtn = Button(btnframe, text='Login', width=12, state=DISABLED, command=do_login)
loginbtn.pack(side=LEFT)

# Lists
# Peerlist
Label(listframe, text="Peers").grid(row=0, column=0)
peerlist = Listbox(listframe)
peerlist.grid(row=1, column=0, sticky=W+E+N+S)
# Transferlist
Label(listframe, text="Transfers").grid(row=0, column=1)
translist = Listbox(listframe)
translist.grid(row=1, column=1, sticky=W+E+N+S)

# Log, command and status line
# Log display
ft_courier=('courier', 10,)
log = Text(consframe, width=50, height=4, font=ft_courier, state=DISABLED)
log.pack(side=TOP, fill=BOTH, expand=YES)
# Command input
Command = Entry(consframe, font=ft_courier)
Command.pack(side=TOP, fill=X)
Command.focus_set()
# Status line
scol_neut = "#ddd"
scol_conn = "#ccf"
scol_disc = "#fcc"
scol_auth = "#cfc"
status = Label(consframe, text="---", bg=scol_neut, fg="#000")
status.pack(side=BOTTOM, fill=X)


###########################################
# Bindings
#

# Triggered to process client data
def proc_clt(event=None):
    root.after(0, subproc_clt) # Not called directly to avoid flickering!
root.bind('<<cltdata>>', proc_clt)

# Triggered by hitting ENTER key
def send_cmd(event=None):
    clt_write(Command.get())
    Command.selection_range(0, END)
root.bind('<Return>', send_cmd)


###########################################
# Helper
#

def id2name(peerid):
    peerid = peerid.lstrip('0')
    if not peerid:
        return srvalias
    items = peerlist.get(0,END)
    for item in items:
        sidx = item.find(' ')
        curid = item[:sidx].rstrip('* ')
        if curid == peerid:
            return item[sidx:].lstrip(' ')
    return peerid

def logadd(line):
    log.config(state=NORMAL)
    log.insert(END, time.strftime('%H:%M:%S ') + line.expandtabs(8) + "\n")
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

# Send a command to the client
clt_write_lock = Lock()
def clt_write(cmd):
    with clt_write_lock:
        b = bytes(cmd + "\n", "utf-8")
        proc.stdin.write(b)
        proc.stdin.flush()

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
            status.config(bg=scol_conn, text=line)
            connbtn.config(text='Disconnect')
            loginbtn.config(state=NORMAL)
            logadd(line)
        elif pfx == 'DISC':
            is_authed = False
            is_connected = False
            status.config(bg=scol_disc, text=line)
            connbtn.config(text='Connect')
            loginbtn.config(state=DISABLED, text='Login')
            logadd(line)
        elif pfx == 'AUTH':
            is_authed = True
            status.config(bg=scol_auth, text=line)
            loginbtn.config(text='Logout')
            logadd(line)
        elif pfx == 'NAUT':
            is_authed = False
            status.config(bg=scol_conn, text=line)
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
                logadd("<" + peername + "> " + line[cidx+2:])
    # Server messages
        elif pfx == 'SMSG':
            logadd('[' + srvalias + '] ' + line)
    # Informational messages
        elif pfx == 'IMSG':
            logadd(line)
    # External command output
        elif pfx == 'COUT':
            logadd(line)
    # Peer list item
        elif pfx == 'PLST':
            line = line.lstrip('0')
            if not line:
                peerlist.delete(0, END)
            else:
                peerlist.insert(END, line)
            logscrl = False
    # Transfer list item
        elif pfx == 'TLST':
            if not line:
                translist.delete(0, END)
            else:
                translist.insert(END, line)
            logscrl = False
        elif pfx == 'OFFR':
            logadd("ToDo offr: " + line)
        elif pfx == 'CERR':
            logadd("ToDo cerr: " + line)
        elif pfx == 'LERR':
            logadd("ToDo lerr: " + line)
        elif pfx == 'SERR':
            logadd("ToDo serr: " + line)
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
        translist.delete(0,END)
    root.after(refresh_local_ms, subrefresh_local)

def subrefresh_remote():
    if is_authed:
        clt_write("peerlist");
    else:
        peerlist.delete(0, END)
    root.after(refresh_remote_ms, subrefresh_remote)

# Main loop
# Determine and lock root window's minimum size
root.update()
root.minsize(root.winfo_width(), root.winfo_height())
#root.after(100, do_connect)
root.after(0, subrefresh_local)
root.after(0, subrefresh_remote)
root.mainloop()


# EOF
