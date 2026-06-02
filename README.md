# LLVM String Obfuscation Pass

A custom LLVM pass that identifies string literals in LLVM IR, encrypts them using XOR-based obfuscation, rewrites call sites to insert runtime decoding, and preserves original program behavior.

This project was implemented as part of an LLVM/Android NDK string obfuscation assignment.

---

## Overview

String literals are commonly extracted from native binaries using tools such as `strings`, making sensitive information easy to discover.

This project demonstrates a compiler-based obfuscation approach:

1. Detect string literals in LLVM IR.
2. Encrypt string contents at compile time.
3. Replace original global string data with encrypted data.
4. Insert runtime decode calls before string usage.
5. Rewrite call sites to use decoded strings.
6. Preserve original application behavior.

---

## Architecture

```text
hello.cpp
    ↓ clang -emit-llvm
hello.ll
    ↓
StringObfuscationPass
    ↓
hello_obfuscated.ll
    ↓
runtime/decode.cpp
    ↓
Executable
```

---

## Project Structure

```text
llvm-string-obfuscation-pass/
│
├── pass/
│   ├── StringObfuscationPass.cpp
│   └── CMakeLists.txt
│
├── runtime/
│   └── decode.cpp
│
├── test/
│   ├── hello.cpp
│   ├── hello.ll
│   └── hello_obfuscated.ll
│
├── verification/
│   ├── strings_before.txt
│   └── strings_after.txt
│
└── README.md
```

---

## LLVM Pass Workflow

### 1. String Discovery

The pass iterates through module globals and identifies string literals stored as `ConstantDataArray`.

### 2. Compile-Time Encryption

Each string is XOR-encrypted using a compile-time key.

Example:

```text
Original:
Hello World

Encrypted:
ZQ:~[vE[$~P\
```

### 3. Global Replacement

The original string initializer is replaced with encrypted bytes.

### 4. Decode Call Injection

The pass inserts a runtime decode call before string usage.

Before:

```llvm
printf(@.str)
```

After:

```llvm
%decoded_str = call ptr @decode(ptr @.str)

printf(%decoded_str)
```

### 5. Call-Site Rewriting

All matching string arguments are rewritten to use the decoded value.

---

## Runtime Decoder

The runtime decoder is implemented in:

```text
runtime/decode.cpp
```

The decoder reverses the XOR transformation and returns a writable buffer containing the plaintext string.

### Design Decision

An initial implementation attempted in-place decryption.

This caused runtime failures because encrypted globals are emitted into read-only memory sections.

The decoder was updated to operate on a writable copy of the string before decryption.

---

## Verification

### Functional Verification

Original executable:

```text
Hello World
This string should be obfuscated
```

Obfuscated executable:

```text
Hello World
This string should be obfuscated
```

Program behavior remains unchanged.

### Binary Verification

Using the `strings` utility:

Before obfuscation:

```text
Hello World
```

After obfuscation:

```text
No match found
```

This demonstrates that plaintext strings are no longer directly present in the final binary.

---

## Limitations

Current implementation uses a fixed XOR key for all strings.

A future enhancement would be per-string random keys embedded alongside encrypted string data. This requires replacing LLVM globals with larger storage layouts and redirecting all uses to the replacement globals.

---

## Android Integration Approach

The intended Android integration model is:

```text
Android Native Source
        ↓
LLVM Pass
        ↓
Obfuscated LLVM IR
        ↓
NDK Build
        ↓
Shared Library (.so)
```

The runtime decoder can be linked as a small support library and used by native Android components built through the Android NDK.

---

## Build Notes

Environment:

* macOS (Apple Silicon)
* LLVM 18.1.8
* Clang 18
* CMake
* Homebrew LLVM Toolchain

```
```
