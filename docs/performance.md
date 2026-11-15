# Performance tuning

The SDK serves the recognizer config `gocr_mobile_und_config.pb` to
`chrome_screen_ai.dll` through the runtime-package file callbacks.  Before the
DLL parses it, `src/backend/chrome_screen_ai/model_files.cpp` rewrites two
scheduling fields.  The model and the OCR output are unchanged.

The values are compile-time constants in
`src/backend/chrome_screen_ai/runtime_config.h`:

```cpp
#define SCREENAI_INTERPRETER_THREADS 6
#define SCREENAI_CHUNKS_PER_BATCH 64
```

## `SCREENAI_INTERPRETER_THREADS`

Threads used by the TFLite/XNNPACK interpreter inside each process.  This is the
`TfliteModelPooledRunnerConfig.interpreter_num_threads` field (field 3).

- The native default is one thread.
- Higher values reduce single-image latency by using more cores.
- Higher values also increase total CPU per image, so they reduce aggregate
  throughput once the CPU is saturated.
- On an 8-core/16-thread CPU, 4-8 is the useful range.  The test machine
  (Ryzen 7 2700X) is best at 6.

## `SCREENAI_CHUNKS_PER_BATCH`

Number of 32x168 line chunks packed into one recognizer invocation.  This is
the `GocrLineRecognizerConfig.num_chunk_per_batch` field (field 47).

- The native default is 8.
- Higher values amortize the per-invocation overhead.
- Too high increases peak memory and can reduce throughput.  On the 21-page
  test corpus, 32-64 was best and 96+ was slower.

## Measured results

Single-image latency, one warm recognizer, 153.7 runtime:

| page | baseline (1 thread, batch 8) | tuned (6 threads, batch 64) | speedup |
|---|---:|---:|---:|
| 1 | 2.286 s | 1.245 s | 1.84x |
| 5 | 0.854 s | 0.561 s | 1.52x |
| 17 | 0.630 s | 0.550 s | 1.15x |
| 10 | 3.340 s | 1.848 s | 1.81x |

Batch throughput with `RecognizeBatch` (25 requests over the 21-page corpus,
warm workers):

| workers | baseline img/s | tuned img/s | baseline CPU s/img | tuned CPU s/img | baseline peak | tuned peak |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.35 | 0.67 | 3.50 | 3.43 | 0.52 GiB | 0.53 GiB |
| 2 | 0.68 | 1.08 | 3.18 | 4.23 | 0.89 GiB | 0.95 GiB |
| 4 | 1.20 | 1.49 | 3.47 | 5.74 | 1.58 GiB | 1.71 GiB |
| 8 | 1.80 | 1.95 | 4.23 | 6.50 | 3.26 GiB | 3.31 GiB |
| 15 | 2.09 | 1.84 | 5.58 | 6.76 | 5.68 GiB | 5.82 GiB |

The tuned build is faster at 1-8 workers and slower at 15 because six threads
per process oversubscribe the 16 hardware threads.  The best operating point
depends on the workload:

- Minimum single-image latency: 2-4 workers, 6 threads, batch 64.
- Maximum throughput: 15 workers, 1 thread, batch 8 (the baseline settings).
- Balanced throughput/memory: 8 workers, 6 threads, batch 64.

The 21-page corpus output is byte-identical to the baseline.  The patch only
changes scheduling; it does not change the model or the decoder.

## Changing the values

Edit the two defines in `runtime_config.h` and rebuild:

    cmake --build build --config Release

The patch is applied in every process, including the persistent
`screenai_worker.exe` workers, because they load the same `screenai.dll` and
the same runtime package.  No package repack is needed.

If the config layout changes in a future runtime, the patcher fails during
initialization and the SDK reports the callback error instead of silently
serving an unpatched config.
