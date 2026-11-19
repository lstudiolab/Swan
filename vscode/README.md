<p align="center"><img src="../assets/brew-logo.svg" alt="Brew" width="180"></p>

# Brew for Visual Studio Code

This extension adds basic editor support for Brew source files.

## Included

- Language registration for `.brew`
- TextMate syntax highlighting
- Indentation and bracket behavior
- Snippets for common declarations and control-flow blocks

## Requirements

Install the Brew compiler separately and ensure `brewc` is available on your `PATH`. The compiler installation and usage instructions are in the repository root README.

## Install the compiler

From the repository root, build the single production compiler and put it on PATH:

~~~sh
sh build.sh
mkdir -p ~/.local/bin
cp build/brewc ~/.local/bin/brewc
export PATH="$HOME/.local/bin:$PATH"
brewc --version
~~~

Add the PATH export to ~/.bashrc, ~/.zshrc, or your shell's startup file so VS Code can find the compiler after restarting.

## Build and install the editor extension

From the repository's vscode directory:

~~~sh
npm install
npm run package
code --install-extension brew-language-0.1.0.vsix
~~~

Reload VS Code and open a `.brew` file.

## Current scope

This is editor syntax support, not a language server. It does not provide completion, symbol navigation, refactoring, semantic highlighting, or inline diagnostics. Workspace tasks are provided by the repository's `.vscode/tasks.json` and invoke the installed compiler.
