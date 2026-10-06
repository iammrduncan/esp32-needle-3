# Constrained decoding and product routing

Needle 3 is not used here as a general chat model. It is a small, local
controller that emits a compact JSON tool call. The useful system is therefore
the combination of model, byte grammar, vocabulary filtering, deterministic
sampler, prefix state, and host-side validation. Optimizing only the transformer
misses both important runtime cost and important correctness state.

## Contract

The device receives a prompt, evaluates a primed schema prefix, and generates a
structured call array. The first, route-schema pass is stricter: it must contain
exactly one capability call with empty arguments and executes no local device
operation. The host maps that route through its catalog and either stops at an
external label or sends the **unchanged original prompt** to a second ESP32 pass
under the local-tool schema. That second pass may generate multiple calls. The
firmware validates the entire array before executing any of them and returns
their device-state results; there is no third inference over those results.

The demonstration does **not** call Claude, Qwen, or GPT-OSS. Those are route
labels. `remote_called=false` is a product invariant in the saved capture.

## Byte-level grammar

The grammar operates on emitted bytes rather than on abstract JSON tokens.
That matters because one vocabulary token can contain multiple bytes and may
cross several grammar states. At each decode step the runtime:

1. derives the set of bytes legal from the current grammar state;
2. identifies vocabulary tokens whose complete byte strings are valid;
3. computes logits only for that legal subset where the optimized path applies;
4. chooses deterministically; and
5. advances the grammar through every byte of the selected token.

The implementation lives in [`engine/src/nd_grammar.c`](../../../engine/src/nd_grammar.c),
[`engine/src/nd_tokenizer.c`](../../../engine/src/nd_tokenizer.c), and
[`engine/src/nd_sample.c`](../../../engine/src/nd_sample.c). The firmware-side
generation loop and cached schema states are in
[`esp32/main/main.c`](../../../esp32/main/main.c).

## Why subset logits matter

A dense output projection touches every vocabulary row even when the grammar
permits only a small set. With an 8,192-token vocabulary, filtering the legal
IDs first can avoid most output-matrix work. The benefit depends on the legal
set size, so the benchmark records constrained decode separately from the
unconstrained `think` path. A speedup that exists only in the primary grammar
workload is not automatically a transformer-kernel speedup.

There are three implementation requirements:

- the subset must be exactly equivalent to dense logits followed by masking;
- row IDs must preserve the same tie-breaking order as the reference sampler;
- scratch and cached candidate lists must be request-local or deliberately
  invalidated when grammar state changes.

## Determinism and ties

The quality contract is byte-for-byte output, not merely valid JSON. Tiny
floating-point differences can change an argmax when two legal tokens are
close. Stable comparison order and explicit tie policy are therefore part of
the public behavior. An optimized logit kernel can pass average-error probes
yet fail a tool-call golden through a single near-tie.

Use the layered gates in [numerical correctness](numerical-correctness.md):
kernel differential, logit delta/top-1, host goldens, device goldens, and an
end-to-end behavior capture. No one layer substitutes for the others.

## Prefix and schema caches

The firmware primes a fixed system prefix and saves reusable inference state.
It also maintains schema-dependent cached states used by the routing/tool
protocol. A model-prefix cache is valid only if all persistent model state that
affects the next token is captured: KV vectors/scales, Q/K/V convolution
histories, engram token/value history, optional confidence state, and
position/sink bookkeeping. Grammar state is initialized for the new generation,
and mHC lanes are per-token scratch rather than saved recurrent state.

The compact-prefix work reclaimed about 768 KiB and was quality-neutral under
the host prefix-isolation gate. It was valuable mostly because it made room for
another optimization (narrow engram staging), not because compression alone
made decode faster. This is a reusable pattern: state compaction can be an
enabler even when it is not a throughput lever.

## Capability descriptions are executable prompt design

The route schema originally exposed one `select_model` enum. In the originating
Codex session, all four test prompts collapsed to Qwen. Splitting it into six
concrete capabilities fixed most routes, but an architecture request still
chose a vague translate/write capability. Explicitly describing the intended
research/design/architecture capability fixed that case. A batched request
still selected only one local action, demonstrating a separate compositional
limit.

The lesson is not to overfit those exact words. Tool names and descriptions
form part of the model input and should describe observable capability, scope,
and stopping behavior. Treat a schema edit like a code edit: freeze regression
prompts and validate both the selected route and the arguments.

## What worked

- Byte-accurate grammar walking kept malformed continuations out before
  sampling rather than repairing JSON afterward.
- Legal-subset output projection removed work proportional to disallowed
  vocabulary rows while retaining a dense reference path.
- Deterministic selection made exact-output regression practical.
- Concrete capability-oriented route descriptions performed better than a
  bare model-choice enum.
- Two-pass local execution separated capability choice from local call
  generation while preserving the original prompt; external routes stopped at
  selection and the second-pass tools returned fresh state after execution.
- End-to-end capture checked routing, tools, telemetry, timer expiry, sampling
  changes, pass count, and the no-external-call invariant together.

## What failed or remained limited

- A single model enum encouraged label priors rather than task routing.
- Vague or bundled capabilities misrouted timer/status/telemetry prompts.
- A seven-case capture is a regression suite, not proof of general semantic
  routing quality.
- Exact output can change when transport/request state changes even with the
  engine binary unchanged; two frozen cases remain an owner oracle decision.
- Host grammar tests cannot prove UART framing, firmware cache lifetime, or the
  product's two-pass behavior.

## Porting checklist

1. Specify the output language and stop condition before tuning kernels.
2. Keep a slow dense-logit/reference grammar path.
3. Define byte handling for multi-byte tokens and invalid UTF-8 explicitly.
4. Make tie-breaking deterministic and test close logits.
5. Snapshot every recurrent/prefix state field or prove it is reconstructible.
6. Test single actions, external selections, malformed prompts, batches, and
   long requests.
7. Record route match, tool match, pass count, side effects, and external-call
   behavior—not only the final text.
