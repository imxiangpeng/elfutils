/* Get macro information.
   Copyright (C) 2002-2009, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <dwarf.h>
#include <string.h>
#include <search.h>

#include <libdwP.h>

#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

static int
get_offset_from (Dwarf_Die *die, int name, Dwarf_Word *retp)
{
  /* Get the appropriate attribute.  */
  Dwarf_Attribute attr;
  if (INTUSE(dwarf_attr) (die, name, &attr) == NULL)
    return -1;

  /* Offset into the corresponding section.  */
  return INTUSE(dwarf_formudata) (&attr, retp);
}

static bool
demand (size_t nbytes, const unsigned char *readp, const unsigned char *endp)
{
  bool ret = readp < endp && (size_t) (endp - readp) >= nbytes;
  if (! ret)
    __libdw_seterrno (DWARF_E_INVALID_DWARF);
  return ret;
}

#define ck_read_ubyte_unaligned_inc(Nbytes, Dbg, Addr, AddrEnd, Ret)	\
  (demand ((Nbytes), (Addr), (AddrEnd))					\
   ? ((Ret = read_ubyte_unaligned_inc ((Nbytes), (Dbg), (Addr))), true) \
   : false)

static int
macro_op_compare (const void *p1, const void *p2)
{
  const Dwarf_Macro_Op_Table *t1 = (const Dwarf_Macro_Op_Table *) p1;
  const Dwarf_Macro_Op_Table *t2 = (const Dwarf_Macro_Op_Table *) p2;

  if (t1->offset < t2->offset)
    return -1;
  if (t1->offset > t2->offset + t2->read)
    return 1;

  return 0;
}

static void
build_table (Dwarf_Macro_Op_Table *table,
	     Dwarf_Macro_Op_Proto op_protos[static 255])
{
  unsigned ct = 0;
  for (unsigned i = 1; i < 256; ++i)
    if (op_protos[i - 1].forms != NULL)
      table->table[table->opcodes[i - 1] = ct++] = op_protos[i - 1];
    else
      table->opcodes[i] = 0xff;
}

#define MACRO_PROTO(NAME, ...)					\
  Dwarf_Macro_Op_Proto NAME = ({				\
      static const uint8_t proto[] = {__VA_ARGS__};		\
      (Dwarf_Macro_Op_Proto) {sizeof proto, proto};		\
    })

static Dwarf_Macro_Op_Table *
get_macinfo_table (void)
{
  static Dwarf_Macro_Op_Table *ret = NULL;
  if (ret != NULL)
    return ret;

  MACRO_PROTO (p_udata_str, DW_FORM_udata, DW_FORM_string);
  MACRO_PROTO (p_udata_udata, DW_FORM_udata, DW_FORM_udata);
  MACRO_PROTO (p_none);

  Dwarf_Macro_Op_Proto op_protos[255] =
    {
      [DW_MACINFO_define - 1] = p_udata_str,
      [DW_MACINFO_undef - 1] = p_udata_str,
      [DW_MACINFO_vendor_ext - 1] = p_udata_str,
      [DW_MACINFO_start_file - 1] = p_udata_udata,
      [DW_MACINFO_end_file - 1] = p_none,
    };

  static Dwarf_Macro_Op_Table table;
  memset (&table, 0, sizeof table);

  build_table (&table, op_protos);
  ret = &table;
  return ret;
}

