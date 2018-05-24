#include "third_party/blink/renderer/modules/credentialmanager/credentials_container.h"

#include "gin/converter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {
  
  ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions& options) {
      v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
      return ScriptPromise::Reject(script_state, v8::Exception::TypeError(gin::StringToV8(isolate,
                "This api has been disabled.")));
  }

  ScriptPromise CredentialsContainer::store(ScriptState* script_state,
                                          Credential* credential) {
    v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
    return ScriptPromise::Reject(script_state, v8::Exception::TypeError(gin::StringToV8(isolate,
                  "This api has been disabled.")));
  }
} // namespace blink
