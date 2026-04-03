# feditor
A fast, native C++ implementation of a text editor for Windows using DirectWrite.

Single file, under 1000 lines of code.
Zero third party libraries; only using standard Windows header files.

Provides clean, smooth and fast file loading and editing.

## Reason for making this project

This editor came out of an overnight session, where I basically just
(to a large extent) kept chatting with ChatGPT about different stuff we can
do with Windows DirectWrite - starting out with a blank window, then writing
text to the window, then loading files using memory mapping to obtain the text to
be written, and so on and so on, just adding stuff one at a time - mostly as
a mixture of manual editing, and copy-pasting snippets of code from ChatGPT.

Why do this? I just became very irritated with the fact that Emacs (the editor
I am using currently, and that I use in general for programming) is taking
such an enormous amount of time to simply load a file - something that should
be the most elementary of operations. I of course thought for a long time that
there is something wrong with my own config - so I tested that by throwing it all
away - and also be disabling all syntax highlighting (using fundamental mode),
and even desperately trying stuff like turning off the Antivirus Real-Time scanning.
However, the loading time still averages 1-2 seconds for extremely small files.
(NOTE: Before anyone comes out to attack me for going after Emacs, yes, I know
that most probably the issue is not in emacs itself, but something else that I just
have not figured out yet. I tried loading the sqlite3.h file using emacs, and it
loads almost instantly (I am using Tsoding's Simple C mode instead of the standard
C mode, which makes this much faster) - however I have a bunch of C++ files (where
I also use Simple C mode) that are just 100s of lines of code, but take 1-2 seconds
to load for some reason... Anyways, I just needed some motivation to see that
I could make an editor that did not have to suffer from such problems)

This project is not meant to "replace" Emacs, or vim, neovim, or
any other editor whatsoever. It is more of a "Proof of Concept" that fast loading
and fast editing is actually not just possible on Windows, but fundamentally very simple.

At the moment, the code for this program is literally just a single C++ file with
a touch under 1000 lines of code. This implements not just direct rendering of text
to a window, but also loading any file, saving files, showing a blinking cursor,
basic movement and typing, scrolling, and line numbers. Also, there's a minibuffer
(partly inspired by the design in emacs). Note: this code is entirely "ugly",
as in it is just a bunch of stuff "put together" messily, and there's absolutely
no optimization done anywhere. The only basic kind of performance handling is
that I am not re-rendering the editor at literally every single loop iteration,
but only when it is actually needed (due to loading file, or any user interaction).

## Usage

This is a minimal simplistic text editor, which just aims at doing two jobs:
- Load your files fast / instantly
- Edit your files smoothly

and yes, of course, save your files once you have made any changes.

When you open the editor, it just starts out with an empty "scratch" buffer.
You can write into this normally, but it won't be saved anywhere.

### Editing

- Arrow Keys / Mouse left click : Moving the cursor around
- Scrolling
- Normal character keys : Typing (either in the editor or minibuffer)

### Commands

- Ctrl O : Open a file (using the file browse dialog)
- Ctrl X + Ctrl F : Open a file by manually entering the path (in the minibuffer)
- Ctrl G : (When inside a minibuffer) Exit the minibuffer and clear it.
- Ctrl S : Save the file currently opened

### Minibuffer

At the bottom there is a small horizontal region provided, called a "minibuffer".
Here, any commands or special actions are recorded and viewable (for instance, if
entering the path to a file to be loaded, your typed path will show up here).

## Potential Future Work

A lot of stuff could be added to this - while keeping it bare minimal,
as a simple straightforward editor.

### Potential features that could be added

- Text selection, and actions on selected text
- Ctrl + Arrow Keys for "jump" motions of cursor
- Ctrl + A for Select all
- Ctrl + C, Ctrl + X, Ctrl + V, for copy, cut and paste
- Ctrl + Z for undo (must implement change tracking for this)
- Moving to top line, or the end of a file (Alt + < and Alt + >)
- Multiple cursors (using Ctrl + > and Ctrl + <)

### Performance Optimization

As of now (paradoxically) I have not worked on this, even though that
was kind of the whole point of this side-project of mine! But in a way,
it works out for me, because it shows that even a direct unoptimized
editor can be so fast. But yes, that said, working on improving upon
any bottlenecks in performance would be something that could be looked
into in the coming future.

In case anyone would like to contribute, they are free to do so.
Also, anyone could of course use this code to build upon it for their
own work, either using parts of it, or just forking the repo (if you
want to make your own editor, for instance).
