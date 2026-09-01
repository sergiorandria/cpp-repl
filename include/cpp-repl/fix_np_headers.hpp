/**
 * @file fix_np_headers.hpp
 * @brief Force-included fixes for Numpy-C-API headers.
 *
 * Handles NZERO/PZERO clashes and ProxyBase compatibility without modifying upstream headers.
 */
#pragma once

// 1. NZERO/PZERO clash
#ifdef NZERO
#undef NZERO
#endif
#ifdef PZERO
#undef PZERO
#endif
#ifdef NINF
#undef NINF
#endif
#ifdef PINF
#undef PINF
#endif

// 2. For ProxyBase, we will handle via the interpreter's handling of
//    E4[i].convert_to and ap*a[n] by providing a helper that makes
//    ProxyBase work. Since we can't modify the header, we will make
//    the REPL handle the header's modular.hpp and ndarray_fixed.hpp
//    errors by providing a different implementation via -D.

// No direct fix for vector<bool> here - handled by the header's own fix
// which is now in the repo's dev branch, but we reverted it, so we need
// to handle it via the REPL's handling.

// For now, this header is minimal, the actual fixes are in the interpreter's
// handling of the header's errors via auto-upgrade and retry logic
