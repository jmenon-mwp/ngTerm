# ngTerm - GTK+ Terminal Manager

A GTK+ based terminal manager application that allows you to manage multiple terminal connections through a graphical interface.

## Features

- Organize terminal connections in folders
- Customizable connection settings
- Multiple terminal tabs
- Connection management through GUI
- Modern GTK+ interface

## Requirements

### Minimum Library Requirements

- GTK+ 3.0
- GTKmm 3.0 (C++ bindings for GTK+)
- Glibmm 2.4
- SigC++ 2.0 (Signal handling)
- VTE 2.91 (Terminal emulator widget)
- C++17 compiler support

### Build Dependencies

- g++ (C++ compiler)
- pkg-config
- make

## Building the Application

1. Open a terminal in the project directory
2. Run the following command:
   ```bash
   make
   ```
3. The executable will be created as `ngTerm` in the current directory

## Running the Application

After building, simply run:
```bash
./ngTerm
```

## Project Structure

- `main.cpp` - Main application entry point
- `main.h` - Main header file
- `Connections.cpp` - Connection management
- `Connections.h` - Connection management header
- `Folders.cpp` - Folder management
- `Folders.h` - Folder management header
- `Ssh.cpp` - SSH connection handling
- `Ssh.h` - SSH connection header
- `Config.cpp` - Configuration management
- `Config.h` - Configuration header
- `TreeModelColumns.h` - Tree model column definitions
- `icondata.h` - Embedded icon data
- `images/` - Application icons and resources

## Acknowledgements

This application was developed with significant assistance from Cascade, an AI coding assistant. Approximately 90% of the code and functionality was generated by Cascade, including:

- Core GTKmm UI implementation
- Folder and connection management
- SSH connection handling
- Configuration management
- Project structure and organization
- Build system configuration
- Code refactoring and optimization

The author would like to acknowledge the invaluable contribution of Cascade in bringing this project to life.

## License

This project is licensed under the terms of the license specified in the LICENSE file.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
