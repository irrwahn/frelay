#!/usr/bin/python3

from tkinter import *
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

connected = False
authed = False


###########################################
# GUI initialization
#

# Root window
root = Tk()
root.wm_title("Frelay")

# Peerlist
Label(root, text="Peers").grid(row=0, column=0)
peerlist = Listbox(root, width=30, height=15)
peerlist.grid(row=1, column=0, sticky=NW+SE)

# Transferlist
Label(root, text="Transfers").grid(row=0, column=1)
translist = Listbox(root, width=80, height=15)
translist.grid(row=1, column=1, sticky=NE+SW)

# Log display
ft_courier=('courier', 10,)
log = Text(root, width=126, height=25, font=ft_courier, state=DISABLED)
log.grid(row=2, columnspan=2, sticky=NE+SW)

# Command input
Command = Entry(root, width=112, font=ft_courier)
Command.grid(row=3, columnspan=2, sticky=W)
Command.focus_set()

def send_cmd(event=None):
    clt_write(Command.get())
    Command.selection_range(0, END)

sendbtn = Button(root, text='Send', command=send_cmd)
sendbtn.grid(row=3, column=1, sticky=E)
root.bind('<Return>', send_cmd)

# Status line
scol_neut = "#ddd"
scol_conn = "#ccf"
scol_disc = "#fcc"
scol_auth = "#cfc"
Status = Label(root, text="---", bg=scol_neut, fg="#000")
Status.grid(row=4, columnspan=2, sticky=NW+SE)

# Triggered to process client data
def proc_clt(event=None):
    subproc_clt()
    return False

root.bind('<<cltdata>>', proc_clt)


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
    log.insert(END, time.strftime('%H:%M:%S ') + line + "\n")
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
# Called from proc_clt(), which is bound to the <<cltdata>> virtual event
def subproc_clt():
    global connected
    global authed
    while True :
        try:  b = readq.get_nowait()
        except Empty:
            break
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
                authed = False
                connected = True
                Status.config(bg=scol_conn, text=line)
                logadd(line)
            elif pfx == 'DISC':
                authed = False
                connected = False
                Status.config(bg=scol_disc, text=line)
                logadd(line)
            elif pfx == 'AUTH':
                authed = True
                Status.config(bg=scol_auth, text=line)
                logadd(line)
            elif pfx == 'NAUT':
                authed = False
                Status.config(bg=scol_conn, text=line)
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
    if authed:
        clt_write("list");
    else:
        translist.delete(0,END)
    root.after(refresh_local_ms, subrefresh_local);

def subrefresh_remote():
    if authed:
        clt_write("peerlist");
    else:
        peerlist.delete(0, END)
    root.after(refresh_remote_ms, subrefresh_remote);

# Main loop
root.after(0, subrefresh_local);
root.after(0, subrefresh_remote);
root.mainloop()


# EOF
