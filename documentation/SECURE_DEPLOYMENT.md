# Secure deployment and containment

R1-8-0 treats calibration paths, Serval destinations, credentials, and EPICS
write access as deployment policy rather than operator-selected arbitrary input.
The supplied TPX3 and MPX3 profiles establish the policies below before
`ADTimePixConfig` is called. Calibration access defaults to portable,
legacy-compatible operation; sites can opt into a narrower root.

## Calibration files

`ADTIMEPIX_CALIBRATION_ROOT` selects one existing absolute directory before
driver construction. If it is unset or empty, the driver defaults to `/` for
legacy-compatible operation. The driver canonicalizes that directory once
and exposes it through `CalibrationRoot_RBV` and `CalibrationPolicy_RBV`.

The supplied profiles use `/` as the default root because an upstream driver
cannot know a site's calibration layout. This reports `Permissive: /` and permits
calibration directories anywhere on the local filesystem while retaining
basename, regular-file, and atomic-write protections.

For a restrictive deployment, replace `/` in the family `unique.cmd` with one
approved subtree. Examples (choose one) are:

```iocsh
epicsEnvSet("ADTIMEPIX_CALIBRATION_ROOT", "$(ADTIMEPIX)/vendor")
epicsEnvSet("ADTIMEPIX_CALIBRATION_ROOT", "/opt/adtimepix/calibration")
epicsEnvSet("ADTIMEPIX_CALIBRATION_ROOT", "/data/detectors/tpx3")
```

`ADTIMEPIX_CALIBRATION_ROOT` accepts exactly one absolute directory. It is not
a comma-separated list or an exact-file allowlist. That single root covers the
root directory and all of its descendants. When several calibration directories
are needed, select their common parent as the root and constrain access with
filesystem permissions.

- BPC and DACS directories must resolve to the root or one of its descendants.
- BPC, DACS, and generated mask names must be basenames. Absolute names and
  names containing `..` or directory separators are rejected.
- Existing symlinks are resolved; a link escaping the root is rejected.
- An explicitly configured relative or inaccessible root blocks calibration
  reads, writes, JSON export, and Serval calibration-load requests.
- BPC and masked-pixel JSON replacements are staged in the destination
  directory, completely written and synchronized, then atomically renamed.
  New files use mode `0640`; replacements preserve existing permission bits.
  Failed staging or replacement leaves the prior target intact and does not
  initiate a BPC upload.

The root is immutable for the lifetime of the driver. Changing the environment
after `ADTimePixConfig` does not change policy; restart the IOC intentionally.

## Serval destinations

`ADTIMEPIX_DESTINATION_ALLOWLIST` is an immutable, comma-separated list of
exact destination bases. A single trailing `*` makes an entry a prefix rule.
Embedded or multiple wildcards and unsupported schemes are rejected. Supported
schemes are `file:/`, `tcp://`, and `http://`.

An unset or empty variable defaults to the portable policy used by the supplied
TPX3 and MPX3 profiles:

```iocsh
epicsEnvSet("ADTIMEPIX_DESTINATION_ALLOWLIST", "file:/*,tcp://*,http://*")
```

This permits arbitrary supported file destinations, TCP ports, TCP `listen` or
`connect` roles, and HTTP endpoints. `DestinationPolicy_RBV` reports
`Permissive`; `DestinationAllowlist_RBV` shows the effective patterns. The
driver still rejects unsupported schemes and unsafe `file:/` paths.

Sites with known endpoints can replace the permissive value with exact entries
and directory prefixes, for example:

```iocsh
epicsEnvSet("ADTIMEPIX_DESTINATION_ALLOWLIST", "file:/data/detector/*,tcp://listen@localhost:8088")
epicsEnvSet("ADTIMEPIX_DESTINATION_ALLOWLIST", "tcp://connect@example.invalid:9000,http://example.invalid:8080/output")
```

For `file:/` destinations, paths must be absolute and may not contain dot
segments, percent escapes, query or fragment syntax, or backslashes. Prefix
rules for file destinations must end at a directory boundary (`/*`).

Every enabled Raw, Image, Preview Image, and Preview Histogram base is checked
before the driver sends destination JSON to Serval. An invalid explicit
allowlist blocks destination updates. A valid restrictive list reports
`Enforced`.

## Serval credentials and transport

Literal placeholder credentials are not compiled into the driver. If Serval
requires HTTP Basic authentication, set both `ADTIMEPIX_SERVAL_USERNAME` and
`ADTIMEPIX_SERVAL_PASSWORD` in the protected IOC environment before the first
request. If either is absent or empty, the driver sends no Authorization
header. Never place credentials in committed startup scripts or PVs.

Basic authentication does not encrypt credentials or detector traffic. Use
plain HTTP only on a controlled detector network. For traffic crossing an
untrusted boundary, terminate authenticated TLS at a managed reverse proxy or
use a Serval endpoint that provides HTTPS, and restrict firewall access to the
IOC and expected stream endpoints.

## EPICS access security

`File.template` and the mask write controls accept `CALIBRATION_ASG`;
`Server.template` destination bases and `Dashboard.template` Apply accept
`DESTINATION_ASG`. The supplied profiles default them to `DEFAULT` so an
existing deployment is not silently locked out. Production sites should:

1. Copy `common/accessSecurity.acf.example` outside the repository and replace
   its placeholder user and host groups.
2. Set `CALIBRATION_ASG=CALIBRATION` and `DESTINATION_ASG=DESTINATION`
   before loading the templates.
3. Call `asSetFilename("/protected/path/accessSecurity.acf")` before `iocInit`.
4. Keep the ACF and IOC environment readable only by the service account and
   administrators.
5. Verify an authorized operator can write the calibration controls and an
   unauthorized account can only read them.

The calibration ASG covers BPC/DACS path and name selection, explicit BPC/DACS
load actions, the generated-mask filename, and mask upload. The destination ASG
covers output bases plus WriteData and ApplyConfig. Sites should also protect
channel enable, file-pattern, detector bias, trigger, and acquisition records in
site-appropriate groups when assembling the complete beamline database.

## Runtime qualification

TPX3 emulator qualification with the restrictive module-vendor root confirmed
that the calibration policy and a deliberately restrictive destination policy
report `Enforced`, and an approved destination can be applied without configured
Serval credentials. An out-of-root calibration directory was rejected with
`WRITE/INVALID` and `BPCFilePathExists_RBV=No`; a traversal filename was
rejected before a BPC request; and an unapproved histogram endpoint was rejected
before destination configuration reached Serval. Restoring approved values
returned the records and Apply operation to normal.

After restarting with the supplied portable defaults, IOC environment values and
driver readbacks agreed: `CalibrationRoot_RBV=/`,
`CalibrationPolicy_RBV=Permissive: /`,
`DestinationAllowlist_RBV=file:/*,tcp://*,http://*`, and
`DestinationPolicy_RBV=Permissive`.

## Service-account and filesystem policy

- Run the IOC and Serval as dedicated, non-login service accounts.
- Grant the IOC write access only below the calibration root and approved data
  roots. Grant Serval only the read/write access its selected destinations need.
- Do not run either process as root.
- Keep calibration and ACF directories off world-writable filesystems.
- Review `CalibrationPolicy_RBV` and `DestinationPolicy_RBV` after every IOC
  restart before enabling acquisition.
