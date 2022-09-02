# The `mnu2h` programming guide

`mnu2h` is a mini-compiler for menu description (*.mnu) files. It is converting them into C++
header files which compile directly into SWOS binary menu format. The language is not Turing-complete.

The prerequisite to understanding menu description files is good understanding of SWOS menu system. It is
described within a file located in the SWOS documentation directory (`docs/SWOS/menus.txt`).
Examples of complete scripts can be found in `src/menus/mnu` directory.

Minimum required Python version is 3.6.

## Motivation

SWOS is a very menu-heavy game containing a plethora of menus, many of them quite elaborate.
Being able to express complex menus succinctly and iterate quickly is a great help in development.

SWOS menu technologies have come a long way since its modest beginnings. At first the only option was manual
modification of bytes in hex editor. Those edits were slow, painful and error-prone. More often than not it
would all end in a shameful defeat (i.e. crash).

SWOS++ upgraded it to a set of NASM macros to declare menus. It introduced quite a stability although it
wasn't ideal and suffered from some shortcomings. Later versions of NASM even introduced changes that broke
compatibility forcing a version freeze. `mnu2h` builds upon this foundation and takes it further.

## Command line usage

`mnu2h` is a command line tool, and requires two parameters: path to the input file and path to the output
file.
```
python mnu2h.py <input path> <output path>
```
Normally the input path is a path to a `*.mnu` file and the output path to a resulting C++ header.

## Output

Final output is a C++ header file. It contains a custom-built menu structure corresponding to SWOS menu
binary format, as well as any instance variable declared in the input file. The menu variable can then
be further passed to `showMenu()/activateMenu()/unpackMenu()` to potentially display the menu on the
screen.

Resulting header file is meant to be used as a private one-time include. Its name will be formed from the
menu name with ".mnu.h" extension appended, for example `Menu joypadConfigMenu {...}` ->
`joypadConfigMenu.mnu.h`. The C++ file that includes it is expected to provide the implementation for any
of the menu handler functions and declare any necessary variables. Usually it contains other supporting code
as well.

Structures and constants that `mnu2h` uses to construct menus can be found in `menuCodes.h` header.

## Syntax highlighting support

For syntax highlighting in Visual Studio a TextMate grammar is available in the `mnu.mh.tmLanguage` file.
It should to be copied from `project\vc++` to `%USERPROFILE%\.vs\Extensions\mnu2htmlanguage\mnu2h`.
Visual Studio automatically picks it up and adds color to `*.mnu` and `*.mh` files. Any previously opened
file may need to be reopened.

## Syntax

Main entities in the language are `Menu` and `Entry`.
Each source files consists of zero or more menus, and each menu holds zero or more entries.
Both menus and entries can have properties and variables.

Whitespace is not important, and the language elements may be spaced freely.

### Comments

Comments start with two slashes (`//`), just like in C++, and span until the end of the line. There are no
block comments.

### Menus

Menus consist of keyword `Menu` followed by an identifier (menu name), and a menu body, contained within
curly braces (`{}`). Menu name must be unique, and must not be a C++ reserved word.

#### Menu properties

Menu property assignments have the following syntax:

```
<property> : <value>
```

There are three types of menu properties: function, integer and default.

Function properties are:
- `onInit`
- `onReturn`
- `onDraw`
- `onDestroy`

`onInit`, `onReturn` and `onDraw` correspond to SWOS menu even handler functions. `onDestroy` is an
addition and is executed when the menu is about to be destroyed. Value is expected to be a proper C++ function
identifier. The function will be declared as `static void functionName()`, and the user is expected to
supply the definition in a C++ file. If the function already exists elsewhere, and doesn't require a
prototype, declaration can be suppressed by prefixing the function name with tilde (`~`). To use one of the
SWOS functions, add `swos.` prefix (e.g. `swos.SWOSMainMenuInit`). In this case declaration will also be
suppressed. To force an unprocessed literal name to be passed to C++, put it in quotes, for example:

```
onSelect: ~"SWOS::OptionsMenuSelected"
```

