# C++ antipatterns

These are habits to avoid in project-owned C++. Each entry names the
antipattern, why it causes trouble, and what to write instead. Where one
exists, the matching rule in the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
is given in parentheses; the project follows those guidelines.

This list is about design and correctness. Layout and formatting are covered
by [CODING_STYLE.md](CODING_STYLE.md), and the architectural boundaries by
[TECH.md](TECH.md).

## Ownership and lifetime

- **Calling `new` and `delete` directly.** Every path that skips the `delete`,
  including an exception, leaks; a second `delete` corrupts memory. Use values,
  standard containers, or `std::make_unique`. (R.11)
- **Owning raw pointers.** A `T*` that must be deleted by its holder looks
  identical to one that must not. Raw pointers and references only borrow; an
  owner is a value, a container or a `std::unique_ptr`. (R.3, I.11)
- **`std::shared_ptr` by default.** Shared ownership hides who controls a
  lifetime, costs an atomic count and invites reference cycles. Use
  `std::unique_ptr` unless ownership is genuinely shared. (R.21)
- **Smart pointers as ordinary parameters.** A function that only uses an
  object should take `T&` or `T*`, not `const std::unique_ptr<T>&` or a
  `std::shared_ptr<T>` copy. Take a smart pointer only to transfer or share
  ownership. (F.7, R.30)
- **Heap allocation without a reason.** `auto widget = std::make_unique<Widget>()`
  for an object that lives in one scope adds an allocation and an indirection.
  Declare a local. (R.5)
- **Returning a pointer or reference to a local.** The object is gone when the
  function returns. This includes a `std::string_view` or span of a local
  string or vector. (F.43)
- **Lambdas that capture by reference and outlive the scope.** A callback
  stored in a signal, a container or another thread still refers to locals
  that no longer exist. Capture by value, or capture a handle whose lifetime
  is managed. (F.53)
- **Keeping a reference, pointer or iterator into a container that is being
  changed.** Inserting into a `std::vector` can move every element.
  Re-find the element after the change, or store an index or ID.
- **Manual cleanup instead of RAII.** Paired calls such as open/close and
  lock/unlock break on early returns and exceptions. Wrap the resource in an
  object whose destructor releases it. (E.6, CP.20)

## Classes

- **Writing copy, move or destructor functions that the members already
  provide.** Each hand-written special member is code that can drift from the
  members it manages. Let the compiler generate them (the rule of zero). If
  one must be written, define or delete all five. (C.20, C.21)
- **A public base class without a virtual destructor.** Deleting a derived
  object through a base pointer is then undefined behavior. Make the
  destructor public and virtual, or protected and non-virtual. (C.35)
- **Overrides without `override`.** A misspelled name or a changed signature
  silently becomes a new function. Write `override` (or `final`) and only one
  of `virtual`, `override` and `final`. (C.128)
- **Calling virtual functions from constructors or destructors.** The derived
  part does not exist yet (or any longer), so the base version runs. (C.82)
- **Copying polymorphic objects by value.** Copying a derived object into a
  base object slices off the derived part. Suppress public copying in
  polymorphic bases. (C.67)
- **Implicit single-argument constructors.** A constructor such as
  `Distance(int feet)` lets any `int` turn into a `Distance` without notice.
  Mark it `explicit`. (C.46)
- **`const` or reference data members in a copyable type.** They make
  assignment impossible or surprising. Keep members mutable and the interface
  `const`-correct. (C.12)
- **Getters and setters for every member.** If any value is valid, the type is
  a plain `struct`. If there is an invariant, the class should offer
  operations that keep it, not raw access. (C.2, C.131)
- **Members that start uninitialized.** Give each member a default initializer
  where it is declared so every constructor starts from a valid state. (C.48)

## Functions and interfaces

- **Output parameters.** `void compute(const Input&, Result& out)` hides what
  the function produces and forces a default-constructed `Result` first.
  Return the value; return a `struct` for several values. (F.20, F.21)
- **Passing large objects by value or small ones by `const&` without
  thought.** Pass cheap-to-copy types by value and others by `const&`; take
  by value and move only when the function keeps its own copy. (F.16)
- **Adjacent parameters of the same type.** `attack(int bonus, int armor_class)`
  compiles with its arguments swapped. Use distinct types, a parameter
  `struct`, or a different order. (I.24)
- **Long parameter lists.** More than about four parameters usually means a
  missing type or a function doing two jobs. (I.23)
- **Boolean flags that select behavior.** `refresh(true, false)` is unreadable
  at the call site. Use two functions or a scoped enum.
- **Functions that do several unrelated things.** A function should keep one
  promise that a reader can see quickly. Split it. (F.2, F.3)
- **Ignoring a result the caller must handle.** Mark functions whose result
  matters `[[nodiscard]]`, and do not discard such results with a cast without
  a comment saying why.
- **`return std::move(local);`** It blocks copy elision and is never faster.
  Write `return local;`. (F.48)
- **Returning `const T` by value.** It prevents moving from the result. (F.49)

## Types and expressions

- **Plain `enum` and `#define` constants.** They leak names into the enclosing
  scope and convert to integers silently. Use `enum class` and `constexpr`.
  (Enum.1, Enum.3, ES.31)
- **Function-like macros.** They ignore scope and types and evaluate their
  arguments more than once. Use functions, templates or `constexpr`. (ES.31)
- **C-style casts.** `(int)value` can silently become a `reinterpret_cast` or
  cast away `const`. Avoid casts; when one is needed, use a named cast.
  (ES.48, ES.49)
- **Casting away `const`.** Writing through it is undefined behavior if the
  object really is `const`. Fix the interface instead. (ES.50)
