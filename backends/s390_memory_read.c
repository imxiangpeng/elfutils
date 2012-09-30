/* ptrace-like read from memory of PID.
   Copyright (C) 2012 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/ptrace.h>
#include <errno.h>

#define BACKEND s390_
#include "libebl_CPU.h"

bool
s390_memory_read (Ebl *ebl __attribute__ ((unused)), pid_t pid, Dwarf_Addr addr, unsigned long *ul)
{
  errno = 0;
  *ul = ptrace (PTRACE_PEEKDATA, pid, (void *) (uintptr_t) addr, NULL);
  if (errno != 0)
    return false;
  return true;
}