Without the quotes, the function name would be broken into tokens at the colons. System will correctly handle
any mixture of SWOS and native functions which is enabled by using v2 of the menu header.

There are no parameters to the functions, and one way of getting the menu is to invoke `getCurrentMenu()`.

Integer properties are:
- initialEntry
- x
- y

Their values can be expressions, and for `x` and `y` they will be evaluated immediately. `initialEntry` is
under delayed expansion as all entry references. x and y coordinates are added to the coordinates of every
entry and any other menu element. By default, SWOS sets x coordinate to 8, so any calculation has to take it
into account, or to reset it explicitly to zero.

Default properties are:
- defaultWidth/defWidth
- defaultHeight/defHeight
- defaultX/defX
- defaultY/defY
- defaultColor
- defaultTextFlags

These properties can be viewed as kind of pseudo-properties as they do not affect the menu itself. Instead,
they store the default values for the entry properties and apply them to each entry that doesn't specifically
override them.

Their values can be expressions, but the expansion is delayed, so that they can have different values
when evaluated under different entry contexts.

### Entries

Entries are the main constituents of menus. An entry is basically a collection of properties, contained
within curly braces (`{}`). Entry name is optional, and if omitted the entry will be anonymous. If given,
name must be unique, valid C++ identifier but not a reserved keyword.

#### Types of entries

Besides standard `Entry` type there are also `TemplateEntry` and `ResetTemplate` entry types. Contents of
template entry will be copied to any standard entry that follows it, until reset template entry is
encountered, at which point any new standard entry will be initialized as usual (with standard default
values).

Template entry mechanism is mostly used for filling large number of entries with identical default values,
usually `onSelect` handler, color, text style... Few examples where template entries are used are filename
entries for loading career files, team names when selecting teams, player names when viewing teams.

#### Entry properties

Allowed entry properties are:
* x
* y
* width
* height
* color
* textFlags
* invisible
* disabled
* leftEntry
* rightEntry
* upEntry
* downEntry
* directionLeft
* directionRight
* directionUp
* directionDown
* skipLeft
* skipRight
* skipUp
* skipDown
* controlsMask
* onSelect
* beforeDraw
* onReturn
* customDrawBackground
* text
* number
* sprite
* stringTable
* customDrawForeground

Certain properties are mutually exclusive. Entry can only have one of the following:
- text
- number
- sprite
- stringTable
- customDrawForeground

`mnu2h` will compare each given property value with the default one, and if there is no difference will not
emit record for that property.

Background/Foreground(Content)

##### `x`/`y`/`width`/`height` properties

Entry dimensions and coordinates are always present, and have zero value by default. Zero width/height entries
will cause the game to crash, so make sure to fill them with valid values. `mnu2h` aborts compilation in case
it encounters any such dimensionless entry. This behavior can be overridden using
[preprocessor](#preprocessor) directives `#allowZeroWidthHeightEntries` and
`#disallowZeroWidthHeightEntries`.

##### `text` property

Entry having text property is marked as a text entry and will display the given string when rendered.
Acceptable values include literal string constants, C++ string variables, as well as two special values:
`@kAlloc` and `@kNull`/`@kNullText` (synonyms).

`@kAlloc` will cause the menu system to allocate space for the string in the unused portion of the menu
buffer, and assign it to the entry's text pointer. Buffer length is 70 bytes (symbolic constant in `swos.h`:
`kStdMenuTextSize`) and is initialized to `"STDMENUTEXT"`. These buffer-allocated entries are commonly used
through out the SWOS when the entries are dynamically populated with some string processing.

`@kNull`/`@kNullText` will
leave entry pointer as null. Without intervention this would cause the game to crash, so it is used when the
entry text pointer will be dynamically assigned.

##### `stringTable` property

These kinds of entries have a string table associated with them. Only one string from the table can be active,
and is shown within the entry in the manner of standard text entry. Inside the SWOS, this table is represented
by the following variable sized structure:

