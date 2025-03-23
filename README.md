# K/drivers

## What

A module containing a general set of drivers.

## Adding / Removing drivers

Directories prefixed with `sys` imply that the drivers contained within are system drivers, or ones that come with the kernel by default. Ideally, not many drivers are contained here. Drivers not part of the default should be added to directories prefixed with `us`. This is merely for organization.

To add a driver, copy its source code into the appropriate directory under `src/c` and its includes into the appropriate sub-directory `src/c/include`.

## Notes

* Drivers that leave certain function of the ARC_DriverDef structure unimplemented should tie those functions to one which returns an error (a non-zero value). Use functions suffixed with `_empty` defined in dri_defs.c.

