# llvm-string-obfuscation-pass
LLVM pass for string literal obfuscation in Android NDK shared libraries.

Design Choice: Runtime Decoder vs Generated IR Decoder

Initial decoder attempted in-place decryption.
This failed because encrypted globals were emitted as
constant data in read-only memory.

The decoder was updated to operate on a writable copy.