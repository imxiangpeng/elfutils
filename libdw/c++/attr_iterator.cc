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

struct elfutils::attr_iterator::pimpl
{
  Dwarf_Die *m_die;
  Dwarf_Attribute m_at;
  ptrdiff_t m_offset;

  struct cb_data
  {
    // The visited attribute.
    Dwarf_Attribute *at;

    // Whether this is second pass through the callback.
    bool been;
  };

  static int
  callback (Dwarf_Attribute *at, void *data)
  {
    cb_data *d = static_cast <cb_data *> (data);
    if (d->been)
      return DWARF_CB_ABORT;

    *d->at = *at;
    d->been = true;

    // Do a second iteration to find the next offset.
    return DWARF_CB_OK;
  }

  void
  move ()
  {
    // If m_offset is already 1, we are done iterating.
    if (m_offset == 1)
      {
	*this = pimpl ((ptrdiff_t) 1);
	return;
      }

    cb_data data = {&m_at, false};
    m_offset = dwarf_getattrs (m_die, &callback, &data, m_offset);
    if (m_offset == -1)
      throw_libdw ();
  }

  pimpl (ptrdiff_t offset)
    : m_die (NULL)
    , m_at ((Dwarf_Attribute) {0, 0, NULL, NULL})
    , m_offset (offset)
  {}

  pimpl (Dwarf_Die *die)
    : m_die (die)
    , m_at ((Dwarf_Attribute) {0, 0, NULL, NULL})
    , m_offset (0)
  {
    move ();
  }

  bool
  operator== (pimpl const &other) const
  {
    return m_offset == other.m_offset
      && m_at.code == other.m_at.code;
  }
};

elfutils::attr_iterator::attr_iterator (ptrdiff_t offset)
  : m_pimpl (new pimpl (offset))
{}

elfutils::attr_iterator::attr_iterator (Dwarf_Die *die)
  : m_pimpl (new pimpl (die))
{}

elfutils::attr_iterator::attr_iterator (attr_iterator const &that)
  : m_pimpl (new pimpl (*that.m_pimpl))
{}

elfutils::attr_iterator::~attr_iterator ()
{
  delete m_pimpl;
}

elfutils::attr_iterator &
elfutils::attr_iterator::operator= (attr_iterator const &that)
{
  if (this != &that)
    {
      pimpl *npimpl = new pimpl (*that.m_pimpl);
      delete m_pimpl;
      m_pimpl = npimpl;
    }

  return *this;
}

elfutils::attr_iterator
elfutils::attr_iterator::end ()
{
  return attr_iterator ((ptrdiff_t) 1);
}

bool
elfutils::attr_iterator::operator== (attr_iterator const &that) const
{
  return *m_pimpl == *that.m_pimpl;
}

bool
elfutils::attr_iterator::operator!= (attr_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::attr_iterator &
elfutils::attr_iterator::operator++ ()
{
  m_pimpl->move ();
  return *this;
}

elfutils::attr_iterator
elfutils::attr_iterator::operator++ (int)
{
  attr_iterator tmp = *this;
  ++*this;
  return tmp;
}

Dwarf_Attribute &
elfutils::attr_iterator::operator* ()
{
  return m_pimpl->m_at;
}

Dwarf_Attribute *
elfutils::attr_iterator::operator-> ()
{
  return &**this;
}