static Dwarf_Macro_Op_Table *
get_table_for_offset (Dwarf *dbg, Dwarf_Word macoff,
		      const unsigned char *readp,
		      const unsigned char *const endp)
{
  Dwarf_Macro_Op_Table fake = { .offset = macoff };
  Dwarf_Macro_Op_Table **found = tfind (&fake, &dbg->macro_ops,
					macro_op_compare);
  if (found != NULL)
    return *found;

  const unsigned char *startp = readp;
  uint16_t version;
  if (! ck_read_ubyte_unaligned_inc (2, dbg, readp, endp, version))
    return NULL;
  if (version != 4)
    {
      __libdw_seterrno (DWARF_E_INVALID_VERSION);
      return NULL;
    }

  uint8_t flags;
  if (! ck_read_ubyte_unaligned_inc (1, dbg, readp, endp, flags))
    return NULL;

  bool is_64bit = (flags & 0x1) != 0;

  Dwarf_Off line_offset = (Dwarf_Off) -1;
  if ((flags & 0x2) != 0
      && ! ck_read_ubyte_unaligned_inc (is_64bit ? 8 : 4, dbg,
					readp, endp, line_offset))
    return NULL;

  /* """The macinfo entry types defined in this standard may, but
     might not, be described in the table""".  I.e. we have to assume
     these may be present and handle accordingly.  */

  MACRO_PROTO (p_udata_str, DW_FORM_udata, DW_FORM_string);
  MACRO_PROTO (p_udata_strp, DW_FORM_udata, DW_FORM_strp);
  MACRO_PROTO (p_udata_udata, DW_FORM_udata, DW_FORM_udata);
  MACRO_PROTO (p_secoffset, DW_FORM_sec_offset);
  MACRO_PROTO (p_none);

  Dwarf_Macro_Op_Proto op_protos[255] =
    {
      [DW_MACRO_GNU_define - 1] = p_udata_str,
      [DW_MACRO_GNU_undef - 1] = p_udata_str,
      [DW_MACRO_GNU_define_indirect - 1] = p_udata_strp,
      [DW_MACRO_GNU_undef_indirect - 1] = p_udata_strp,
      [DW_MACRO_GNU_start_file - 1] = p_udata_udata,
      [DW_MACRO_GNU_end_file - 1] = p_none,
      [DW_MACRO_GNU_transparent_include - 1] = p_secoffset,
      /* N.B. DW_MACRO_undef_indirectx, DW_MACRO_define_indirectx
	 should be added when 130313.1 is supported.  */
    };

  if ((flags & 0x4) != 0)
    {
      unsigned count;
      if (! ck_read_ubyte_unaligned_inc (1, dbg, readp, endp, count))
	return NULL;
      for (unsigned i = 0; i < count; ++i)
	{
	  unsigned opcode;
	  if (! ck_read_ubyte_unaligned_inc (1, dbg, readp, endp, opcode))
	    return NULL;

	  Dwarf_Macro_Op_Proto e;
	  get_uleb128 (e.nforms, readp); // XXX checking
	  e.forms = readp;
	  op_protos[opcode] = e;

	  demand (e.nforms, readp, endp);
	  readp += e.nforms;
	}
    }

  size_t ct = 0;
  for (unsigned i = 1; i < 256; ++i)
    if (op_protos[i - 1].forms != NULL)
      ++ct;

  /* We support at most 0xfe opcodes defined in the table, as 0xff is
     a value that means that given opcode is not stored at all.  But
     that should be fine, as opcode 0 is not allocated.  */
  assert (ct < 0xff);

  size_t macop_table_size = offsetof (Dwarf_Macro_Op_Table, table[ct]);

  Dwarf_Macro_Op_Table *table = libdw_alloc (dbg, Dwarf_Macro_Op_Table,
					     macop_table_size, 1);

  *table = (Dwarf_Macro_Op_Table) {
    .offset = macoff,
    .line_offset = line_offset,
    .header_len = readp - startp,
    .version = version,
    .is_64bit = is_64bit,
  };
  build_table (table, op_protos);

  Dwarf_Macro_Op_Table **ret = tsearch (table, &dbg->macro_ops,
					macro_op_compare);
  if (unlikely (ret == NULL))
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  return *ret;
}

static bool
comes_from (Dwarf *dbg, int idx, ptrdiff_t token,
	    const unsigned char **startpp,
	    const unsigned char **readpp,
	    const unsigned char **endpp)
{
  Elf_Data *d = dbg->sectiondata[idx];
  if (unlikely (d == NULL))
    return false;

  const unsigned char *readp = (void *) (uintptr_t) token;
  const unsigned char *startp = d->d_buf;
  const unsigned char *endp = startp + d->d_size;
  if (unlikely (readp == NULL || readp < startp || readp > endp))
    return false;

  *startpp = startp;
  *readpp = readp;
  *endpp = endp;
  return true;
}

static ptrdiff_t
read_macros (Dwarf *dbg, Dwarf_Macro_Op_Table *table,
	     const unsigned char *startp,
	     const unsigned char *readp,
	     const unsigned char *endp,
	     int (*callback) (Dwarf_Macro *, void *), void *arg)
{
  while (readp < endp)
    {
      unsigned int opcode = *readp++;
      if (opcode == 0)
	/* Nothing more to do.  */
	return 0;

      unsigned int idx = table->opcodes[opcode - 1];
      if (idx == 0xff)
	{
	  __libdw_seterrno (DWARF_E_INVALID_OPCODE);
	  return -1;
	}

      Dwarf_Macro_Op_Proto *proto = &table->table[idx];

      /* A fake CU with bare minimum data to fool dwarf_formX into
	 doing the right thing with the attributes that we put
	 out.  */
      Dwarf_CU fake_cu = {
	.dbg = dbg,
	.version = 4,
	.offset_size = table->is_64bit ? 8 : 4,
      };

#define SKIP(N)						\
      do						\
	{						\
	  __typeof ((N)) n = (N);			\
	  if (! demand (n, readp, endp))		\
	    return -1;					\
	  readp += n;					\
	}						\
      while (false)

      Dwarf_Attribute attributes[proto->nforms];
      for (Dwarf_Word i = 0; i < proto->nforms; ++i)
	{
	  /* We pretend this is a DW_AT_GNU_macros attribute so that
	     DW_FORM_sec_offset forms get correctly interpreted as
	     offset into .debug_macro.  */
	  attributes[i].code = DW_AT_GNU_macros;
	  attributes[i].form = proto->forms[i];
	  attributes[i].valp = (void *) readp;
	  attributes[i].cu = &fake_cu;

	  readp += __libdw_form_val_len (dbg, &fake_cu,
					 proto->forms[i], readp);
	}

      Dwarf_Macro macro = {
	.line_offset = table->line_offset,
	.version = table->version,
	.opcode = opcode,
	.nargs = proto->nforms,
	.attributes = attributes,
      };

      Dwarf_Off nread = readp - startp;
      if (nread > table->read)
	table->read = nread;

      if (callback (&macro, arg) != DWARF_CB_OK)
	return (ptrdiff_t) (uintptr_t) readp;
    }

  return 0;
}

