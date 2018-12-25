# machinatrix

A very silly [Matrix](https://matrix.org) bot written in C.

To use it, create an account, start the `machinatrix_matrix` program, join a
room, and send commands prefixed with an IRC-style mention of the user name.
Current supported commands:

- `ping`: pong
- `word`: random word
- `damn`: random curse
- `abbr`: random de-abbreviation
- `bard`: random Shakespearean insult
- `dlpo <term>`: lookup etymology (DLPO)
- `wikt <term>`: lookup etymology (Wiktionary)
- `parl`: use unparliamentary language
- `tr`: translate word (Wiktionary)

## building

A C compiler and a UNIX programming environment are required, along with the
following libraries (tested versions in parentheses):

- libcurl (7.55.1)
- tidy-html5 (5.6.0)
- cjson (1.7.10)

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

A second binary, `machinatrix_matrix`, connects to the server, listens for new
messages, and replies.  It executes the `machinatrix` program to handle the
commands.

    $ ./machinatrix_matrix \
        --server matrix.example.com \
        --user @machinatrix:matrix.example.com \
        --token 123456
