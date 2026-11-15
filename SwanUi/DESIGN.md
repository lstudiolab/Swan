# SwanUi Architecture

This document defines the first architecture contract for SwanUi. It is a design specification; implementation status is tracked in ROADMAP.md.

## 1. Public programming model

SwanUi should expose a declarative tree of immutable view descriptions. A view describes what should appear; it does not own a native window or directly issue drawing calls.

The core concepts are:

- **View**: a composable description of UI.
- **View identity**: stable identity across updates, used to preserve state and platform objects.
- **State**: storage owned by a view identity or observable model.
- **Binding**: a controlled read/write connection to state.
- **Environment**: inherited configuration such as locale, color scheme, typography, layout direction, and accessibility settings.
- **Modifier**: a composable transformation of layout, drawing, interaction, or accessibility behavior.
- **Scene**: an application-level presentation unit such as a window, document, or settings scene.
- **Renderer**: a backend that turns the platform-neutral render tree into visible UI.

The language features required to express these APIs cleanly are not all implemented in Swan yet. The library must wait for the language contracts described below rather than shipping fake protocol, generic, closure, or property-wrapper behavior.

## 2. State and update model

State changes must be deterministic and observable.

1. A state value belongs to a stable view identity or an explicitly owned model.
2. Reads register dependencies during view evaluation.
3. Writes mark dependent nodes dirty.
4. The scheduler coalesces invalidations into transactions.
5. Only affected view subtrees are reevaluated.
6. Reconciliation matches old and new nodes by stable identity and type.
7. Platform nodes are updated, inserted, or removed without recreating unaffected state.
8. Animation metadata is carried by the transaction, not hidden in global mutable state.

State mutation from worker threads must enter through a defined scheduler boundary. The initial implementation should use a single UI executor and add concurrency only after Swan's memory and synchronization model is ready.

Avoid implicit global state. Environment values are inherited snapshots; explicit model ownership and lifecycle rules define who retains and destroys observable models.

## 3. Layout contract

Use a proposal/measurement/placement model:

- Parent proposes a size range and alignment context.
- Child reports its ideal or constrained size.
- Parent places the child in a resolved rectangle.
- Layout is repeated only when a relevant proposal, child measurement, or environment value changes.

Initial layout primitives:

- HStack, VStack, ZStack
- Grid and lazy grid
- Spacer and Divider
- Frame, padding, offset, alignment, aspect ratio
- Overlay and background
- ScrollView and safe-area insets
- GeometryReader-equivalent geometry access, with explicit invalidation rules

Layout must define behavior for intrinsic content size, min/ideal/max dimensions, overflow, clipping, baseline alignment, right-to-left layout, and pixel rounding. Lazy containers must virtualize off-screen children and preserve stable identities.

## 4. Render tree

The render tree is platform-neutral and contains explicit operations for:

- text runs and glyph layout;
- images and symbol resources;
- filled and stroked paths;
- gradients, opacity, blend modes, clipping, masks, and transforms;
- shadows and supported visual effects;
- focus, hit regions, and accessibility nodes;
- native-control placeholders with semantic role and value;
- display-list caching and invalidation regions.

Drawing commands must carry bounds, transform, clip, opacity, resource identity, and source/debug identity. The renderer must not need to re-evaluate Swan source code.

## 5. Platform backend interface

Each backend advertises a capability set. A backend owns platform resources and translates framework operations to the platform.

Required backend responsibilities:

- application and scene lifecycle;
- windows, surfaces, display scale, and refresh scheduling;
- text shaping, input methods, selection, and clipboard;
- pointer, touch, keyboard, controller, and focus routing;
- native menus, dialogs, accessibility, and system appearance;
- image/font resource decoding and caching;
- render submission, synchronization, and teardown.

The initial backend should be chosen by a real supported deployment target and validated with a working window, text, input, and accessibility path. Do not implement all platform names as empty adapters.

Native controls and custom-rendered controls are both necessary. Use native controls when platform behavior, accessibility, text input, or system integration matters; use custom rendering when consistent cross-platform appearance or high-performance drawing is required.

## 6. Input, focus, and accessibility

Input is routed through a framework event model with capture/target/bubble phases and explicit cancellation rules. Gesture recognizers compete using documented precedence and simultaneous-recognition rules.

Focus is a first-class tree property. Keyboard navigation, focus restoration, escape behavior, and text-input focus must be consistent.

Accessibility is generated from semantic view nodes, not inferred only from pixels. Every interactive control must expose role, label, value, available actions, state, and focus behavior. Dynamic updates must notify platform accessibility APIs.

## 7. Animation and transactions

An animation is metadata attached to a state transaction and a set of animatable properties. Define:

- timing curves and spring parameters;
- duration, delay, repeat, and completion;
- property interpolation and interruption;
- transition insertion/removal semantics;
- reduced-motion behavior;
- frame scheduling and cancellation.

Animation must never delay semantic state updates. The visual presentation interpolates toward the committed state.

## 8. Resources and localization

Resource identity is stable and independent of filesystem paths. Resource bundles should support:

- raster/vector images and symbols;
- font families and weights;
- named colors and appearance variants;
- localized strings and plural rules;
- scale/density variants;
- asynchronous loading, caching, cancellation, and memory pressure.

The API must distinguish a missing resource from a resource that is still loading.

## 9. Testing model

Provide deterministic tests for:

- view identity and state preservation;
- dependency tracking and minimal invalidation;
- layout measurement and placement;
- event routing, focus, and gesture precedence;
- accessibility tree generation;
- animation transactions with a controllable clock;
- renderer output and resource lifetime;
- platform integration tests on each claimed target.

Golden-image tests supplement semantic tests; they do not replace them.

## 10. Required Swan language capabilities

Before a full declarative API is implemented, Swan needs verified support for:

1. Structs/value types and explicit layout rules.
2. Protocols/traits and constrained generic types.
3. Generic associated types or an equivalent composition mechanism.
4. Closures, captures, and function values.
5. A declarative composition mechanism equivalent to result builders, or a simpler Swan-native alternative.
6. Property-like state declarations or explicit state container syntax.
7. Reference/ownership semantics for observable models and platform handles.
8. Enums and pattern matching for events, layout results, and platform states.
9. Safe foreign interfaces for native windowing, graphics, text, and accessibility APIs.
10. Concurrency and a defined UI executor before background updates are exposed.

The exact surface syntax is a language-design decision. This framework must not introduce compiler-only magic that cannot be specified, type-checked, tested, and documented.
