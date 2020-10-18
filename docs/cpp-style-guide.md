# SWOS porting C++ Style Guide

This document describes coding conventions used throughout SWOS port in attempt to unify the style.
New code should try to conform to these standards, so it is as easy to maintain as existing code.

## Naming Conventions

### Files

File names should be sequences of capitalized words, starting with upper case for files corresponding to a class, or
lower case otherwise. No prefix. Examples: `scanCodes.cpp`, `SoundSample.cpp`. Convention for directories is
CamelCase starting with lower case. Each class header-cpp file pair should include single declaration. Only exception is
if the class needs a helper or implementation (pImpl) class, then it can be included in the same file pair.

#### Platform specific files

Platform specific files should be moved to a separate directory, named
according to the platform. Files should have platform tag added to the name
as a suffix.
Currently three platform tags are planned:

- `win32` for Windows
- `osx` for OS X
- `linux` for Linux

For example

```
src/
  crashReporter/
    linux/
        crashReporterLinux.h
        crashReporterLinux.cpp
    osx/
        crashReporterOsx.h
        crashReporterOsx.cpp
    win32/
        crashReporterWin32.h
        crashReporterWin32.cpp
```


### Type names and variable names

Types and variables are to be named using CamelCase convention -- meaning they should be a sequence of capitalized,
concatenated words. Type variables should start with uppercase letter and variables and functions should start with
lowercase letter. Hungarian notation is not allowed.

Variable names should be expressive and convey information about the intended use. Avoid short or meaningless names
(e.g. `ns`, `ts`, `ppg1`) and abbreviations. Single character variable names are only adequate for counters and
temporaries, where the purpose of the variable is obvious. `for` loop control variable can be simply called `i`,
no need for `counter` or similar names.

```c++
// wrong
short cntr;
char prevScrId;

// correct
short control;
char previousScreenId;
```

Defer declaring a variable right until it is needed.

#### Use of prefixes

Constants should begin with lower-case letter `k`, such as `kMaxVolume`. Member and private module
variables should have `m_` prefix. Classes/structs/complex types should begin
with upper-case letter. There are no other restrictions
on variable naming, just use common sense. Names should be expressive, but not excessive (such as
`StartTrackingActivityDialogControllerImplementationManagerAbstractFactory`).

```c++
class EntryBackgroundSprite;
const char kInputControlsComment[] = "; 0 = none, 1 = keyboard, 2 = mouse, 3 = joypad/game controller";
enum TeamControls;
using SfxSamplesArray = std::array<SoundSample, kNumSoundEffects>;

class DeLorean
{
public:
    void resetFluxCapacitator();

private:
    FluxCapacitator m_fluxCapacitator;
};
```

## Formatting style

### Spaces

Language commands that are applied to expression should be separated from
the expression by a single white space. There should be no white space
between the opening parentheses and the first term, or between the last term
and the closing parenthesis. Within the parentheses, all terms and operators
should be separated by a single white space.

```c++
if (year == 1985)
while (speedBelow(88))
for (int i = 0; i < 1956; i++)
```

Functions should not be separated from their argument lists, with no spaces
between opening parenthesis and first argument, or between last argument and
closing parenthesis. Arguments should be separated by a comma followed by a
single white space. Empty argument list (i.e. no arguments) should be
represented by a pair of parentheses with no white space between them. There
should be no space between reference sign and reference type, and no space
between pointer variable and pointer sign.

```c++
// wrong
f ( x, y );
g(x,y);
h(){ }
setFullScreenResolution(
                            m_resolutions[i].first,
                            m_resolutions[i].second
                      );

// correct
f(x, y);
g(x, y);
h() {}
setFullScreenResolution(m_resolutions[i].first, m_resolutions[i].second);
```

Keep the line length below 130 characters. Single empty line is encouraged
between related lines of code, but vertical space should otherwise be used
sparingly. In general, the more code that fits on one screen, the easier it
is to follow and understand the control flow of the program.

```c++
int c = ::getch();
::isalnum(c);
::sprintf(formatString, c, name);

engine.setTargetDate(2015, 10, 21);
engine.accelerateToMph(88);

sprite.rotate(90.0);
```

### Identation

Use spaces instead of hard tabs. Tabs, and indentation, should be 4 spaces wide. *Do not* break indentation level.

