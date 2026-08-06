# Precision panel: relative X/Y offset for multi-selection

**Date:** 2026-08-06  
**Scope:** `tools/zone-editor` — `PrecisionPanel` only (plus existing `nudgeSelection`)  
**Status:** approved design, pending implementation

## Problem

With two or more zones selected, the Precision panel disables X and Y. Absolute edits would stack every selected box on the same point. Users still want numeric control to slide the whole selection along one axis without overlapping.

Keyboard arrow nudge already moves the selection relatively via `nudgeSelection`. The gap is the Precision panel number fields.

## Decision

In multi-selection, X and Y become **relative offset (Δ)** fields, not absolute coordinates.

Chosen UX (option C + approach 1):

- Fields do not show per-LED or mixed absolute positions.
- They show `0` as the current offset draft.
- Committing a non-zero value nudges every selected zone by that delta on that axis only, then resets the field to `0`.

## Behavior

### Single selection (unchanged)

- X / Y / width / height edit absolute geometry via `setLedGeometry`.
- Commit on Enter or blur; round to integer; ignore non-finite values.

### Multi-selection (2+ zones)

| Field | Mode | Commit effect |
| --- | --- | --- |
| X | Δ offset | `nudgeSelection({ x: Δ, y: 0 })`, then field → `0` |
| Y | Δ offset | `nudgeSelection({ x: 0, y: Δ })`, then field → `0` |
| width | absolute batch | every selected LED gets that width (unchanged) |
| height | absolute batch | every selected LED gets that height (unchanged) |

Additional rules for X/Y offsets:

- Empty, non-finite, or `0` → no-op (no undo step).
- Values are rounded with `Math.round` before nudge.
- **Commit triggers** (must not apply on every keystroke — typing `10` must nudge once by +10, not +1 then reset):
  - Enter
  - blur
  - native DOM `change` on the number input (Chromium fires this when the spinner is clicked, but not per typed digit; React’s `onChange` is the `input` event and must only update local draft state)
- After a successful commit, the field resets to `0` so it stays an offset entry, not a running counter.
- One successful nudge commit = one undo step (existing `nudgeSelection` / `moveLed` behavior).
- Align and Pack controls stay as they are.

### UI copy

- Keep the existing “Width/height apply to all selected.” line.
- Add a short line that X/Y move the selection by offset (e.g. “X/Y move selection by offset.”).
- Placeholder on X/Y in multi mode: `Δ` (not `—` / disabled).

## Non-goals

- No absolute X/Y batch edit.
- No min-bounding-box or “first selected” absolute reference field.
- No changes to group apply/edge-fill logic.
- No new store API if `nudgeSelection` already covers the mutation.

## Implementation sketch

Primary file: `tools/zone-editor/src/components/Panels/PrecisionPanel.tsx`

- Stop disabling X/Y when `isMulti`.
- For multi X/Y: local draft defaults to `"0"`; React `onChange` only updates draft; commit on Enter, blur, and native DOM `change` (spinner); call `nudgeSelection` with a single-axis delta, then reset that field to `"0"`.
- Wire `nudgeSelection` from the store alongside `setLedGeometry`.
- Update the panel comment/docstring to describe offset mode.

Tests: `tools/zone-editor/src/components/Panels/__tests__/PrecisionPanel.test.tsx`

- Multi: X/Y enabled; display `0`.
- Commit Δx = 10 → every selected LED’s X increases by 10, Y unchanged; input back to `0`.
- Commit Δy similarly.
- Commit `0` / empty → geometry unchanged.
- Single-selection absolute X/Y regression still passes.
- Existing width batch / Align / Pack tests unchanged in intent.

Optional README note under Precision panel: multi X/Y are offsets.

## Success criteria

- Selecting several zones and entering `10` in X (or using the stepper) shifts the whole selection +10px on X without collapsing positions.
- Undo reverses that nudge in one step.
- Single-zone numeric editing behaves exactly as before.
