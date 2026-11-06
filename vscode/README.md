# Swan for Visual Studio Code

This extension adds basic editor support for Swan source files.

## Included

- Language registration for `.swan`
- TextMate syntax highlighting
- Indentation and bracket behavior
- Snippets for common declarations and control-flow blocks

## Requirements

Install the Swan compiler separately and ensure `swan` is available on your `PATH`. The compiler installation and usage instructions are in the repository root README.

## Build and install

~~~sh
npm install
npm run package
code --install-extension swan-language-0.1.0.vsix
~~~

Reload VS Code and open a `.swan` file.

## Current scope

This is editor syntax support, not a language server. It does not provide completion, symbol navigation, refactoring, semantic highlighting, or inline diagnostics. Workspace tasks are provided by the repository's `.vscode/tasks.json` and invoke the installed compiler.
