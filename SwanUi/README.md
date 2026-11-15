# SwanUi

SwanUi is Swan's declarative, cross-platform user-interface framework. Its design is inspired by the developer experience and breadth of SwiftUI: describe a UI as a composition of views, bind it to state, and let a renderer manage layout, updates, input, accessibility, and presentation.

SwanUi is a new framework, not a wrapper around SwiftUI and not a port of Apple's implementation. It will be implemented as Swan modules with a Swan-owned UI runtime and renderer interfaces. Platform APIs may be used behind explicit backends; the public view model remains platform-independent.

## Goals

- Declarative, composable UI with a predictable view lifecycle.
- State-driven rendering and efficient incremental updates.
- Layout primitives, modifiers, controls, navigation, presentation, animation, and accessibility.
- Input handling for pointer, touch, keyboard, controller, and platform-specific interaction.
- Shared application logic across supported desktop, mobile, and embedded UI targets.
- Native platform integration where it improves fidelity, accessibility, performance, or distribution.
- A coherent API rather than a pile of unrelated platform bindings.

## Planned API families

| Family | Scope |
|---|---|
| `SwanUi.Core` | `View`, view identity, environment, preferences, transactions, view composition |
| `SwanUi.State` | state, bindings, observable models, derived state, environment values |
| `SwanUi.Layout` | stacks, grids, overlays, spacers, frames, alignment, geometry, safe areas |
| `SwanUi.Text` | text, formatting, fonts, labels, attributed content, localization |
| `SwanUi.Controls` | buttons, toggles, sliders, steppers, pickers, text fields, secure fields, progress |
| `SwanUi.Collections` | lists, tables, sections, lazy stacks/grids, scroll views |
| `SwanUi.Navigation` | navigation stacks, destinations, tabs, split views, toolbars |
| `SwanUi.Presentation` | sheets, popovers, alerts, confirmation dialogs, menus, windows |
| `SwanUi.Drawing` | paths, shapes, gradients, strokes, fills, Canvas, images, symbols |
| `SwanUi.Animation` | animation curves, transitions, matched geometry, transactions |
| `SwanUi.Gestures` | tap, drag, long press, magnification, rotation, composed gestures |
| `SwanUi.Accessibility` | labels, values, hints, actions, traits, focus, accessibility tree |
| `SwanUi.Scene` | application, window groups, scene lifecycle, commands, document scenes |
| `SwanUi.Resources` | images, fonts, colors, asset catalogs, bundles, localization |
| `SwanUi.Platform` | native handles, platform services, lifecycle and system integration |
| `SwanUi.Testing` | view inspection, deterministic state, interaction and snapshot tests |

These are planned API families, not claims that those modules already compile or run.

## Example direction

The intended experience is declarative and state-driven. This is an API sketch to communicate the design; it is **not valid against the current Swan compiler yet**:

```swan
# Future SwanUi API sketch — not currently compilable.
import SwanUi

struct CounterView: View:
    @State var count = 0

    func body() -> some View:
        VStack(spacing: 12):
            Text("Count: \\(count)")
            Button("Increase"):
                count = count + 1
```

The final syntax will be decided alongside Swan's implementation of structs, protocols/traits, generics, closures, property-like state declarations, and declarative composition. The framework will not force these concepts into the current function-only subset.

## Rendering architecture

SwanUi separates the declarative view description from platform rendering:

1. **View graph** — stable identity, composition, dependencies, and lifecycle.
2. **State engine** — tracks state and observable dependencies; invalidates only affected subtrees.
3. **Layout engine** — proposes constraints, measures children, and places geometry.
4. **Render tree** — platform-neutral drawing, text, clipping, effects, and hit-testing commands.
5. **Input and focus** — routes pointer, touch, keyboard, controller, and accessibility actions.
6. **Platform backend** — owns windows, surfaces, native controls, text input, display scheduling, and OS integration.
7. **Resource system** — loads and caches images, fonts, colors, and localized assets.

The renderer contract must support both native controls and custom-rendered controls. It must not assume every platform has the same windowing model or capabilities.

## Platform strategy

The framework core is platform-independent. Platform backends are separate implementations with explicit capability declarations.

- Apple platforms: native windowing, text input, accessibility, menus, and system integration through an Apple backend.
- Windows: native windowing, input, text, accessibility, and composition through a Windows backend.
- Linux: supported windowing/input/rendering stacks selected behind the backend contract.
- Android: activity/window lifecycle, touch, text input, and accessibility through an Android backend.
- Web: optional WebAssembly/DOM or canvas backend; this is a separate target, not an assumption of the native runtime.

A backend may initially support only a documented subset. SwanUi must report unsupported capabilities rather than silently substituting behavior that changes semantics.

## Status

**Design stage.** No SwiftUI-equivalent renderer, widget set, state engine, or platform backend is implemented yet. See [DESIGN.md](DESIGN.md) and [ROADMAP.md](ROADMAP.md) for the architecture and implementation gates.
