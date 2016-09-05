# Frelay

Frelay is a simple software to transfer individual files between clients
connected to an intermediate relay server. It consists of a server
component, an interactive command line client, and an optional GUI client
front-end.


## Description

#### Motivation

Frelay grew out of the desire to complement the [Mumble](https://www.mumble.info)
VoIP application with a simple tool that allows to send single files to
other users during calls.

#### What it is

* Frelay strives to mimic, at least to some degree, the interactive
file transfer feature that can be found in some well known (proprietary)
VoIP applications; a feature which is not (and most likely never will be)
implemented in Mumble. It relies on persistent client-server TCP
connections to perform this task.

#### What it is not

* Frelay is *not* a replacement for FTP, SFTP, SCP, IRC-DCC or comparable
conventional file transfer mechanisms.

* Frelay does *not* perform direct client-to-client transfers tunneling
NAT layers or firewalls. (The frelay server must reside in a network
reachable by all clients.)

* Frelay is *not* a way to securely transfer files. Do *not* use it to
transmit sensitive data!

* Frelay is *not* a chat software. (Although the `PING` command can be
abused to send short text messages to another client.)

* Frelay does *not* provide any means to subdivide users in distinct
groups, i.e. it has no notion of concepts like channels or rooms.


## Installation

The C code has no dependencies beyond the C standard library; it should
build cleanly with both `gcc` and `clang` in C99 mode. The frelay-gui
front-end depends on `python3` and `python3-tk`, preferably version 3.4
or higher.

#### Linux

```
$ git clone https://github.com/irrwahn/frelay.git
$ make install
```

#### FreeBSD

```
$ git clone https://github.com/irrwahn/frelay.git
$ gmake install
```

Note: The build environment can be tweaked by editing `config.mk`.


## Usage

#### Server

Create a suitable configuration file using `frelaysrv.sample.conf` as
template, then start `frelaysrv`.

### Client

To use the command line client, create a suitable configuration file
using `frelayclt.sample.conf` as a template, then start `frelayclt`.
Alternatively, to use the GUI frontend, start `frelay-gui` and configure
it as desired.

Clients connected to the same server can now offer individual files to
each other, and in turn decide to accept or decline an offer.


## Future features

Features, that *might* be added in future versions, include:

* Multicast and omnicast file transfers
* Support for MingW builds


## Release History

| Version  | Comment              |
| -------- | -------------------- |
| 0.0.1    | Work in progress     |


## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request

See `CREDITS` for a list of people who have contributed to the project.


## License

Frelay is distributed under the Modified ("3-clause") BSD License.
See `LICENSE` for more information.
