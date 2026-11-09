# Compatibility

The SDK requires the ScreenAI 153.5 package at the fixed path `runtime/screenai-runtime-153.5.sai` beside `screenai.dll`. The package filename and version are locked by the build and loader; the SDK does not scan for other package versions.

The adapter's exported function declarations and bitmap layout were recovered against Chrome ScreenAI 153.4. The fixed 153.5 package has initialized and recognized the sample image through the packaged runtime path. This end-to-end check covers the exercised OCR path; it is not a complete audit of the private ABI.

The 153.5 package is kept as a separate dependency so its assets can be preserved without relying on a Chrome installation. Supporting another release requires changing the fixed version and filename together and reviewing the ABI and result format.
