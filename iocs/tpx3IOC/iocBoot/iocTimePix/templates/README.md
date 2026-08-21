# File plugin XML templates

Reference layouts for areaDetector file plugins. Not loaded at IOC startup — set plugin PVs at runtime (or in init scripts):

| Plugin | PVs | Directory |
|--------|-----|-----------|
| **NDFileHDF5** | `XMLFileName` (basename) + layout path from plugin config | `templates/hdf5/` |
| **NDFileNexus** | `TemplateFilePath` (trailing `/`) + `TemplateFileName` | `templates/nexus/` |

These use **different XML schemas**; do not point both plugins at the same file.

## `templates/hdf5/` (NDFileHDF5 `hdf5_layout`)

| File | Use |
|------|-----|
| `hdf5_minimal.xml` | 2D detector image with NeXus-style `NX_class` attributes |
| `hdf5_prvhst_histogram.xml` | PrvHst 1D ToF histogram (counts + uniform axis metadata via NDAttributes) |

Example (from IOC boot dir, prefix `TPX3-TEST:`):

```
dbpf("TPX3-TEST:HDF1:XMLFileName", "hdf5_minimal.xml")
# Set HDF5 plugin layout search path per ADCore / site convention
```

See `documentation/PROCESSED_IMAGE_FILE_SAVING.md` for PrvHst addresses **4–7** and troubleshooting.

## `templates/nexus/` (NDFileNexus `NXroot`)

| File | Use |
|------|-----|
| `plugin_template.xml` | Minimal validated NDFileNexus starter (`NXroot` / `pArray`) |

Validate offline with ADCore `XML_schema/template.sch` (see `ADCore/docs/ADCore/NDFileNexus.rst`).

**Note:** The [areaDetector collaboration](https://github.com/areaDetector/collaboration) has discussed retiring **NDFileNexus** in favour of **NDFileHDF5** (HDF5 layouts can carry NeXus metadata). Keeping NDFileNexus templates in a separate folder makes removal straightforward if ADCore drops that plugin.

## Adding layouts

- Shared 2D / generic layouts → `templates/hdf5/`
- Family-specific HDF5 layouts → optional `profiles/<family>/templates/hdf5/` (future)
- NDFileNexus only → `templates/nexus/` (legacy)

Use `git mv` when renaming or moving templates so history is preserved.
