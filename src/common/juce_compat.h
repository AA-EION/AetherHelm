#pragma once

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#ifndef JUCE_MODAL_LOOPS_PERMITTED
#define JUCE_MODAL_LOOPS_PERMITTED 1
#endif

#include <memory>
#include <type_traits>
#include <utility>

// Only define ScopedPointer for JUCE 6+ where it was deprecated/removed.
// In JUCE 5, ScopedPointer is already defined in juce_core.
#if defined(JUCE_MAJOR_VERSION) && (JUCE_MAJOR_VERSION >= 6)
#ifndef JUCE_SCOPEDPOINTER_H_INCLUDED
#define JUCE_SCOPEDPOINTER_H_INCLUDED

namespace juce {

template <class ObjectType>
class ScopedPointer {
public:
  inline ScopedPointer() noexcept : object(nullptr) {}
  inline ScopedPointer(std::nullptr_t) noexcept : object(nullptr) {}
  inline ScopedPointer(ObjectType* const objectToTakePossessionOf) noexcept : object(objectToTakePossessionOf) {}
  inline ScopedPointer(ScopedPointer& other) noexcept : object(other.release()) {}
  inline ScopedPointer(ScopedPointer&& other) noexcept : object(other.release()) {}

  inline ~ScopedPointer() {
    delete object;
  }

  inline ScopedPointer& operator=(ObjectType* const newObjectToTakePossessionOf) {
    if (object != newObjectToTakePossessionOf) {
      ObjectType* const old = object;
      object = newObjectToTakePossessionOf;
      delete old;
    }
    return *this;
  }

  inline ScopedPointer& operator=(ScopedPointer& other) {
    if (this != &other) {
      ObjectType* const old = object;
      object = other.release();
      delete old;
    }
    return *this;
  }

  inline ScopedPointer& operator=(ScopedPointer&& other) noexcept {
    if (this != &other) {
      ObjectType* const old = object;
      object = other.release();
      delete old;
    }
    return *this;
  }

  inline ScopedPointer& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  inline void reset() noexcept {
    ObjectType* const old = object;
    object = nullptr;
    delete old;
  }

  inline void reset(ObjectType* newObject) noexcept {
    operator=(newObject);
  }

  inline ObjectType* release() noexcept {
    ObjectType* const old = object;
    object = nullptr;
    return old;
  }

  inline operator ObjectType*() const noexcept { return object; }
  inline ObjectType* get() const noexcept { return object; }
  inline ObjectType& operator*() const noexcept { return *object; }
  inline ObjectType* operator->() const noexcept { return object; }

  inline bool operator==(const ObjectType* other) const noexcept { return object == other; }
  inline bool operator!=(const ObjectType* other) const noexcept { return object != other; }

private:
  ObjectType* object;
};

} // namespace juce

#endif // JUCE_SCOPEDPOINTER_H_INCLUDED
#endif // JUCE_MAJOR_VERSION >= 6
