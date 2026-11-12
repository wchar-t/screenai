# Compatibility

The SDK requires the ScreenAI 153.7 package at the fixed path `runtime/screenai-runtime-153.7.sai` beside `screenai.dll`. The package filename and version are locked by the build and loader; the SDK does not scan for other package versions.

The adapter's exported function declarations and bitmap layout were recovered against Chrome ScreenAI 153.4. The fixed 153.7 package initialized and recognized the sample image through the packaged runtime path. The smoke check also exercised the CLI JSON output and the Python single-image and two-worker batch APIs, including a failed batch item.

These checks cover the exercised OCR path; they are not a complete audit of the private ABI or every model and image type.

The 153.7 package is kept as a separate dependency so its assets can be preserved without relying on a Chrome installation. Supporting another release requires changing the fixed version and filename together and reviewing the ABI and result format.