ptrdiff_t
dwarf_getmacros_next (Dwarf *dbg,
		      int (*callback) (Dwarf_Macro *, void *),
		      void *arg, ptrdiff_t token)
{
  Dwarf_Macro_Op_Table *table;

  const unsigned char *startp, *readp, *endp;
  if (likely (comes_from (dbg, IDX_debug_macro, token,
			  &startp, &readp, &endp)))
    {
      Dwarf_Off macoff = readp - startp;
      table = get_table_for_offset (dbg, macoff, readp, endp);
      if (table == NULL)
	return -1;
    }
  else if (likely (comes_from (dbg, IDX_debug_macinfo, token,
			       &startp, &readp, &endp)))
    table = get_macinfo_table ();
  else
    {
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return -1;
    }

  return read_macros (dbg, table, startp, readp, endp, callback, arg);
}

ptrdiff_t
dwarf_getmacros_addr (Dwarf *dbg, Dwarf_Off offset,
		      int (*callback) (Dwarf_Macro *, void *),
		      void *arg)
{
  if (unlikely (dbg == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_DWARF);
      return -1;
    }

  Elf_Data *d = dbg->sectiondata[IDX_debug_macro];
  if (unlikely (d == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return -1;
    }

  const unsigned char *readp = d->d_buf + offset;
  const unsigned char *const endp = d->d_buf + d->d_size;
  Dwarf_Macro_Op_Table *table = get_table_for_offset (dbg, offset, readp, endp);
  if (table == NULL)
    return -1;

  return read_macros (dbg, table, d->d_buf, readp + table->header_len, endp,
		      callback, arg);
}

static Elf_Data *
old_style_data (Dwarf_Die *cudie)
{
  if (unlikely (! dwarf_hasattr (cudie, DW_AT_macro_info)))
    {
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return NULL;
    }

  Elf_Data *d = cudie->cu->dbg->sectiondata[IDX_debug_macinfo];
  if (unlikely (d == NULL) || unlikely (d->d_buf == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return NULL;
    }

  return d;
}

ptrdiff_t
dwarf_getmacros_die (Dwarf_Die *cudie,
		     int (*callback) (Dwarf_Macro *, void *),
		     void *arg)
{
  if (cudie == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DWARF);
      return -1;
    }

  if (likely (dwarf_hasattr (cudie, DW_AT_GNU_macros)))
    {
      Dwarf_Word macoff;
      if (get_offset_from (cudie, DW_AT_GNU_macros, &macoff) != 0)
	return -1;
      return dwarf_getmacros_addr (cudie->cu->dbg, macoff, callback, arg);
    }

  Elf_Data *d = old_style_data (cudie);
  if (d == NULL)
    return -1;

  Dwarf_Word macoff;
  if (get_offset_from (cudie, DW_AT_macro_info, &macoff) != 0)
    return -1;

  Dwarf_Macro_Op_Table *table = get_macinfo_table ();
  const unsigned char *readp = d->d_buf + macoff;
  const unsigned char *endp = d->d_buf + d->d_size;

  return read_macros (cudie->cu->dbg, table, d->d_buf, readp, endp,
		      callback, arg);
}

ptrdiff_t
dwarf_getmacros (die, callback, arg, offset)
     Dwarf_Die *die;
     int (*callback) (Dwarf_Macro *, void *);
     void *arg;
     ptrdiff_t offset;
{
  if (die == NULL)
    return -1;

  /* We can't support .debug_macro transparently by dwarf_getmacros,
     because extant callers would think that the returned macro
     opcodes come from DW_MACINFO_* domain and be confused.

     N.B. DIE's with both DW_AT_GNU_macros and DW_AT_macro_info are
     disallowed by the proposal that DW_AT_GNU_macros support is based on.  */
  if (unlikely (old_style_data (die) == NULL))
    return -1;

  /* But having filtered out cases of missing old-style data, we can
     safely piggy-back on existing new-style interfaces.  */
  if (offset == 0)
    return dwarf_getmacros_die (die, callback, arg);
  else
    return dwarf_getmacros_next (die->cu->dbg, callback, arg, offset);
}
