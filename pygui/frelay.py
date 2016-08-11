#!/usr/bin/python3

from tkinter import *
import sys
from subprocess import PIPE, Popen
from threading  import Thread
from queue import Queue, Empty

ON_POSIX = 'posix' in sys.builtin_module_names


###########################################
# Configuration
#

refresh_local_ms = 1000
refresh_remote_ms = 3000

client_path = [ '../frelayclt' ]

connected = False
authed = False


###########################################
# Subprocess handling
#

# Read and enqueue any output from client
def enqueue_output(out, queue):
    for line in iter(out.readline, b''):
        queue.put(line)

# Run client instance
p = Popen(client_path, stdin=PIPE, stdout=PIPE, bufsize=1, close_fds=ON_POSIX)
q = Queue()
t = Thread(target=enqueue_output, args=(p.stdout, q))
t.daemon = True
t.start()

# Send a command to the client
def send_cmd(cmd):
    c = bytes(cmd + "\n", "utf-8")
    p.stdin.write(c)
    p.stdin.flush()
    #Log.insert(END, c)


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
Log = Text(root, width=126, height=25, font=ft_courier)
Log.grid(row=2, columnspan=2, sticky=NE+SW)

# Command input
Command = Entry(root, width=112, font=ft_courier)
Command.grid(row=3, columnspan=2, sticky=W)
Command.focus_set()
def send_Command(event=None):
    send_cmd(Command.get())
    Command.selection_range(0, END)
Button(root, text='Send', command=send_Command).grid(row=3, column=1, sticky=E)
root.bind('<Return>', send_Command)

# Status
scol_neut = "#ddd"
scol_conn = "#ccf"
scol_disc = "#fcc"
scol_auth = "#cfc"
Status = Label(root, text="---", bg=scol_neut, fg="#000")
Status.grid(row=4, columnspan=2, sticky=NW+SE)


###########################################
# GUI processing
#

srvname='*SRV*'

def id2name( peerid ):
    peerid = peerid.lstrip('0')
    if not peerid:
        return srvname
    items = peerlist.get(0,END)
    for item in items:
        sidx = item.find(' ')
        curid = item[:sidx].rstrip('* ')
        if curid == peerid:
            return item[sidx:].lstrip(' ')
    return peerid

# Process any queued client output
def subread():
    global connected
    global authed
    while True :
        # Triage lines based on prefix
        try:  lineb = q.get_nowait()
        except Empty:
            break
        else:
            line = lineb.decode(encoding='utf-8').strip()
            if len(line) > 4 and line[4] == ':' and line[:4].isupper():
                pfx = line[:4]
                line = line[5:]
            else:
                pfx = ''
        # Connection and login status
            if pfx == 'CONN':
                authed = False
                connected = True
                Status.config(bg=scol_conn, text=line)
                Log.insert(END, line + "\n")
            elif pfx == 'DISC':
                authed = False
                connected = False
                Status.config(bg=scol_disc, text=line)
                Log.insert(END, line + "\n")
            elif pfx == 'AUTH':
                authed = True
                Status.config(bg=scol_auth, text=line)
                Log.insert(END, line + "\n")
            elif pfx == 'NAUT':
                authed = False
                Status.config(bg=scol_conn, text=line)
                Log.insert(END, line + "\n")
        # Pings
            elif pfx == 'QPNG':
                cidx = line.find(':')
                if cidx != -1:
                    peerid=line[:cidx][-16:]
                    Log.insert( END, "[" + id2name(peerid) + "] " + line[cidx+2:].strip("'") + "\n" )
            elif pfx == 'RPNG':
                cidx = line.find(':')
                if cidx != -1:
                    peerid=line[:cidx][-16:]
                    Log.insert( END, "<" + id2name(peerid) + "> " + line[cidx+2:] + "\n" )
        # Server messages
            elif pfx == 'SMSG':
                Log.insert(END, '<' + srvname + '> ' + line + "\n")
        # Informational messages
            elif pfx == 'IMSG':
                Log.insert(END, line + "\n")
        # External command output
            elif pfx == 'COUT':
                Log.insert(END, line + "\n")
        # Peer list item
            elif pfx == 'PLST':
                line = line.lstrip('0')
                if not line:
                    peerlist.delete(0, END)
                else:
                    peerlist.insert(END, line)
        # Transfer list item
            elif pfx == 'TLST':
                if not line:
                    translist.delete(0, END)
                else:
                    translist.insert(END, line)
            elif pfx == 'OFFR':
                Log.insert(END, "ToDo offr: " + line + "\n")
            elif pfx == 'CERR':
                Log.insert(END, "ToDo cerr: " + line + "\n")
            elif pfx == 'LERR':
                Log.insert(END, "ToDo lerr: " + line + "\n")
            elif pfx == 'SERR':
                Log.insert(END, "ToDo serr: " + line + "\n")
        # Unhandled
            else:
                if not pfx: # MotD hack!
                    Log.insert(END, '<' + srvname + '> ' + line + "\n")
                else:
                    Log.insert(END, pfx + ':' + line + "\n")
    Log.see("end")
    root.after(200, subread)

# Scheduled refresh
def subrefresh_local():
    if authed:
        send_cmd("list");
    else:
        translist.delete(0,END)
    root.after(refresh_local_ms, subrefresh_local);

def subrefresh_remote():
    if authed:
        send_cmd("peerlist");
    else:
        peerlist.delete(0, END)
    root.after(refresh_remote_ms, subrefresh_remote);

# Main loop
root.after(100, subread)
root.after(200, subrefresh_local);
root.after(300, subrefresh_remote);
root.mainloop()








# scrapbook:

#from tkinter import *
#import tkinter as tk
#proc = subprocess.Popen([ '../frelayclt' ], stdout=subprocess.PIPE)

#Lb1 = Listbox(root, width=100, height=25)
#Lb1.pack()
    #print("hello")
    #Lb1.delete(0)
    #Lb1.insert(0, time.ctime())
    #stdout_value = proc.communicate()[0]
#Button(root, text='Transferlist', command=lambda cmd="list": send_cmd(cmd)).grid(row=1, column=1)
# or q.get(timeout=.1)
#Button(root, text='Peerlist', command=peers_refresh).grid(row=1, column=0)
#Button(root, text='Transferlist', command=trans_refresh).grid(row=1, column=1)
        #root.update_idletasks()