```c++
// wrong
drawText(
         "This looks like a small program",
         120,
         200,
         true,
         RGB(255, 0, 0),
         RGB(0, 0, 255),
         ALIGN_CENTER_X |
         ALIGN_CENTER_Y
        );

// correct
drawText("This looks like a small program", 120, 200, true,
    RGB(255, 0, 0), RGB(0, 0, 255), ALIGN_CENTER_X | ALIGN_CENTER_Y);
```

### Definitions

Functions' and methods' definitions should be separated by single empty
lines. No special separators are required. If the declaration of function
contains default parameters, it is recommended that the definition header
contains these as comment:

```c++
int getIntValue(Data& data, int index, int offset /* = 0 */) { ... }
```

Reference sign (`&`) is written next to the type since it's part of it. Pointer
is a property of the variable and hence it's written next to it.

```c++
int get(const std::string& string, const char *ptr);
```

### Brackets

In case of functions and classes curly brackets should be on a line on their
own, not indented with respect to the code surrounding them. In case of if
statements and inline methods they should remain on the same line.
To avoid clutter it is recommended to omit unnecessary curly brackets on
single line statement unless there's a strong reason for including them.
Statement itself should remain indented. See Example at the end of the
document.

Some exceptions are complicated conditions spanning multiple lines, empty
body of a conditional statement and symmetry between conditions -- if either
branch contains multiple statements curly braces are to be used on both
branches.

```c++
// wrong
if (address.isEmpty()) {
    return false;
}

for (int i = 0; i < 10; i++) {
    log("%i", i);
}

// correct
if (address.isEmpty())
    return false;

for (int i = 0; i < 10; i++)
    log("%i", i);
```

Use round brackets around parts of mixed expressions, to denote operator
precedence you require, do so even if the precedence is the same as the one
defined by standard, e.g. `(a + b) >> 3`.

### Scope

Use scope resolution operator (`::`) when accessing symbols in global
namespace (e.g. calling operating system API's or standard C library
functions) to avoid ambiguities and to make the intent obvious. When
accessing class members it is not necessary to use `this` keyword.

## Code documentation

### Inline comments

Use comments only where necessary. Do not comment the obvious. Comment
everything that is not immediately clear, or better yet, think of a way to write it in a
more expressive way.

Comments should be written in well-composed English, as proper sentences
with capitalization and punctuation. Prefer C++ comments (`//`) over C-style
comments -- `/* … */`.

## Headers

### Header guards

Use the `#pragma once` idiom for multiple inclusions protection -- it's
faster, safer and supported by all major compilers it is still non-standard.
Do not use typical

```c++
#ifndef HEADER_NAME_H
#define HEADER_NAME_H
...
#endif
```

### Include order

Use the following order for file inclusion in .cpp files, for Foo.cpp file:

```c++
#include "Foo.h"            // <- first, corresponding header
#include "Application.h"    // <- files from the same project
...
#include "core.h"           // <- files from core and other projects
...
#include <Windows.h>        // <- platform and framework includes
...
#include <vector>           // <- C and C++ standard library includes
                            // <- empty line before definitions
...
```

The only exception to the above ordering is precompiled header file that has to be included first, if used.


## Language constructs

### Classes

Class declarations should contain no more than one public, protected, and
private section, in that order. Avoid public data members. Declare private
methods before the data members. Inline methods should be located in the
header file, marked as inline within the class declaration, with definition
stored after the class declaration. Do not place method definitions inside
header files of abstract classes. For non-abstract classes this can be
acceptable for very simple methods.

Prefer braces (`{}` -- uniform initialization syntax) over parentheses `()`
for object initialization to avoid ambiguities in parsing since compiler
might unexpectedly interpret it as a function declaration (famous C++
syntactic problem -- "most vexing parse").

Do not overuse implementation inheritance. Composition is often more
appropriate. Try to restrict use of inheritance to the "is-a" case:
MightyDuck subclasses Duck if it can reasonably be said that MightyDuck "is
a kind of" Duck.

Unless required by the design, avoid using protected member variables and
functions to reduce dependencies of derived classes on base class.

Explicitly annotate overrides of virtual functions or virtual destructors
with an `override` keyword to help the compiler report invalid usages. Use
C\++ keyword `default` instead of creating empty functions.

