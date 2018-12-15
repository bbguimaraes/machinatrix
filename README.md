# machinatrix

A very silly [Matrix](https://matrix.org) bot written in C.

Current supported commands:

- `ping`: pong
- `word`: random word
- `damn`: random curse
- `abbr`: random de-abbreviation
- `bard`: random Shakespearean insult
- `dlpo <term>`: lookup etymology (DLPO)
- `wikt <term>`: lookup etymology (Wiktionary)

## building

A C compiler and a UNIX programming environment are required, along with the
following libraries (tested versions in parentheses):

- libcurl (7.55.1)
- tidy-html5 (5.6.0)

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
