// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOUCH_KEYBOARD_FAKEKEYBOARD_H_
#define TOUCH_KEYBOARD_FAKEKEYBOARD_H_

#include <algorithm>
#include <base/macros.h>
#include <list>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unordered_map>
#include <vector>

#include "touch_keyboard/evdevsource.h"
#include "touch_keyboard/haptic/touch_ff_manager.h"
#include "touch_keyboard/statemachine/statemachine.h"
#include "touch_keyboard/uinputdevice.h"

namespace touch_keyboard {

class Key {
 /* A class that represents a single key on the fake keyboard.
  *
  * This class is used to describe the location, size, and event code (which
  * letter is on the key) for a single key on a fake keyboard and keep track
  * of it's current state.  A keyboard's layout is defined as a vector of
  * these Key objects.
  */
 public:
  Key(int event_code, int xmin, int xmax, int ymin, int ymax) :
      event_code_(event_code), xmin_(xmin), xmax_(xmax), ymin_(ymin),
      ymax_(ymax) {}

  // Check if the point (x, y) is contained within this key.
  bool Contains(int x, int y) const {
    return (x < xmax_ && x >= xmin_ && y < ymax_ && y >= ymin_);
  }

  // This defines which event code to emit when this key is pressed.
  // Essentially this specifies which key it is. (eg: KEY_A, KEY_BACKSPACE, etc)
  int event_code_;

  // The range of x and y values that are contained within the key.
  int xmin_, xmax_;
  int ymin_, ymax_;
};

struct Event {
 /* A class to represent a pending keyboard event that is scheduled to be
  * generated by the fake keyboard.
  *
  * As keys are pressed by the user, the FakeKeyboard class generates keyboard
  * events, which represent the keys being pressed and released.  To allow some
  * measure of revoking, events are enqueued and only released after a brief
  * pause.  This way, if something unexpected happens to invalidate a keypress,
  * the system can simply not send it.  These events are represented as Event
  * objects stored in a queue.  They store all the information you need about
  * a keyboard event such as which keycode it is for, which direction the key
  * is going, and when the deadline to release the event is.
  */
 public:
  Event(int ev_code, bool is_down, struct timespec deadline, int tid) :
    is_guaranteed_(false), ev_code_(ev_code), is_down_(is_down), tid_(tid),
    deadline_(deadline) {}

  // Some events are guaranteed to fire before their deadline expires.  For
  // example, if a finger leaves before the deadline the system already knows
  // everything about it and can make a decision right away.  Since there may
  // be earlier, non-guaranteed events pending, we have to make such events
  // guaranteed so that when they make it to the front of the queue, we know
  // they are already checked and ready to go.
  bool is_guaranteed_;

  // This value stores which event code (which key) this event deals with.
  int ev_code_;

  // This value stores the "direction" of the event.  A value of true indicates
  // that this is a key-down event whereas false indicates a key-up event.
  bool is_down_;

  // Here we store the tracking ID (tid) of the finger that triggered this
  // event.  This is used to determine the validity of the event later, by
  // looking up the finger's behavior via this tid.
  int tid_;

  // This timespec represents the deadline for this event to be emitted by.
  // When an event is added to the queue a deadline is set briefly in the
  // future.  When this deadline passes, the FakeKeyboard is forced to make a
  // decision on whether or not the event is valid.
  struct timespec deadline_;
};

// These values represent the various rejection states of a finger that
// we're tracking on the touch keyboard and should be stored in their
// corresponding FingerData.rejection_status values.
enum class RejectionStatus {
  kNotRejectedYet = 0,
  kRejectTouchdownOffKey,
  kRejectMovedOffKey,
  kRejectAlreadyComplete,
};

struct FingerData {
 /* FingerData objects represent the information we have for a certain contact.
  *
  * As a finger arrives, moves, and leaves the touch sensor we need to track
  * various properties of the finger to allow this program to make intelligent
  * decisions about what it's doing.  This information is stored in these
  * FingerData objects for each contact.
  */
 public:
  // Here is the time the finger was first reported on the touchpad.
  struct timespec arrival_time_;

  // This value stores the maximum pressure reported for this contact since
  // its arrival.
  int max_pressure_;

  // Here we track which key in the layout the finger first appeared on.
  int starting_key_number_;

  // This Boolean indicates if a "key down" event has already been sent because
  // of something this finger did, and as a result a "key up" event must be sent
  // eventually.
  bool down_sent_;

