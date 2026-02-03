# Project Rules

## C++ Code Guidelines

### Coding Standards
- **C++ Standard**: Use C++20 features and syntax
- **Code Style**: Follow Google C++ Style Guide
- **Single Responsibility Principle**: Each class/function should have one clear responsibility
- **Ambiguity Elimination**: Write clear, unambiguous code with explicit types and naming

### Code Quality Requirements
- **Comments**: All code must have comprehensive comments including:
  - File-level documentation describing purpose and usage
  - Function-level documentation with parameters, return values, and behavior
  - Complex logic explanations
  - Inline comments for non-obvious operations
- **Naming**: Use descriptive, self-documenting names
- **Type Safety**: Prefer explicit types over auto when clarity is needed

### Dependencies and Frameworks
- **Testing Framework**: Google Test (gtest) for unit and integration testing
- **Texture Compression**: basisu for texture compression
- **Geometry Compression**: draco for 3D geometry compression
- **Triangulation**: earcut-hpp for polygon triangulation
- **Linear Algebra**: eigen3 for matrix operations and linear algebra
- **Formatting**: fmt for modern string formatting
- **Geospatial Data**: gdal with features (geos, gif, iconv, jpeg, libkml, libspatialite, openjpeg, png, recommended-features, sqlite3, webp, zstd)
- **Math Library**: glm for 3D mathematics
- **Mesh Optimization**: meshoptimizer for mesh optimization algorithms
- **JSON Processing**: nlohmann-json for JSON parsing and generation
- **3D Graphics**: osg (OpenSceneGraph) for 3D graphics rendering
- **Logging**: spdlog for fast and efficient logging
- **Database**: sqlite3 for embedded database operations
- **Image Processing**: stb for image loading and processing
- **glTF Loading**: tinygltf for glTF 2.0 model loading
- **Error Handling**: C++20 std::expected, std::variant, std::optional for error handling and type-safe alternatives to exceptions

### Code Quality Tools
- **clang-format**: Format all C++ code according to Google style
- **clang-tidy**: Run static analysis and fix all warnings
- **clippy**: Use clippy for additional linting (if applicable)
- **check**: Ensure all compilation checks pass

### Business Logic Responsibilities
- **Data Parsing**: Implement parsing logic for 3D Tiles data formats
- **Coordinate Parsing**: Handle coordinate system transformations and parsing
- **Complex Computation**: Implement computationally intensive algorithms
- **Exception Handling**: Implement robust error handling and exception management

## Rust Code Guidelines

### Coding Standards
- **Single Responsibility Principle**: Each module/function should have one clear responsibility
- **Ambiguity Elimination**: Write clear, unambiguous code with explicit types
- **Error Handling**: Use Result<T, E> for proper error propagation

### Code Quality Requirements
- **Comments**: All code must have comprehensive comments including:
  - Module-level documentation describing purpose and usage
  - Function-level documentation with parameters, return values, and errors
  - Complex logic explanations
  - Inline comments for non-obvious operations
- **Naming**: Use Rust naming conventions (snake_case for functions/variables, PascalCase for types)
- **Documentation**: Use `///` for public API documentation

### Dependencies and Frameworks
- **Testing Framework**: Rust built-in testing framework (cargo test)
- **FFI**: libc for C-compatible types and functions
- **CLI Parsing**: clap for command line argument parsing
- **Time Handling**: chrono for date and time operations
- **Parallelism**: rayon for data parallelism
- **Serialization**: serde with derive feature for serialization framework
- **JSON**: serde_json for JSON serialization/deserialization
- **XML**: serde-xml-rs for XML serialization/deserialization
- **Logging**: log for logging facade, env_logger for logger implementation
- **Byte Order**: byteorder for reading/writing byte order
- **Build Dependencies**: cmake for CMake integration, pkg-config for pkg-config integration
- **Error Handling**: Result<T, E> for error propagation, thiserror for deriving error types, anyhow for easy error context and handling

### Code Quality Tools
- **clippy**: Run clippy linter and fix all warnings
- **format**: Format all Rust code using rustfmt

### Business Logic Responsibilities
- **Command Line Parsing**: Implement CLI argument parsing using clap or similar
- **Orchestration**: Coordinate workflow and process management
- **Error Handling**: Implement comprehensive error handling and reporting

## FFI (Foreign Function Interface) Guidelines

### Data Transfer Principles
- **Avoid Complex Data Structure Copying**: Minimize data copying between C++ and Rust
- **Zero-Copy Where Possible**: Use pointers, references, or shared memory when feasible
- **Ownership Transfer**: Clearly define ownership semantics across FFI boundaries
- **Memory Safety**: Ensure proper memory management to prevent leaks and use-after-free

### Protocol Design
- **Simple Data Types**: Prefer primitive types and simple structs for FFI interfaces
- **Serialization**: Use efficient serialization (e.g., JSON, MessagePack) for complex data
- **Buffer Management**: Use explicit buffer allocation and deallocation protocols
- **Error Codes**: Return error codes rather than exceptions across FFI boundaries

### Implementation Guidelines
- **C-ABI Compatibility**: Ensure FFI functions use C-compatible calling conventions
- **Opaque Pointers**: Use opaque pointers for complex Rust types exposed to C++
- **Lifetime Management**: Clearly document lifetime requirements for borrowed data
- **Thread Safety**: Ensure thread-safe operations across language boundaries

## Build and Verification Commands

### C++ Commands
```bash
# Format code
clang-format -i --style=Google src/**/*.cpp src/**/*.h

# Run static analysis
clang-tidy src/**/*.cpp -- -std=c++20

# Build and check
cmake --build build && ctest
```

### Rust Commands
```bash
# Format code
cargo fmt

# Run linter
cargo clippy -- -D warnings

# Build and test
cargo build && cargo test
```

## General Project Guidelines

### Code Review Checklist
- [ ] Single responsibility principle followed
- [ ] No ambiguous code or naming
- [ ] Comprehensive comments present
- [ ] All linting tools pass without warnings
- [ ] FFI interfaces avoid unnecessary data copying
- [ ] Proper error handling implemented
- [ ] Code follows language-specific style guidelines

### Testing Requirements
- **C++ Testing**: Use Google Test (gtest) framework for all C++ unit and integration tests
- **Rust Testing**: Use Rust built-in testing framework (cargo test) for all Rust tests
- Unit tests for all business logic
- Integration tests for FFI boundaries
- Performance benchmarks for complex computations
- Error handling test coverage

### Documentation Requirements
- API documentation for all public interfaces
- Architecture documentation for major components
- FFI protocol documentation
- Build and setup instructions