<table>
    <tr><th>offset</th><th>size</th><th>name</th><th>description</th></tr>
    <tr><td>0</td><td>4</td><td>pointer to index variable</td>
        <td>this will determine which string is displayed within the entry</td></tr>
    <tr><td>4</td><td>2</td><td>starting index</td>
        <td>initial value, as well as the value which will get subtracted from the current index
            when fetching the string
        </td></tr>
    <tr><td>6</td><td>4*n</td><td>string pointers array</td>
        <td>array of pointers to actual strings (4 bytes each), this array  is indexed as:
            current index - starting index
        </td>
</table>

Declaration syntax grammar:
```'stringTable' ':' ('~' | 'swos.') <variable name> | <stringTable>
stringTable ::= '[' ['~' | 'extern'] <index variable>, [<starting index> ','] <stringList> ']'
stringList ::= <stringListElement>, {',' <stringListElement>}
stringListElement ::= <string>|<id>
```

If a variable is supplied, it will be used as an already constructed string table, and no additional code will
be emitted. It has to be prefixed with "`~`" or "`swos.`". "`~`" will identify the variable as available elsewhere
in C++ code, and "swos." prefix means the table comes from SWOS.

If the string table body is supplied, it comes within the square brackets. First the index variable is
expected, optionally followed by a starting index value, and finally followed by a list of strings, each of
which can be specified as string constant or a variable (C++ or SWOS).

If the starting index is not present, it is assumed to be zero.

`mnu2h` will declare string table structure, a variable, initialize it with the given data and connect it
with the appropriate menu entry.

Some examples:
```
stringTable: [ swos.g_allPlayerTeamsEqual, swos.aOff, swos.aOn ]
stringTable: [ ~m_gameStyle, "PC", "AMIGA" ]
stringTable: [
    swos.g_pitchType, -2,
    "SEASONAL", "RANDOM", "FROZEN", "MUDDY", "WET", "SOFT", "NORMAL", "DRY", "HARD"
]
stringTable: swos.pitchTypeStrTable
```

##### `multilineText` property

##### `boolOption` property

#### Entry references

* leftEntry
* rightEntry
* upEntry
* downEntry
* skipLeft
* skipRight
* skipUp
* skipDown

- delayed expansion
- can use forward references
...

#### Entry aliases

It is possible to create entry aliases inside the menus. Those aliases then behave in the same manner as
real entry names. The syntax is:
```
entryAlias = <alias>
```
As with entry name, alias must be a valid identifier and not a C++ reserved word. They must be unique among
other aliases and entries, and must point to a valid, already defined entry. Once created, an entry alias
can be used anywhere an entry name can be used, and the result of the expression will be the same as if the
associated entry name was used. They can be useful when working with dynamically created entry names:

```
kNumPlayers = 11

Menu PlayersMenu {
#repeat kNumPlayers
    Entry #join(player, #{i}:02) {
        // ...
    }
#endRepeat
}

entryAlias lastPlayer = #join(player, #{kNumPlayers - 1}:02)
```

### Default properties

### Variables

There are three types of variables: global, menu and preprocessor.

## Preprocessor

Preprocessor directives begin with hash (`#`) sign and are able to modify the input tokens before they reach
the parser. So it's possible, for example, to have dynamically calculated entry names, entries having width
corresponding to their text, and so on.

### \#include "\<path\>"

\#include expects a quoted path to a file that will be included. The path is relative to the input file path,
i.e. the directory where the input file is located.

Once the file is located, it will be tokenized and all its tokens inserted at the current token index.

Examples:
```
#include "common.mh"
#include "includes/common.mh"
```

### \#textWidth

The syntax is:
```
#textWidth(<string>[, <boolean>])
```
The width of the supplied string will be calculated and it will replace all the tokens of the directive.
Boolean parameter decides whether the small or big font is used for measuring. If it's omitted, the default
is small.

Example:
```
Entry customWidth {
    width: #textWidth("999999") + 4     // ensure that the entry can display 6 digits
    // ...
}
```

### \#textHeight

The syntax is:
```
#textWidth([<string>[, <boolean>]])
```
The height of the supplied string will be calculated and it will replace all tokens of the directive. Boolean
parameter decides whether the small or big font is used for measuring. If it's omitted, the default is small.

