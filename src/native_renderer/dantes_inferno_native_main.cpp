// dantes_inferno_native - Entry point for native renderer build
//
// This is a COPY of src/main.cpp that uses DantesInfernoNativeApp
// instead of DantesInfernoApp. The original src/main.cpp is NOT modified.
//
// Part of the DiligentCore migration (IMPL-DC-003).

#include "generated/default/dantes_inferno_init.h"

#include "dantes_inferno_native_app.h"

REX_DEFINE_APP(dantes_inferno_native, DantesInfernoNativeApp::Create)
