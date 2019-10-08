# mnu2h

mnu2h is a mini-compiler for menu (*.mnu) files. It is converting them into C++
header files which compile directly to SWOS binary menu format.

## Motivation

SWOS is a very menu-heavy games containing a plethora of menus, many quite complex.
Being able to express complex menus succinctly and iterate quickly is a great help in development.

SWOS menu technologies have come a long way since their modest beginnings, namely manually modifying bytes
in hex editor. Those edits were slow, painful and error-prone. More often than not it would all end in
a shameful defeat (i.e. crash).

SWOS++ was using set of NASM macros to declare menus. It introduced quite stability
although it wasn't perfect/had some shortcomings
Later versions of NASM introduced changes that broke compatibility forcing

## Syntax

Main entities in the language are `Menu` and `Entry`.
Each source files consists of zero or more menus, each itself holding zero or more entries.
Both menus and entries can have properties.

### Comments

Comments start with two slashes (`//`), just like in C++, and end at the end of the line. There are no block
comments.

### Menus

Menus consist of keyword `Menu` followed by an identifier (menu name), and a menu body, contained within
curly braces (`{}`).

#### Menu properties

### Entries

Name in entries is options, they can be anonymous.

#### Entry properties

Allowed entry properties are:
* x
* y
* width
* height
* color
...

### Default properties

### Variables

There are three types of variables: global, menu and preprocessor.

## Preprocessor

Preprocessor directives begin with hash (`#`) sign and are able to modify the input tokens before they reach
the parser.

### Preprocessor expressions

Preprocessor expressions support majority of C operators (and a few additional ones) and are calculated
immediately.

## Output

Final output is C++ header file.