  // This value indicates the current rejection state of this finger.  When a
  // finger misbehaves (acts in some way non-key-tapping-like) it can be marked
  // here for rejection.
  RejectionStatus rejection_status_;
};

class FakeKeyboard : public UinputDevice, public EvdevSource {
 /* The FakeKeyboard class implements a kernel-level keyboard that
  * generates events by processing touch input and comparing them
  * to a predefined layout.
  *
  * A FakeKeyboard object consists of several parts:
  *  1. It is an EvdevSource which pulls touch events from a source touch
  *     sensor.
  *  2. It is a UinputDevice which creates a fake input device in the
  *     kernel using the uinput module that will emit keyboard events.
  *  3. Finally, it includes logic to compare touches to a layout of keys
  *     printed on the touch sensor and determine which keys the user
  *     is intending to press
  *
  * To use this class, you should first instantiate a FakeKeyboard object
  * then specify which device it is reading from.  When you run Start() the
  * object will block forever, looping on the touch input and generating
  * keyboard events.
  */
 public:
  FakeKeyboard();

  // Use this function to actually start processing.  Start will block forever
  // and should never return, but a new keyboard device should appear and
  // begin sending out key events once you type on the touch sensor.
  void Start(std::string const &source_device_path,
             std::string const &keyboard_device_name);

 private:
  // This is the workhorse function called by Start() that actually loops to
  // consume the touch events and generate keystrokes.
  void Consume();

  // Use this function to enable the appropriate input events for the uinput
  // keyboard device when setting it up.  (eg: EV_KEY, KEY_ENTER, etc)
  void EnableKeyboardEvents() const;

  // This function does all the necessary work on each full "snapshot"
  // describing the current state of the touchpad.  This includes things like
  // updating the current FingerData objects and making inferences based on
  // finger position.
  void ProcessIncomingSnapshot(
      struct timespec now,
      std::unordered_map<int, struct mtstatemachine::MtFinger> const &snapshot);

  // Calling this function populates the layout_ member of a FakeKeyboard,
  // filling it with the locations of each key printed on the touch sensor.
  void SetUpLayout();

  // Place ev into the event queue, while maintaining chronological order of
  // the deadlines.
  void EnqueueEvent(Event ev);

  // Convenience function to build a guaranteed key-up event and enqueue it for
  // the given event code using the default deadline.
  void EnqueueKeyUpEvent(int ev_code, timespec now);

  // Mark a given contact as rejected for the stated reason.  This scans for
  // all pending events associated with this tracking ID and rejects them all.
  void RejectFinger(int tid, RejectionStatus reason);

  // When a finger is leaving the pad, some special bookkeeping is required.
  void HandleLeavingFinger(int tid, FingerData finger, timespec now);

  // When a finger first arrives on the sensor some special setup is required.
  int GenerateEventForArrivingFinger(
      struct timespec now,
      struct mtstatemachine::MtFinger const &finger, int tid);

  // Confirm that a finger's correct position is still within the boundaries of
  // the key that it initially arrived on.
  bool StillOnFirstKey(struct mtstatemachine::MtFinger const & finger,
                       FingerData const & data) const;

  // This is a convenience function that allows for easy manipulation of
  // timespec structs, which are used to track the deadlines for sending
  // events.  To easily add a few ms to a timespec, you can just call this.
  static struct timespec AddMsToTimespec(struct timespec const& orig,
                                         int additional_ms);

  // This is a convenience function that compares two timespecs, returning
  // true if t1 comes *after* than t2.
  static bool TimespecIsLater(struct timespec const& t1,
                              struct timespec const& t2);

  // The touch force feedback manager used to play ff effects.
  TouchFFManager ff_manager_;

  // This group of Key objects stores the full layout of the keyboard.
  std::vector<Key> layout_;

  // This state machine is used to interpret the raw touch events coming from
  // the kernel -- separating them by finger/etc.
  mtstatemachine::MtStateMachine sm_;

  // This list of events stores all pending events in chronological order based
  // on their deadlines.
  std::list<Event> pending_events_;

  // This is a mapping front tracking id's (TIDs) to finger information that
  // persists over the life of a contact to track global stats and information.
  std::unordered_map<int, FingerData> finger_data_;

  DISALLOW_COPY_AND_ASSIGN(FakeKeyboard);
};

}  // namespace touch_keyboard

#endif  // TOUCH_KEYBOARD_FAKEKEYBOARD_H_