- **Mixing signed and unsigned arithmetic.** `size() - 1` on an empty vector
  wraps to a huge number. Use signed types for arithmetic and compare sizes
  with `std::ssize` or `std::cmp_less`. (ES.100, ES.102, ES.106)
- **Narrowing conversions.** `int total = 2.7 * bonus;` truncates without a
  warning. Use `{}` initialization, which rejects narrowing, and convert
  explicitly where loss is intended. (ES.23, ES.46)
- **Uninitialized variables.** Declare a variable where its value is known.
  An immediately invoked lambda can initialize a `const` value that needs
  several steps. (ES.20)
- **`0` or `NULL` for pointers.** Use `nullptr`. (ES.47)
- **Magic numbers.** An unexplained `14` or `0.85` in logic hides its meaning
  and is changed in one place but not another. Name it with `constexpr`. For
  presentation values, see [Godot boundary](#godot-boundary). (ES.45)
- **Reusing one variable for unrelated purposes, or shadowing names in an
  inner scope.** Both make the reader track which meaning is current.
  (ES.12, ES.26)
- **Expressions whose evaluation order matters but is unspecified.**
  `f(++count, count)` has no defined result. Split the statement. (ES.43)
- **Hand-written index loops where a range `for` or algorithm says it
  better.** Off-by-one errors live in index arithmetic. Prefer a range `for`
  or a named standard algorithm, but not a dense one-liner a reader cannot
  follow. (ES.71)
- **Unnamed RAII guards.** `std::lock_guard<std::mutex>{mutex};` locks and
  unlocks on the same line. Give the guard a name. (ES.84)
- **Mutable global state.** Non-`const` globals and function-local statics
  create hidden coupling and order-of-initialization bugs. Pass the state
  explicitly. (I.2, I.22)

## Error handling

- **Throwing built-in types or strings.** `throw 42;` carries no meaning. Throw
  a `std::exception`-derived type, and catch by `const&`. (E.14, E.15)
- **Catching everything and continuing.** `catch (...) {}` turns a broken
  invariant into later, harder-to-trace damage. Catch what can be handled; log
  and report the rest.
- **Destructors, `swap` and move operations that can throw.** Throwing from a
  destructor during stack unwinding terminates the program. Mark them
  `noexcept` and make them unable to fail. (E.16, C.66)
- **Error handling for situations that cannot happen.** Code for impossible
  states hides the real invariants. State the invariant once, with an
  assertion or a check at the boundary where bad input can arrive.

## Headers

- **`using namespace` in a header.** It changes name lookup in every file that
  includes it. (SF.7)
- **Anonymous namespaces in a header.** Each including file gets its own
  copy. (SF.21)
- **Including more than the header needs.** Extra includes slow builds and
  couple modules. Forward-declare where a declaration suffices, and keep
  implementation details in the `.cpp`. Every header has an include guard.
  (SF.8)

## Concurrency

- **`volatile` for synchronization.** It does not make access atomic or
  ordered. Use `std::atomic` or a mutex. (CP.8)
- **Calling `lock()` and `unlock()` by hand.** Use `std::scoped_lock` or
  `std::lock_guard`. (CP.20)
- **Sharing data between threads without synchronization.** Any data race is
  undefined behavior. (CP.2)

## Godot boundary

These are specific to this project's GDExtension code in `src/OpenGoldBox`.

- **Letting an exception escape into Godot.** Godot calls signal handlers and
  virtual methods through the GDExtension C interface, which a C++ exception
  must not cross. Connect handlers through `presentation::guarded()`
  (`guarded_handlers.h`), which logs the failure and shows it through the view.
- **Treating node pointers as owners.** Nodes in the scene tree are owned by
  their parent. A pointer from `get_node` or `required_node` only borrows;
  never `delete` it or keep it in a `std::unique_ptr`. Remove nodes with
  `queue_free()`. A detached node is owned by `godot_nodes.h` until it is
  added as a child (see [TECH.md](TECH.md)).
- **Keeping node pointers across scene changes.** A cached pointer dangles once
  its node is freed. Look the node up again, or check
  `UtilityFunctions::is_instance_valid` for a node that may have been freed.
- **Holding `RefCounted` resources by raw pointer.** Textures, images and
  other resources are reference-counted. Hold them in `godot::Ref<T>`.
- **Layout, colors and sizes in C++.** Positions belong in the `.tscn` scenes
  and their layout animations; colors, font sizes and spacing belong in the
  theme (`OpenGoldPalette`, `OpenGoldMetrics` and variations). C++ supplies
  data. See [UI_GODOT_THEMES.md](UI_GODOT_THEMES.md).
- **Building player-facing text from English fragments.** Joined fragments
  cannot be translated, because word order differs between languages.
  Translate whole messages and insert values with `i18n::format()` and named
  arguments; mark static strings with `N_()`. See
  [LOCALIZATION.md](LOCALIZATION.md).
- **Godot types or game rules in the wrong layer.** Core and rules code must
  not include Godot headers. SRD mechanics belong in `opengold_rules_srd5`,
  not in a view. A rule computed in the presentation layer is untested by the
  native tests and duplicated wherever else it is needed.
- **Copying game code into a demo.** The demos exist to exercise the game's
  code. A demo that needs a game view opens the game's scene or compiles the
  game's source.
- **Drawing random numbers outside the rules' dice.** The SRD module shares one
  deterministic dice implementation so saved games and combat checkpoints
  replay exactly. Calling `std::rand`, `std::random_device` or Godot's `randi`
  in rules or combat code makes results unrepeatable.
