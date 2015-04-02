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

#include <dwarf.h>

#include "libdw"
#include "libdwP.hh"

struct elfutils::unit_iterator::pimpl
{
  Dwarf *m_dw;
  Dwarf_Off m_offset;
  Dwarf_Off m_old_offset;
  unit_info m_info;
  bool m_types;

  bool
  is_end ()
  {
    return m_offset == (Dwarf_Off) -1 && m_types;
  }

  void
  move ()
  {
    assert (! is_end ());
    m_old_offset = m_offset;
    size_t hsize;
    int rc = dwarf_next_unit (m_dw, m_offset, &m_offset, &hsize,
			      &m_info.version, &m_info.abbrev_offset,
			      &m_info.address_size, &m_info.offset_size,
			      m_types ? &m_info.type_signature : NULL, NULL);
    if (rc < 0)
      throw_libdw ();

    if (rc != 0 && m_types)
      done ();
    else if (rc != 0)
      {
	m_types = true;
	m_offset = 0;
	m_old_offset = 0;
	move ();
      }
    else
      m_info.cudie = (m_types ? dwpp_offdie_types : dwpp_offdie)
	(m_dw, m_old_offset + hsize);
  }

  void
  done ()
  {
    *this = *end ().m_pimpl;
    assert (is_end ());
  }

  pimpl (Dwarf_Off off, bool types)
    : m_dw (NULL)
    , m_offset (off)
    , m_old_offset (0)
    , m_types (types)
  {}

  explicit pimpl (Dwarf *dw)
    : m_dw (dw)
    , m_offset (0)
    , m_old_offset (0)
    , m_types (false)
  {
    move ();
  }

  // Construct a CU iterator for DW such that it points to a compile
  // unit represented by CUDIE.
  pimpl (Dwarf *dw, Dwarf_Die cudie)
    : m_dw (dw)
    , m_offset (dwarf_dieoffset (&cudie) - dwarf_cuoffset (&cudie))
    , m_old_offset (0)
    , m_types (dwarf_tag (&cudie) == DW_TAG_type_unit)
  {
    move ();
  }

  bool
  operator== (pimpl const &that) const
  {
    return m_types == that.m_types && m_offset == that.m_offset;
  }

  unit_info &
  deref ()
  {
    return m_info;
  }
};

elfutils::unit_iterator::unit_iterator (Dwarf_Off off, bool types)
  : m_pimpl (new pimpl (off, types))
{
}

elfutils::unit_iterator::unit_iterator (Dwarf *dw)
  : m_pimpl (new pimpl (dw))
{
}

elfutils::unit_iterator::unit_iterator (Dwarf *dw, Dwarf_Die cudie)
  : m_pimpl (new pimpl (dw, cudie))
{
}

elfutils::unit_iterator::unit_iterator (unit_iterator const &that)
  : m_pimpl (new pimpl (*that.m_pimpl))
{
}

elfutils::unit_iterator::~unit_iterator ()
{
  delete m_pimpl;
}

elfutils::unit_iterator &
elfutils::unit_iterator::operator= (unit_iterator const &that)
{
  if (this != &that)
    {
      pimpl *npimpl = new pimpl (*that.m_pimpl);
      delete m_pimpl;
      m_pimpl = npimpl;
    }
  return *this;
}

elfutils::unit_iterator
elfutils::unit_iterator::end ()
{
  unit_iterator ret ((Dwarf_Off) -1, true);
  assert (ret.m_pimpl->is_end ());
  return ret;
}

bool
elfutils::unit_iterator::operator== (unit_iterator const &that) const
{
  return *m_pimpl == *that.m_pimpl;
}

bool
elfutils::unit_iterator::operator!= (unit_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::unit_iterator &
elfutils::unit_iterator::operator++ ()
{
  m_pimpl->move ();
  return *this;
}

elfutils::unit_iterator
elfutils::unit_iterator::operator++ (int)
{
  unit_iterator tmp = *this;
  ++*this;
  return tmp;
}

elfutils::unit_info &
elfutils::unit_iterator::operator* ()
{
  return m_pimpl->deref ();
}

elfutils::unit_info *
elfutils::unit_iterator::operator-> ()
{
  return &**this;
}
