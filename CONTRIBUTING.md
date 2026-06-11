# Contributing to Corsair Lighting Protocol

We love contributions! Here is how you can help:

## Code Style & Formatting
To keep the codebase clean and readable, please format all C++ changes using our `.clang-format` rules before committing and submitting a Pull Request:
```bash
npx clang-format -i src/*.cpp src/*.h examples/**/*.ino
```

## Creating Pull Requests
1. Fork the repository.
2. Create a clean feature branch.
3. Commit your changes with clear, descriptive commit messages.
4. Ensure the GitHub Actions tests compile successfully.
5. Submit a Pull Request targeting the `main` branch.

## Reporting Issues
* Please search the existing issues and discussions before opening a new one.
* When reporting bugs, include details about:
  * Your Arduino board/microcontroller.
  * Your wiring setup (power supply details, resistors).
  * Your LED strip type and count.
  * Your operating system and Corsair iCUE software version.
