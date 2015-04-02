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
#include <algorithm>

#include "libdw"
#include "libdwP.hh"

namespace
{
  Dwarf *
  getdw (elfutils::unit_iterator &cuit)
  {
    return dwarf_cu_getdwarf (cuit->cudie.cu);
  }
}

struct elfutils::die_tree_iterator::pimpl
{
  unit_iterator m_cuit;
  // Internally, only offsets are kept on the stack.
  std::vector <Dwarf_Off> m_stack;
  Dwarf_Die m_die;

  pimpl (Dwarf_Off)
    : m_cuit (unit_iterator::end ())
  {}

  explicit pimpl (Dwarf *dw)
    : m_cuit (unit_iterator (dw))
    , m_die (m_cuit->cudie)
  {}

  explicit pimpl (unit_iterator const &cuit)
    : m_cuit (cuit)
    , m_die (m_cuit->cudie)
  {}

  pimpl (pimpl const &that)
    : m_cuit (that.m_cuit)
    , m_stack (that.m_stack)
    , m_die (that.m_die)
  {}

  bool
  operator== (pimpl const &that) const
  {
    return m_cuit == that.m_cuit
      && m_stack == that.m_stack
      // xxx fishy.  What if other is end()?
      && (m_cuit == unit_iterator::end ()
	  || m_die.addr == that.m_die.addr);
  }

  void
  move ()
  {
    Dwarf_Die child;
    if (dwpp_child (m_die, child))
      {
	m_stack.push_back (dwarf_dieoffset (&m_die));
	m_die = child;
	return;
      }

    do
      if (dwpp_siblingof (m_die, m_die))
	return;
      else
	// No sibling found.  Go a level up and retry, unless this
	// was a sole, childless CU DIE.
	if (! m_stack.empty ())
	  {
	    m_die = (dwarf_tag (&m_cuit->cudie) == DW_TAG_type_unit
		     ? dwpp_offdie_types : dwpp_offdie)
	      (getdw (m_cuit), m_stack.back ());
	    m_stack.pop_back ();
	  }
    while (! m_stack.empty ());

    m_die = (++m_cuit)->cudie;
  }
};

elfutils::die_tree_iterator::die_tree_iterator (Dwarf_Off off)
  : m_pimpl (new pimpl (off))
{}

elfutils::die_tree_iterator::die_tree_iterator (Dwarf *dw)
  : m_pimpl (new pimpl (dw))
{}

elfutils::die_tree_iterator::die_tree_iterator (unit_iterator const &cuit)
  : m_pimpl (new pimpl (cuit))
{}

elfutils::die_tree_iterator::die_tree_iterator (die_tree_iterator const &that)
  : m_pimpl (new pimpl (*that.m_pimpl))
{}

elfutils::die_tree_iterator::~die_tree_iterator ()
{
  delete m_pimpl;
}

elfutils::die_tree_iterator &
elfutils::die_tree_iterator::operator= (die_tree_iterator const &that)
{
  if (this != &that)
    {
      pimpl *npimpl = new pimpl (*that.m_pimpl);
      delete m_pimpl;
      m_pimpl = npimpl;
    }
  return *this;
}

elfutils::die_tree_iterator
elfutils::die_tree_iterator::end ()
{
  return die_tree_iterator ((Dwarf_Off) -1);
}

bool
elfutils::die_tree_iterator::operator== (die_tree_iterator const &that) const
{
  return *m_pimpl == *that.m_pimpl;
}

bool
elfutils::die_tree_iterator::operator!= (die_tree_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::die_tree_iterator &
elfutils::die_tree_iterator::operator++ ()
{
  m_pimpl->move ();
  return *this;
}

elfutils::die_tree_iterator
elfutils::die_tree_iterator::operator++ (int)
{
  die_tree_iterator ret = *this;
  ++*this;
  return ret;
}

Dwarf_Die &
elfutils::die_tree_iterator::operator* ()
{
  return m_pimpl->m_die;
}

Dwarf_Die *
elfutils::die_tree_iterator::operator-> ()
{
  return &**this;
}

elfutils::die_tree_iterator::stack_type
elfutils::die_tree_iterator::stack () const
{
  stack_type ret;
  for (die_tree_iterator it = *this; it != end (); it = it.parent ())
    ret.push_back (*it);
  std::reverse (ret.begin (), ret.end ());
  return ret;
}

elfutils::die_tree_iterator
elfutils::die_tree_iterator::parent ()
{
  assert (*this != end ());
  if (m_pimpl->m_stack.empty ())
    return end ();

  die_tree_iterator ret = *this;
  ret.m_pimpl->m_die = (dwarf_tag (&m_pimpl->m_cuit->cudie) == DW_TAG_type_unit
			? dwpp_offdie_types : dwpp_offdie)
    (getdw (m_pimpl->m_cuit), m_pimpl->m_stack.back ());
  ret.m_pimpl->m_stack.pop_back ();

  return ret;
}