Both the string and the boolean parameter are optional, since the height of SWOS characters doesn't depend on
the text itself. It may be included, for example for documentation purposes.

Example:
```
Entry increaseButton {
    height: #textHeight("+") + 2    // ensure the entry can fit single-line string vertically
    // ...
}
```

### \#repeat

Scans for `#repeatEnd` marker and repeats all the tokens in between by a given number of times.

Syntax:
```
#repeat <number of iterations> [=> <loop variable>]
<tokens to repeat>...
#repeatEnd
```

Number of iterations can be an expression, but only of integer type. It is followed by an optional parameter,
`=>` and a name of loop variable. If no loop variable is specified, it will default to one of standard `i`,
`j`, `k`, whichever is available. If they're all taken an error will be reported.

After the loop is done `#repeat/repeatEnd` directive tokens are removed from the stream and the loop
variable is destroyed.

`#repeat`'s can be arbitrarily nested, as long as loop variables are distinct.

Examples:
```
// create a column of 5 button entries
#repeat 5
Entry #join(button, #{i + 1}:02) {  // button_01, button_02, ...
    x: 100
    y: 100 + #i * 20
    // ...
}
```
```
// create a 5 rows x 2 columns entry matrix
kMaxColumns = 2
kMaxEntriesPerColumn = 5

#repeat kMaxColumns => column
    #repeat kMaxEntriesPerColumn => row
        Entry {
            x: 20 + #column * 80        // (20, 80),  (100, 80)
            y: 80 + #row * 20           // (20, 100), (100, 100)
            // ...                      // (20, 120), (100, 120)
        }                               // (20, 140), (100, 140)
    #endRepeat                          // (20, 160), (100, 160)
#endRepeat
```

### \#join

Creates a single token by concatenating a comma separated list of tokens. Whole construct is then replaced
by the resulting token.

Syntax:
```
#join(<token>{, <token>})
```

Any preprocessor expressions among the tokens will be evaluated. Whitespace separates tokens, so in order to
have spaces in the result strings need to be used. They are joined without quotes, and quotes need to be
added explicitly to end up in the final result.

Examples:
```
nike = #join(Mc, Fly)                           // McFly
play = #join('"Auf ', Wiedersehen, ' Monty"')   // "Auf Wiedersehen Monty"
#repeat kMaxNumJoypads
    Entry #join(joypadEntry, #i) {              // joypadEntry0, joypadEntry1, ...
        // ...
    }
#endRepeat
```

### \#print

Prints one or more comma separated expressions that follow the directive. Filename and line number will be
prepended, in the format `<filename>(<line>):`. Each expression will be quoted. Main purpose is debugging.

Syntax:
```
#print <expression> {, <expression>}
```
Examples:
```
#{i = 10}
#print #{!0}, #{1 + 1}, #{32 >> 1}, #{3 * i}    // example.mnu(2): `1', `2', `16', `30'
#print "We gotta move these refrigerators"      // example.mnu(3): `We gotta move these refrigerators'
#print labelEntry.x                             // check on the actual x coordinate
tmp = labelEntry.x
#print #{tmp}                                   // force evaluation
```

### \#assert

Evaluates a preprocessor expression, and if the result is false halts the program execution. True result is
ignored without any output.

Syntax:
```
#assert <preproc-expression>
```
Examples:
```
#assert width > 0
#assert 1               // always ignored
#assert 0               // always stops the compilation
```

### \#warningsOn and \#warningsOff

These two directives are used to control whether `mnu2h` shows or suppresses warnings. They have no
parameters and function like simple switches. The state is global, so any included file can change it for
everyone else.

### Preprocessor expressions

Preprocessor expressions support majority of C operators (and a few additional ones) and are calculated
immediately. Operand type can be string or integer. If an operation doesn't support given types an error will
be reported and compilation aborted. Result of the successful calculation is injected back into the token
stream. Strings are copied as is, while integers are converted to strings.

Operator precedence table (from highest to lowest):

| Operator group | Description |
|----------------|-------------|
|`++ -- []` |post-increment and decrement, array indexing|
|`~ ! + - ++ -- int str`|bitwise/logical NOT, unary plus/minus, pre-increment and pre-decrement, type conversions|
|`* / %`|multiplicative operators|
|`+ -`|binary plus and minus|
|`<< >> <<`| |`>>`|shift and rotate left and right|
|`< > <= >=` |relational operators|
|`== !=`|equality operators|
|`&`|bitwise AND|
|`\|`|bitwise OR|
|`&&`|logical AND|
|`\|\|`|logical OR|
|`?:`|ternary operator|
|`+= -= *= \/= %= <<= <<=\| >>= \|>>= &= ^= \|= =`|assignment operators|
|`,`|comma operator|

Note that all the operators are left-to-right associative, except for unary operators (excluding postfix
increment/decrement) assignment operators and ternary operator. All the operators in a group have equal
priority.

<!--
warnings about uninitialized variables
string indexing
-->

Syntax for expression evaluation is `#eval(<expression>)` or `#{<expression>}`.

