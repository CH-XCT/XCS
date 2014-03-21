/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2014 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Input.hpp"
#include "MergeMouse.hpp"
#include "Event/Shared/Event.hpp"
#include "Event/Queue.hpp"
#include "IO/Async/IOLoop.hpp"
#include "Asset.hpp"

#include <algorithm>

#include <linux/input.h>
#include <sys/ioctl.h>

#ifdef KEY_DOWN
#undef KEY_DOWN
#endif
#ifdef KEY_UP
#undef KEY_UP
#endif

gcc_const
static unsigned
TranslateKeyCode(unsigned key_code)
{
  if (IsKobo()) {
    switch (key_code) {
    case KEY_HOME:
      /* the Kobo Touch "home" button shall open the menu */
      return KEY_MENU;
    }
  }

  return key_code;
}

template<typename T>
static constexpr unsigned
BitSize()
{
  return 8 * sizeof(T);
}

template<typename T>
static constexpr size_t
BitsToInts(unsigned n_bits)
{
  return (n_bits + BitSize<T>() - 1) / BitSize<T>();
}

template<typename T>
static constexpr bool
CheckBit(const T bits[], unsigned i)
{
  return bits[i / BitSize<T>()] & (T(1) << (i % BitSize<T>()));
}

/**
 * Check if the EVDEV supports EV_ABS or EV_REL..
 */
gcc_pure
static bool
IsPointerDevice(int fd)
{
  assert(fd >= 0);

  unsigned long features[BitsToInts<unsigned long>(std::max(EV_ABS, EV_REL))];
  if (ioctl(fd, EVIOCGBIT(0, sizeof(features)), features) < 0)
    return false;

  return CheckBit(features, EV_ABS) || CheckBit(features, EV_REL);
}

bool
LinuxInputDevice::Open(const char *path)
{
  if (!fd.OpenReadOnly(path))
    return false;

  fd.SetNonBlocking();
  io_loop.Add(fd.Get(), io_loop.READ, *this);

  min_x = max_x = min_y = max_y = 0;

  is_pointer = IsPointerDevice(fd.Get());
  if (is_pointer) {
    merge.AddPointer();

    if (!IsKobo()) {
      /* obtain touch screen information */
      /* no need to do that on the Kobo, because we know its touch
         screen is well-calibrated */

      input_absinfo abs;
      if (ioctl(fd.Get(), EVIOCGABS(ABS_X), &abs) == 0) {
        min_x = abs.minimum;
        max_x = abs.maximum;
      }

      if (ioctl(fd.Get(), EVIOCGABS(ABS_Y), &abs) == 0) {
        min_y = abs.minimum;
        max_y = abs.maximum;
      }
    }
  }

  rel_x = rel_y = 0;
  down = false;
  moving = pressing = releasing = false;
  return true;
}

void
LinuxInputDevice::Close()
{
  if (!IsOpen())
    return;

  if (is_pointer)
    merge.RemovePointer();

  io_loop.Remove(fd.Get());
  fd.Close();
}

void
LinuxInputDevice::Read()
{
  struct input_event buffer[64];
  const ssize_t nbytes = fd.Read(buffer, sizeof(buffer));
  if (nbytes < 0) {
    /* device has failed or was unplugged - bail out */
    if (errno != EAGAIN && errno != EINTR)
      Close();
    return;
  }

  unsigned n = size_t(nbytes) / sizeof(buffer[0]);

  for (unsigned i = 0; i < n; ++i) {
    const struct input_event &e = buffer[i];

    switch (e.type) {
    case EV_SYN:
      if (e.code == SYN_REPORT) {
        /* commit the finger movement */

        const bool pressed = pressing;
        const bool released = releasing;
        pressing = releasing = false;

        if (pressed)
          merge.SetDown(true);

        if (released)
          merge.SetDown(false);

        if (IsKobo() && released) {
          /* workaround: on the Kobo Touch N905B, releasing the touch
             screen reliably produces a finger position that is way
             off; in that case, ignore finger movement */
          moving = false;
          edit_position = public_position;
        }

        if (moving) {
          moving = false;
          public_position = edit_position;
          merge.MoveAbsolute(public_position.x, public_position.y,
                             min_x, max_x, min_y, max_y);
        } else if (rel_x != 0 || rel_y != 0) {
          merge.MoveRelative(rel_x, rel_y);
          rel_x = rel_y = 0;
        }
      }

      break;

    case EV_KEY:
      if (e.code == BTN_TOUCH || e.code == BTN_MOUSE) {
        bool new_down = e.value;
        if (new_down != down) {
          down = new_down;
          if (new_down)
            pressing = true;
          else
            releasing = true;
        }
      } else
        queue.Push(Event(e.value ? Event::KEY_DOWN : Event::KEY_UP,
                         TranslateKeyCode(e.code)));

      break;

    case EV_ABS:
      moving = true;

      switch (e.code) {
      case ABS_X:
        edit_position.x = e.value;
        break;

      case ABS_Y:
        edit_position.y = e.value;
        break;
      }

      break;

    case EV_REL:
      switch (e.code) {
      case REL_X:
        rel_x += e.value;
        break;

      case REL_Y:
        rel_y += e.value;
        break;
      }

      break;
    }
  }
}

bool
LinuxInputDevice::OnFileEvent(int fd, unsigned mask)
{
  Read();

  return true;
}
