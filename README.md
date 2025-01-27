# Jade: Experimental JavaScript Runtime Foundation

**Jade** is an educational JavaScript runtime implementation demonstrating core concepts used in production runtimes like Node.js and Bun. This project serves as a foundation for understanding low-level runtime construction using:

- **JavaScriptCore (JSC)** from WebKit for JS execution
- **libuv** for cross-platform asynchronous I/O
- **C** for native bindings and system integration

**Current State**: Basic prototype supporting core runtime features

## 📦 Installation

### Prerequisites

#### Linux (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install \
  libwebkit2gtk-4.0-dev \
  libuv1-dev \
  cmake \
  build-essential
```

#### macOS
```bash
brew install cmake libuv
xcode-select --install # For Xcode command line tools
```

### Build from Source
```bash
# Clone repository
git clone https://github.com/dexter-xD/jade.git
cd jade

# Configure build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Compile
make

# Verify build
./jade --version
```

## 🏗️ Architecture Overview

### Core Components
```
┌───────────────────────┐
│      JavaScript       │
│       (User Code)     │
└───────────┬───────────┘
            │
┌───────────▼───────────┐   ┌───────────────────┐
│  JavaScriptCore (JSC) │◄─▶│      System       │
│    JS Execution       │   │      APIs         │
└───────────┬───────────┘   └───────────────────┘
            │                     ▲
┌───────────▼───────────┐         │
│      libuv Event      │◄────────┘
│        Loop           │
└───────────────────────┘
```

### Component Details

1. **JSC Engine Layer**
   - Creates/manages JS contexts
   - Handles JS code parsing/execution
   - Manages JS/C value conversions

2. **libuv Event Loop**
   - Timer management (`setTimeout`)
   - Filesystem operations (planned)
   - Network I/O (planned)
   - Thread pool integration

3. **System API Bridge**
   - Console I/O implementation
   - Future: File system access
   - Future: Network interfaces

## 🛠️ Current Features

### Implemented
- Core event loop infrastructure
- Basic JS execution context
- `console.log`/`console.error` bindings
- `setTimeout` implementation
- Memory-safe value passing between JS/C
- Build system with CMake

### In Progress
- Proper error propagation JS ↔ C
- File system API stubs
- Module resolution prototype

## 🚀 Usage

### Basic Execution
```bash
./build/jade path/to/script.js
```

### Example Script
```javascript
// Basic functionality demo
console.log("Jade Runtime Initialized");

setTimeout(() => {
  console.error("Async operation completed");
}, 1000);

console.log("Main thread execution");
```

**Expected Output**:
```
LOG: Jade Runtime Initialized
LOG: Main thread execution
ERROR: Async operation completed
```

### Current Limitations
- No Promise support
- Limited error handling
- Single-file execution only
- Basic memory management

## 🔬 Development Setup

### Test Suite
```bash
# Run all tests
cd build && ctest --output-on-failure

# Specific test target
./test/runtime_tests
```

### Test Coverage
```bash
# Generate coverage report
mkdir -p coverage
gcovr --exclude tests/ --html-details coverage/report.html
```

### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1
make clean && make
```

## 📝 Project Roadmap

### Phase 1: Core Foundation (Current)
- [x] Basic JS execution context
- [x] Event loop scaffolding
- [x] Console API implementation
- [ ] File system API stubs

### Phase 2: Production Patterns
- [ ] Error handling system
- [ ] Memory management audits
- [ ] Cross-platform testing
- [ ] Benchmarking suite

### Phase 3: Advanced Features
- [ ] Promise integration
- [ ] HTTP server prototype
- [ ] WASM support exploration
- [ ] Debugger protocol

## 🤝 Contribution Guide

### Ideal Contributions
- Core runtime improvements
- Additional system APIs
- Test coverage expansion
- Documentation improvements
- Cross-platform fixes

### Workflow
1. Create issue for discussion
2. Fork repository
3. Use feature branch workflow:
   ```bash
   git checkout -b feat/features
   ```
4. Follow coding standards:
   - C11 standard
   - 4-space indentation
   - Doxygen-style comments
5. Submit PR with:
   - Implementation details
   - Test cases
   - Documentation updates

## ⚠️ Known Issues

| Issue | Workaround | Priority |
|-------|------------|----------|
| Memory leaks in timer callbacks | Manual cleanup in tests | High |
| No Windows support | Use WSL/Linux VM | Medium |
| Limited error messages | Check debug build output | Low |

## 📚 Learning Resources

### Core Technologies
- [JavaScriptCore Internals](https://webkit.org/docs/javascript/)
- [libuv Design Overview](http://docs.libuv.org/en/v1.x/design.html)
- [Node.js Architecture Guide](https://nodejs.org/en/docs/guides/)

### Related Projects
- [Bun Runtime](https://bun.sh/docs)
- [Deno Core](https://deno.land/manual/runtime)
- [Node.js Bindings](https://nodejs.org/api/addons.html)


---

**Jade** is maintained by [dexter](https://github.com/dexter-xD) as an educational resource for understanding low-level runtime development. Not affiliated with Node.js, Bun, or WebKit projects.
