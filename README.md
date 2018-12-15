# machinatrix

A very silly [Matrix](https://matrix.org) bot written in C.

Current supported commands:

- `ping`: pong

## building

A C compiler and a UNIX programming environment are required.

## running

The main program, `machinatrix`, is responsible for executing the commands and
can be tested without involving the Matrix server.  Commands are accepted as
arguments.  If none is provided, they are read from `stdin`.

    $ ./machinatrix ping
    pong
    $ ./machinatrix
    ping
    pong
    ping
    pong
