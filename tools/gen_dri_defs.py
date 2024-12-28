'''
/**
 * @file gen_dri_defs.py
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System
 * Copyright (C) 2023-2024 awewsomegamer
 *
 * This file is part of Arctan.
 *
 * Arctan is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @DESCRIPTION
 * This file handles the management of the VFS's node graph, it is not intended
 * to be used by anything other than fs/vfs.c.
*/
'''

import sys
from pathlib import Path
from datetime import datetime

collision_counter = 0

def add_definition(definitions, group, name, ext):
  global collision_counter
  try:
    if (definitions[name]["group"] != group):
      print("Collision")
      collision_counter = collision_counter + 1
      return definitions
    definitions[name].update({ ext:0 })
  except:
    definitions[name] = { "group":group, ext:0 }

  return definitions

def fill_in_indices(definitions):
  index = 0
  for definition in definitions:
    if (definitions[definition]["group"] == 0 or definitions[definition]["group"] == 1):
      definitions[definition] = {"group":definitions[definition]["group"], "super":index, "directory":index + 1, "file":index + 2}
      index = index + 3
      continue
    definitions[definition].update({"":index})
    index = index + 1

  return definitions

def enumerate_source_files(base_path, key):
  definitions = {}
  sources = list(Path(base_path).rglob("*.c"))
  for source in sources:
    print("Found source file:",source)
    for line in reversed(list(open(source))):
      if (key in line):
        clean = line.replace(' ', '').split('(')[1].split(')')[0].split(',')

        group = int(clean[0])
        name = str(clean[1])
        ext = str(clean[2])
        print("\tFound driver definition (GROUP: {0}, NAME: {1}, EXT: {2})".format(group, name, ext))

        definitions = add_definition(definitions, group, name, ext)

  definitions = add_definition(definitions, -1, "COUNT", "")

  return fill_in_indices(definitions)

def construct_dri_defs(definitions, out_header_file, out_source_file):
  out_header = open(out_header_file, "w")

  out_header.write("/*\n")
  out_header.write(" * This file was autogenerated by gen_dri_defs.py.\n")
  out_header.write(" * Date of Generation: {0}\n".format(datetime.today().strftime("%d-%m-%Y")))
  out_header.write("*/\n")

  out_header.write("#ifndef AUTOGEN_ARC_DRIVERS_DRI_DEFS\n")
  out_header.write("#define AUTOGEN_ARC_DRIVERS_DRI_DEFS\n")

  out_header.write("#include <lib/resource.h>\n\n")

  # Variables and definitions
  for definition in definitions:
    for sub in definitions[definition]:
      if (sub == "group"):
        continue
      # Define
      name = "#define ARC_DRIDEF_{0} {1}\n".format(definition.upper(), definitions[definition][sub])
      if (sub != ""):
        name = "#define ARC_DRIDEF_{0}_{1} {2}\n".format(definition.upper(), sub.upper(), definitions[definition][sub])
      out_header.write(name)
      if (definitions[definition]["group"] != -1):
        # Global
        name = "extern struct ARC_DriverDef __driver_{0}_{1};\n".format(definition, sub)
        out_header.write(name)

  out_header.write("#define ARC_DRIDEF_PCI_TERMINATOR 0xFFFFFFFF\n")

  out_header.write("extern struct ARC_DriverDef *__DRIVER_LOOKUP_TABLE[];\n")

  out_header.write("int dridefs_int_func_empty();\n")
  out_header.write("size_t dridefs_size_t_func_empty();\n")
  out_header.write("void *dridefs_void_func_empty();\n")

  out_header.write("#endif // AUTOGEN_ARC_DRIVERS_DRI_DEFS\n")

  out_source = open(out_source_file, "w")

  # Table of silly
  out_source.write("/*\n")
  out_source.write(" * This file was autogenerated by gen_dri_defs.py.\n")
  out_source.write(" * Date of Generation: {0}\n".format(datetime.today().strftime("%d-%m-%Y")))
  out_source.write("*/\n")

  out_source.write("#include <drivers/dri_defs.h>\n");
  out_source.write("struct ARC_DriverDef *__DRIVER_LOOKUP_TABLE[] = {\n")
  for definition in definitions:
    for sub in definitions[definition]:
      if (sub == "group" or definitions[definition]["group"] == -1):
        continue
      name = "\t[{0}] = &__driver_{1}_{2},\n".format(definitions[definition][sub], definition, sub)
      out_source.write(name)
  out_source.write("};\n")

  out_source.write("int dridefs_int_func_empty() { return -1; }\n")
  out_source.write("size_t dridefs_size_t_func_empty() { return 0; }\n")
  out_source.write("void *dridefs_void_func_empty() { return NULL; }\n")

  return 0

def main():
  if (len(sys.argv) < 5):
    printf("Usage: python gen_dri_defs.py DRIVER_DEFINITION_MACRO_NAME path/to/source/root path/to/output/header/file path/to/output/source/file")
    return -1
  definitions = enumerate_source_files(sys.argv[2], sys.argv[1])
  return construct_dri_defs(definitions, sys.argv[3], sys.argv[4])

if (__name__ == "__main__"):
    main()
    print("Collision Counter:", collision_counter)
    sys.exit(collision_counter)
