# 5D Chess Game

A modern implementation of multidimensional chess featuring timeline mechanics, built with C++ and Raylib graphics library.

## Table of Contents
- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Building](#building)
- [Running](#running)
- [Project Structure](#project-structure)
- [Controls](#controls)
- [Features](#features)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

## Overview

This project implements a 5D Chess game with an advanced UI system featuring:
- Multi-timeline chess mechanics
- Interactive board visualization with highlighting and transitions
- Dynamic menu system with hierarchical navigation
- Undo/redo command system
- State-driven scene management
- Modular rendering pipeline

## Prerequisites

### macOS (Primary Support)
- **Xcode Command Line Tools**: Required for compilation
- **Homebrew**: Package manager for dependencies
- **C++20 Compatible Compiler**: GCC or Clang
- **Raylib**: Graphics and audio library

### Other Platforms
The project is primarily configured for macOS but can be adapted for Linux/Windows with appropriate dependency paths.

## Installation

### 1. Install Xcode Command Line Tools
```bash
xcode-select --install
```

### 2. Install Homebrew (if not already installed)
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 3. Install Dependencies
```bash
# Install Raylib graphics library
brew install raylib

# Verify installation
brew list raylib
```

### 4. Clone the Repository
```bash
git clone https://github.com/LLaammTTeerr/5DChess.git
cd 5DChess
```

## Building

The project uses a Makefile for compilation with automatic dependency detection.

### Quick Build & Run
```bash
make
```
This command will:
1. Compile all source files
2. Link with Raylib
3. Run the executable
4. Clean up the binary after execution

### Build Only (Without Running)
```bash
make compile
```

### Clean Build Files
```bash
make clean
```

### Manual Compilation
If you prefer manual compilation or need to debug build issues:
```bash
g++ -std=c++20 \
    -Iinclude -Iinclude/Commands -Iinclude/GameStates -Iinclude/Menu \
    -Iinclude/Render -Iinclude/Scene -Iinclude/Commands/Invoker \
    -Iinclude/GameStates/ConcreteGameStates -Iinclude/Scene/ConcreteScene \
    -I/opt/homebrew/include \
    $(find . -name "*.cpp") \
    -o run \
    -L/opt/homebrew/lib -lraylib \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio
```

## Running

After successful compilation:

### Option 1: Using Make (Recommended)
```bash
make
```

### Option 2: Direct Execution
```bash
./run
```

### Option 3: Build Then Run Separately
```bash
make compile
./run
```

## Project Structure

```
5DChess/
├── assets/                     # Game assets
│   ├── images/                # Piece and board textures
│   ├── fonts/                 # Custom fonts
│   ├── backgroundmusic/       # Audio files
│   ├── soundeffect/          # Sound effects
│   └── buttons/              # UI button graphics
├── include/                   # Header files
│   ├── Commands/             # Command pattern implementation
│   ├── GameStates/           # State pattern for game flow
│   ├── Menu/                 # Menu system (Composite pattern)
│   ├── Render/               # Rendering and view components
│   └── Scene/                # Scene management
├── src/                      # Source files
│   ├── Commands/
│   ├── GameStates/
│   ├── Menu/
│   ├── Render/
│   ├── Scene/
│   ├── main.cpp              # Entry point
│   └── chess.cpp             # Core game logic
├── makefile                  # Build configuration
└── README.md                 # This file
```

## Controls

### Menu Navigation
- **Mouse**: Click to select menu items
- **Hover**: Visual feedback on interactive elements

### Game Controls
- **Mouse Click**: Select pieces and move destinations
- **Mouse Hover**: Preview available moves
- **Menu Button**: Access in-game menu for save/load/settings

### Keyboard Shortcuts
- **ESC**: Toggle menu/back navigation
- **Space**: Confirm actions
- **Enter**: Execute selected commands

## Features

### Core Gameplay
- **5D Chess Mechanics**: Move pieces across time and parallel universes
- **Timeline Visualization**: Clear representation of temporal moves
- **Legal Move Highlighting**: Visual guides for valid moves
- **Undo/Redo System**: Full move history with branching support

### User Interface
- **Dynamic Menus**: Context-sensitive navigation
- **Multiple View Modes**: Button and list-based menu layouts
- **Smooth Transitions**: Animated piece movements and UI changes
- **Responsive Design**: Adaptive layouts for different screen sizes

### Technical Features
- **Modular Architecture**: Clean separation of concerns (MVC pattern)
- **Design Patterns**: Command, State, Strategy, Composite, Singleton
- **Resource Management**: Efficient asset loading and caching
- **Extensible Framework**: Easy addition of new features and game modes

## Troubleshooting

### Common Build Issues

#### 1. Raylib Not Found
```bash
# Verify Raylib installation
brew list raylib
# If not installed:
brew install raylib
```

#### 2. Compiler Not Found
```bash
# Install Xcode Command Line Tools
xcode-select --install
# Verify GCC/Clang
g++ --version
```

#### 3. Include Path Issues
Check that Homebrew installed Raylib in the expected location:
```bash
ls /opt/homebrew/include/raylib.h
ls /opt/homebrew/lib/libraylib.a
```

#### 4. Permission Errors
```bash
# Ensure execute permissions
chmod +x run
```

### Runtime Issues

#### 1. Missing Assets
Ensure you're running from the project root directory where `assets/` folder is located.

#### 2. Window Doesn't Open
- Check if other applications are using OpenGL
- Verify graphics drivers are updated
- Try running with different window flags

#### 3. Performance Issues
- Lower the FPS limit in `main.cpp`
- Reduce texture quality in `assets/images/`
- Check system resources

### Platform-Specific Notes

#### Linux
Modify the Makefile's `LDFLAGS` section:
```makefile
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

#### Windows
Use MinGW or Visual Studio with appropriate library paths for Raylib.

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style
- Use C++20 standards
- Follow the existing naming conventions
- Document new classes and methods
- Maintain the established design patterns

### Testing
- Test builds on macOS before submitting
- Verify all menu interactions work correctly
- Ensure no memory leaks with new features

## License

This project is part of an academic assignment for Object-Oriented Programming course.

## Acknowledgments

- **Raylib**: Graphics and audio library
- **HCMUS**: University of Science, VNU-HCM
- **Course**: Object-Oriented Programming (OOP)

---

For questions or issues, please open an issue on the GitHub repository or contact the development team.
