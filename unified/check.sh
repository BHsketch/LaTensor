# Check the signature
grep -r "tvm_ffi_main" ~/cosmos/cse/projects/tvm/src/ --include="*.cc" --include="*.h" -l 2>/dev/null | head -5
grep -r "__tvm_ffi_main" ~/cosmos/cse/projects/tvm/include/ --include="*.h" -l 2>/dev/null | head -5
grep -r "__tvm_ffi_main" ~/cosmos/cse/projects/tvm/3rdparty/tvm-ffi/include/ --include="*.h" -l 2>/dev/null | head -5
