/* Copyright (C) 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <cassert>

#include <elfutils/c++/libdw>

int
main (int argc, char *argv[])
{
  int fd = open (argv[1], O_RDONLY);
  if (fd < 0)
    throw std::runtime_error (strerror (errno));

  Dwarf *dw = dwarf_begin (fd, DWARF_C_READ);
  std::vector <std::pair <elfutils::unit_iterator, Dwarf_Die> > cudies;
  for (elfutils::unit_iterator it (dw); it != elfutils::unit_iterator::end (); ++it)
    cudies.push_back (std::make_pair (it, it->cudie));

  for (size_t i = 0; i < cudies.size (); ++i)
    {
      elfutils::unit_iterator jt (dw, cudies[i].second);
      std::cerr << std::hex << std::showbase << jt.offset () << std::endl;
      for (size_t j = i; jt != elfutils::unit_iterator::end (); ++jt, ++j)
	assert (jt == cudies[j].first);
    }

  assert (elfutils::die_tree_iterator (elfutils::unit_iterator::end ())
	  == elfutils::die_tree_iterator::end ());
}
