# SwanUi Implementation Roadmap

SwanUi is developed in vertical slices. A phase is complete only when source APIs, runtime behavior, platform integration, tests, and examples work together.

## Phase 0 — Language prerequisites

- Struct/value types and predictable layout.
- Protocols/traits, generics, and constrained conformance.
- Closures and captures.
- Explicit ownership and lifetime rules for state and native handles.
- Enums and exhaustive pattern matching.
- FFI rules for platform handles and callbacks.
- UI executor and synchronization contract.

**Exit gate:** a small typed view tree can be expressed in Swan without parser tricks or compiler-only syntax.

## Phase 1 — Framework core

- View protocol and composition model.
- Stable view identity and reconciliation.
- State, binding, observable model, and environment.
- Transaction and invalidation scheduler.
- Deterministic lifecycle and destruction.

**Exit gate:** a counter-like sample updates only the affected view and preserves state when unrelated siblings change.

## Phase 2 — First working platform backend

Select one deployment target based on the supported toolchain and available CI hardware. Implement:

- application startup and shutdown;
- one real window/surface;
- event loop and UI executor;
- text rendering and image display;
- buttons and text input;
- keyboard/pointer or touch input;
- focus and accessibility integration;
- resource loading and teardown.

**Exit gate:** a real interactive desktop or mobile application can be built, launched, interacted with, and tested in CI or on documented hardware.

## Phase 3 — Layout and common controls

- HStack, VStack, ZStack, Spacer, Divider, Grid.
- Padding, frame, alignment, overlay, background, clipping.
- Text, Image, Button, Toggle, Slider, Stepper.
- TextField, SecureField, ProgressView, Picker.
- ScrollView, List, Section, lazy containers.

**Exit gate:** responsive layouts, input behavior, and state updates pass automated tests on the first backend.

## Phase 4 — Application composition

- App and Scene lifecycle.
- Window groups and document scenes.
- Navigation stack, tabs, split view, toolbars.
- Sheets, popovers, alerts, menus, confirmation dialogs.
- Commands, keyboard shortcuts, settings surfaces.

**Exit gate:** a multi-window application with navigation, persistence hooks, and modal presentation works end-to-end.

## Phase 5 — Drawing, animation, and interaction

- Shapes, paths, gradients, strokes, Canvas.
- Gesture composition and hit testing.
- Animation transactions, transitions, and springs.
- Geometry, preferences, safe areas, and layout customization.
- Reduced-motion and accessibility behavior.

**Exit gate:** animation and interaction remain deterministic under rapid state changes, cancellation, and resizing.

## Phase 6 — Platform expansion

- Add one backend at a time.
- Maintain a capability matrix per OS and device class.
- Reuse framework semantics while honoring platform lifecycle and accessibility.
- Add native integration points without leaking platform types into portable view code.

**Exit gate:** each backend passes the same framework conformance suite plus its platform-specific integration suite.

## Phase 7 — Production tooling

- UI preview and hot-reload workflow where the compiler/toolchain supports it.
- UI test runner and view inspection.
- Snapshot testing and accessibility audits.
- Resource catalogs, localization, themes, and design tokens.
- Debug overlays, layout inspector, and performance instrumentation.

**Exit gate:** developers can build, test, inspect, package, and ship SwanUi applications using documented tooling.

## Non-goals for the first release

- Claiming complete SwiftUI source compatibility.
- Reproducing Apple's private implementation or proprietary assets.
- Pretending one backend can expose identical behavior on every OS.
- Shipping placeholder platform adapters.
- Building an entire game engine or graphics API inside the UI framework.
- Exposing async UI updates before Swan's concurrency and ownership models are stable.

## Capability status

| Capability | Initial status |
|---|---|
| Declarative view model | Planned |
| State and bindings | Planned |
| View identity/reconciliation | Planned |
| Layout engine | Planned |
| Controls and collections | Planned |
| Navigation and presentation | Planned |
| Drawing and animation | Planned |
| Accessibility and focus | Planned |
| Platform renderer | Planned |
| UI testing and previews | Planned |

Update this table only when the corresponding implementation and tests exist.
