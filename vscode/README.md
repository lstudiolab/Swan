<p align="center"><img src="../assets/swan-logo.png" alt="Swan" width="180"></p>

# Swan for Visual Studio Code

This extension adds basic editor support for Swan source files.

## Included

- Language registration for `.swan`
- TextMate syntax highlighting
- Indentation and bracket behavior
- Snippets for common declarations and control-flow blocks

## Requirements

Install the Swan compiler separately and ensure `swan` is available on your `PATH`. The compiler installation and usage instructions are in the repository root README.

## Install the compiler

From the repository root, build the single production compiler and put it on PATH:

~~~sh
sh build.sh
mkdir -p ~/.local/bin
cp build/swanc ~/.local/bin/swan
export PATH="$HOME/.local/bin:$PATH"
swan --version
~~~

Add the PATH export to ~/.bashrc, ~/.zshrc, or your shell's startup file so VS Code can find the compiler after restarting.

## Build and install the editor extension

From the repository's vscode directory:

~~~sh
npm install
npm run package
code --install-extension swan-language-0.1.0.vsix
~~~

Reload VS Code and open a `.swan` file.

## Current scope

This is editor syntax support, not a language server. It does not provide completion, symbol navigation, refactoring, semantic highlighting, or inline diagnostics. Workspace tasks are provided by the repository's `.vscode/tasks.json` and invoke the installed compiler.