Expression evaluator uses values of global and menu variables, so it's possible to use their values such as:
```
kWidth = 200
// ...
x: #{kWidth / 2 - 20}
```

If a value of non-preprocessor variable couldn't be evaluated, an error will be reported.

#### Format specification

A preprocessor expression can be followed by a format specification, which can modify the expression output
which will be seen by the parser. It has a format:
```
#{<expression>}:<format specification>
```
or
```
#eval(<expression>):<format specification>
```
It can be letter 'q', which will instruct the preprocessor to quote the result of the expression.
It can be letter 't', which will result in expression result being tokenized (by default no tokenization
occurs and strings get passed to the parser untouched).
If not 'q' or 't', format is expected to be a Python string specifier, and works in exactly the same way.

Examples:
```
#{"Surface " + "Slam"}:q        // "Surface Slam"
#{"Masters of the Universe"}:t  // parser gets 4 tokens: Masters, of, the, Universe
#{10}:b                         // 1010
```

#### Digit separator

Optional single quote (`'`) may be inserted between the digits as a separator. The quotes are ignored by the
compiler. A common usage is to separate thousands, but anything else is permitted.
```
#print #{10000}         // next 3 lines all print "10000"
#print #{10'000}        // the usual way
#print #{1'0'0'0'0}     // nothing prevents you from writing it this way if you fancy
#print #{0x1'000}       // works with hexadecimals too
```

#### Identifier to string conversion

Any identifier prefixed with a colon will be turned into string. For example:
```
#print #{:High + :tower}    // `Hightower'
```
Any non-valid identifier following a colon is considered an error.

#### String indexing

Strings can be indexed via integer expressions inside the brackets `[]`. Result is a string consisting of
a single character at the position indicated by the bracketed expression. Indices start with zero. Positive
and zero indices count from the first character, negative from the last. If the index is out of range, an
empty string is returned.

```
#print #{"balkanization"[7]}        // example.mnu(1): `z'
#print #{"balkanization"[3]}        // example.mnu(2): `k'
#print #{"balkanization"[-6]}       // example.mnu(3): `z'
#print #{"balkanization"[-10]}      // example.mnu(4): `k'
#print #{"balkanization"[9001]}     // example.mnu(5): `'
#print #{"balkanization"[-9001]}    // example.mnu(6): `'
```

#### Rotation operators

Operators for bitwise and string rotation are an addition compared to C. Their integer operands are assumed
to be 32-bit.

Strings can also be shifted and rotated. Shifting deletes number of characters specified from the left or
right end of the string, while rotation rotates the characters.

<!-- todo: examples -->
```
```

<!--
lowerCaseStringsWarningOn
lowerCaseStringsWarningOff
#allowZeroWidthHeightEntries
#disallowZeroWidthHeightEntries
... However, in case of some testing or experiments it might be useful to let such code run.
standard default entry values are =? [table]
about exports
dot variables + endX/Y
menu property aliases
parser expressions
standard strings indexing & operations
onRestore <~> onReturn alias
a word about tokenization?
da li templejt kopira width? ne, cini mi se
-->
