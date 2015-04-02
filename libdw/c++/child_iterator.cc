/* -*-c++-*-
   Copyright (C) 2009, 2010, 2011, 2012, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include "libdw"
#include "libdwP.hh"

#include <cstring>

struct elfutils::child_iterator::pimpl
{
  Dwarf_Die m_die;

  // Create pimpl for end-iterator.
  pimpl ()
  {
    std::memset (&m_die, 0, sizeof m_die);
    m_die.addr = (void *) -1;
  }

  explicit pimpl (Dwarf_Die parent)
  {
    if (! dwpp_child (parent, m_die))
      *this = pimpl ();
  }

  pimpl (pimpl const &that)
    : m_die (that.m_die)
  {}

  bool
  operator== (pimpl const &that) const
  {
    return m_die.addr == that.m_die.addr;
  }

  void
  move ()
  {
    assert (! (*this == pimpl ()));
    if (! dwpp_siblingof (m_die, m_die))
      *this = pimpl ();
  }
};

elfutils::child_iterator::child_iterator ()
  : m_pimpl (new pimpl ())
{}

elfutils::child_iterator::child_iterator (Dwarf_Die parent)
  : m_pimpl (new pimpl (parent))
{}

elfutils::child_iterator::child_iterator (child_iterator const &that)
  : m_pimpl (new pimpl (*that.m_pimpl))
{}

elfutils::child_iterator::~child_iterator ()
{
  delete m_pimpl;
}

elfutils::child_iterator &
elfutils::child_iterator::operator= (child_iterator const &that)
{
  if (this != &that)
    {
      pimpl *npimpl = new pimpl (*that.m_pimpl);
      delete m_pimpl;
      m_pimpl = npimpl;
    }

  return *this;
}

elfutils::child_iterator
elfutils::child_iterator::end ()
{
  return child_iterator ();
}

bool
elfutils::child_iterator::operator== (child_iterator const &that) const
{
  return *m_pimpl == *that.m_pimpl;
}

bool
elfutils::child_iterator::operator!= (child_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::child_iterator &
elfutils::child_iterator::operator++ ()
{
  m_pimpl->move ();
  return *this;
}

elfutils::child_iterator
elfutils::child_iterator::operator++ (int)
{
  child_iterator ret = *this;
  ++*this;
  return ret;
}

Dwarf_Die &
elfutils::child_iterator::operator* ()
{
  return m_pimpl->m_die;
}

Dwarf_Die *
elfutils::child_iterator::operator-> ()
{
  return &**this;
}