```c++
// wrong
class CBingzzle
{
public:
    CBingzzle() { /* VOID */ }
    virtual ~CBingzzle() { /* VOID */ }
};

// correct
class CBingzzle
{
public:
    CBingzzle() = default;
    virtual ~CBingzzle() = default;
};
```
```c++
struct CBingzzle
{
    bool canPurfle() const;
};

// wrong
struct CBlayzbeengher : Bingzzle
{
    bool canPurfle() const;
};

// correct
struct CBlayzbeengher : Bingzzle
{
    bool canPurfle() const override;
};
```

### General

- if speed matters, measure!
- do not ignore compiler warnings, fix any warnings your code generates
- prefer STL for all common data structures; use `<stdint.h>` `int` types
- use `std::vector`, use `std::map` only if you need map; avoid `std::list`
- platform specific code should be separated in auxiliary class, unless this
  imposes performance or readability penalty; all OS specific code should be
  enclosed in `#ifdef` -- `#endif` block, with the last `#else` section
  containing `#error` message for all unsupported platforms
- use C++ cast operators instead of C casts
- do not use `goto` unless necessary; even then take a step back and see if
  the code could be redesigned or restructured
- prefer `enum`'s and static `const`'s to `#define`'s
- in general -- do not use preprocessor macros, use `inline`'s instead if possible
- reuse as much as possible, especially use well established libraries
- avoid using `using namespace std`, be specific -- use `std::` prefix


### Error handling

Use `assert()`'s whenever applicable. Do not execute code within
`assert()`'s, or perform assignments or any operations with side effects,
as `assert()` translates to null statement in release build. If an explanation is required use following
tricks:
```c++
bool testbool = false;
assert(("Clipping is expected to be done", x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight));

assert((x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight) && "Clipping is expected to be done");
```

For compile-time assertions use `static_assert`.

Prefer `throw`-ing an exception over long chain of `if`'s checking return values, if the failure is a
hard error. The generated code is faster, contains no magic numbers/values, and the return path is more visible.

### Modern C\++

Use modern C\++ standards, such as C\++11 and above as much as possible, to the extent
supported by selected compiler. Especially use the following (all supported
in VS 2015 and higher):
- auto
- generic lambda's
- `std::move()`, `std::forward()`, and elisions
- `std::unique_ptr<>`, `std::make_unique<>()`, `std::shared_ptr<>`,
  `std::make_shared<>()`, `std::weak_ptr<>`, and other constructs from
  `<memory>` header
- standard algorithms from `<algorithm>` header, such as `find()`,
  `sort()`, `for_each()`, and other, together with lambda's; do not use
  predicates unless necessary
- ranged-based for loops: `for (auto& e : v)`, and `for (const auto& e : v)`
- initializers -- `char *p{ new char[200] }; std::vector v{ 1, 2, 3 };`
- `override`, `final`, `= default`, `= delete`, `noexcept`
- `nullptr`, `bool`, etc.

## Source control

Do not put binary, temporary, and intermediate files in source control repository, unless required -- such as
closed source libraries, etc. Especially avoid files such as .suo, .sdf, .obj, .exe, and folders like ipch, Debug,
and Release -- never include those in source control unless specifically required. Always write a proper commit
comment, in plain English.

## Example

```c++
class ExampleClass
{
public:
    ExampleClass();
    bool assign(const Data& data);
};

bool ExampleClass::assign(const Data& data)
{
    assert(data.isValid() && "Invalid data passed to assign()");

    if (!data.isValid())
        throw std::invalid_argument("Invalid Data");

    switch (data.type()) {
    case Data::kNormal:
        // ...handle normal data...
        break;
    case Data::kAdvanced:
        {
            int count = 0;
            while (data.gotMore())
                count++;
            // ...
        }
        break;
    default:
        break;
    }
}
```

## CMake

Usage of CMake is strictly forbidden. For cross-platform build management only alternatives may be used.

## Exceptions to the rules

Use common sense, in cases where strictly following a rule makes your code look bad, exceptions are allowed.

## Disclaimer

Author hereby disclaims liability for any personal injury,
property or other damage, of any nature whatsoever, whether special,
indirect, consequential, or compensatory, directly or indirectly
resulting from the publication, use of, or reliance upon this document.
Author does not warrant or represent the accuracy or
content of the material contained in this document, and expressly disclaims
any express or implied warranty, including any implied warranty of
merchantability or fitness for a specific purpose, or that the use of the
material contained in this document is free from patent infringement. This
document must only be used for good. Never for evil. Eat lot of fruits.
Sit straight. Exercise.
