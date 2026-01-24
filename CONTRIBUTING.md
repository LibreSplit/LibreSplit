Contributing
============

Coding Style
------------

The project uses `clang-format` to check for coding style. You will need to format the files accordingly before opening a Pull Request.

If you want to format a file, you can use the `clang-format -i -- myfile.c` (where `myfile.c` is the name of the file to format) to format a file in-place.

Alternatively, you can configure your editor to use `clang-format` on save automatically.

Static Analysis and Linting
---------------------------

The project also uses `cppcheck` to check for common mistakes and errors in C code.

It checks for any undefined behavior and dangerous code constructs in an attempt to ensure we aren't introducing bugs into the LibreSplit codebase.

You can run it with `meson test -C build -v` to show the output of the code formatter and analysis tools.

Opening a PR
------------

- If you're thinking about implementing a big feature or undertaking some major refactoring of the project, you should discuss it first through issues or discussions;
- If you're fixing an open issue, remember to refer to such issue either in the PR description or in the commit message that fixes the issue properly;
- Try to describe what your Pull Request is about, what it does and the rationale behind it, where appropriate.

Generating the developer documentation
--------------------------------------

The code is commented in a way that allows the creation of developer documentation using [Doxygen](https://doxygen.nl).

First of all, you'll need to install Doxygen:

- Debian-based systems

    ```sh
    sudo apt update
    sudo apt install doxygen
    ```

- Arch-based systems

    ```sh
    sudo pacman -Sy
    sudo pacman -S doxygen
    ```

Then you can generate the documentation by running:

```sh
doxygen Doxyfile
```

From the root of the project. Doxygen will automatically generate HTML documentation and place it under the `developer_docs/html/` subdirectory.

To give it a quick read, you can use your favourite browser directly or spin up a quick local server using Python:

```sh
python -m http.server --bind 127.0.0.1 5000
```

And then navigating to `localhost:5000`.
