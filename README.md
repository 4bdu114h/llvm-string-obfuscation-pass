# LLVM String Obfuscation Pass

An out-of-tree LLVM pass that obfuscates string literals at compile time, applied to an Android NDK `.so`. Strings are not visible to `strings` or in a disassembler, but the binary runs correctly.

---

## Repositories

| Repo | Purpose |
|------|---------|
| `llvm-string-obfuscation-pass` | The LLVM pass, runtime decoder, test files, and verification artifacts |
| [4bdu114h/ndk-samples](https://github.com/4bdu114h/ndk-samples/tree/llvm-string-obfuscation) (branch: `llvm-string-obfuscation`) | Forked from [android/ndk-samples](https://github.com/android/ndk-samples). Contains the Android project with the pass integrated into the build. |

---

## How to Build

### Prerequisites

- LLVM 18 (`/opt/homebrew/opt/llvm@18` on macOS)
- Android NDK r28+
- Android SDK with build tools
- Java 21+
- CMake 3.22+

### Build the Pass

```bash
cd pass
mkdir build && cd build
cmake .. -DLLVM_DIR=/opt/homebrew/opt/llvm@18/lib/cmake/llvm
make
```

This produces `pass/build/StringObfuscationPass.so`.

### Build the Android Project

```bash
cd /path/to/ndk-samples
./gradlew :hello-jni:app:assembleDebug
```

The pass runs automatically during the build. You will see:

```
=== String Scan Started ===
Replaced string: .str
Replaced string: .str.1
Skipping aggregate-only string: .str.2
Skipping aggregate-only string: .str.3
```

The output APK is at:
```
hello-jni/app/build/outputs/apk/debug/app-debug.apk
```

### Install and Run

```bash
adb install hello-jni/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.example.hellojni/.HelloJni
```

---

## How the Pass Works

### Build Pipeline

```
hello-jni.cpp
      ↓
clang -S -emit-llvm   →   hello-jni.ll
      ↓
opt -load-pass-plugin StringObfuscationPass.so -passes=string-obfuscation
      ↓
hello-jni-obfuscated.ll
      ↓
clang -c              →   hello-jni-obfuscated.o
      ↓
linked with decode.cpp.o  →  libhello-jni.so
```

### Encoding Algorithm

The pass uses a rotating XOR with a 3-byte key `{0x12, 0x34, 0x56}`:

```
encoded[i] = original[i] ^ key[i % 3]
```

This satisfies the rotating XOR requirement and produces a different effective key byte for each position modulo 3, making single-byte XOR frequency analysis ineffective.

### What the Pass Does

1. Iterates over all globals in the LLVM module
2. Finds `ConstantDataArray` globals that are strings (`isString()` returns true)
3. Checks whether the global has at least one `Instruction`-level user (i.e. is directly used in code, not just embedded in an aggregate)
4. For qualifying globals: XOR-encodes the bytes in place and rewrites the global initializer
5. For each instruction-level use: inserts a `call @decode(ptr @.str)` immediately before the use and replaces the original operand with the decoded pointer

### Decoder Strategy

The decoder (`runtime/decode.cpp`, also at `hello-jni/app/src/main/cpp/decode.cpp`) is a simple lazy decoder: strings stay encrypted in the binary and are decrypted on first use at the call site. The decoder applies the same rotating XOR in reverse (XOR is its own inverse) and writes the result into a static 1 KB buffer.

The pass declares `@decode` as an external function and inserts calls to it. The Android build links `decode.cpp` into the same shared library.

---

## Android Integration

The pass is integrated without modifying `hello-jni.cpp`. The `CMakeLists.txt` in `hello-jni/app/src/main/cpp/` defines a custom build command that:

1. Compiles `hello-jni.cpp` to LLVM IR (`-S -emit-llvm`)
2. Runs `opt` with the pass plugin to produce obfuscated IR
3. Compiles the obfuscated IR to an object file
4. Links that object file together with `decode.cpp` into `libhello-jni.so`

The build is restricted to `arm64-v8a` only, which required changing `abiFilters` in `AndroidApplicationConventionPlugin.kt` and `hello-jni/app/build.gradle`.

---

## Verification

### strings Before and After

`verification/strings_before.txt` — strings from a baseline (unobfuscated) build:
```
Hello World
This string should be obfuscated
```

`verification/strings_after.txt` — strings from the obfuscated `.so`:
```
ZQ:~[vE[$~P\
F\?a
```

Neither `Hello from JNI` nor `Hello World` nor `com/example/hellojni` appear in the obfuscated binary.

### Ghidra Screenshot

`verification/ghidra_obfuscated_string.png` shows the `StringFromJni` function in Ghidra's disassembly. The key observations:

- `adr x0, DAT_00115b10` loads a pointer to the obfuscated data, not a readable string
- `bl decode` calls the runtime decoder immediately after, with Ghidra's comment showing `char * decode(char * str)`
- No plaintext "Hello from JNI" is visible anywhere in the function

### Runtime Screenshot

`verification/runtime_verification.png` shows the app running on a Pixel 9 emulator (arm64-v8a, Android API 36) and correctly displaying "Hello from JNI." — confirming that `decode()` correctly reverses the XOR at runtime.

---

## What the Pass Covers and What It Misses

### Covered

- String globals that are used directly in instructions (e.g. passed as arguments to `NewStringUTF`, `FindClass`, etc.)
- Works correctly on LLVM IR generated for `aarch64-none-linux-android21`
- Rotating XOR with per-position key bytes

### Not Covered

**Aggregate-embedded strings.** Some strings are only referenced inside `ConstantStruct` or `ConstantArray` aggregate initializers, with no direct instruction-level use. The JNI method registration table is the key example:

```llvm
@_ZZ10JNI_OnLoadE7methods = internal constant [1 x %struct.JNINativeMethod] [
  %struct.JNINativeMethod {
    ptr @.str.2,   ; "stringFromJNI"
    ptr @.str.3,   ; "()Ljava/lang/String;"
    ptr @_Z13StringFromJniP7_JNIEnvP8_jobject
  }
]
```

`stringFromJNI` and `()Ljava/lang/String;` are still visible in the binary. Handling these would require rebuilding the aggregate initializer with runtime-decoded pointers, which is significantly more complex and risks breaking JNI registration.

**Other gaps:**
- The `decode()` function itself is a visible symbol in the binary — an attacker can find it immediately
- The static buffer in `decode()` is not thread-safe
- Debug info (`-g`) may preserve original string content in DWARF sections
- The XOR loop in `decode()` is trivially recognizable in disassembly — see bonus section below

---

## Bonus: Decoder Visibility

The current `decode()` implementation is a straightforward XOR loop. In Ghidra, a reviewer will spot it in seconds and recover every string with a one-pass script.

No additional hardening was implemented in this submission, but the realistic mitigations would be:

- **Force-inline the decoder at each call site** using `__attribute__((always_inline))` — eliminates the single recoverable `decode` function and spreads the XOR logic across many call sites
- **Split the key across multiple helpers** — instead of one loop with `key[i % 3]`, have three separate functions each handling one key byte
- **Opaque predicates** — wrap the XOR in conditionals that always evaluate the same way but are hard to simplify statically
- **Per-string keys** — use a different key derived from the global's address or a hash, so recovering one string's key does not recover all strings

What an attacker can still do regardless: run the binary under a debugger, set a breakpoint after `decode()` returns, and read the plaintext from the return value. Dynamic analysis trivially bypasses all static obfuscation. The goal is to raise the cost of static analysis, not to make recovery impossible.
