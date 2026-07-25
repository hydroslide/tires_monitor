#ifndef FIRMWARE_VERSION_H
#define FIRMWARE_VERSION_H

// Firmware build identity for self-describing NBP dumps (story 08 / issue #9).
//
// The boot-metadata packet emits FIRMWARE_GIT_SHA so a log captured months later can be
// tied back to the exact firmware that produced it. The pinned arduino-cli profile build
// has no pre-build hook, so this value is a committed baseline (the SHA at the time of
// the commit that touched it) rather than a live HEAD read. A CI/build step can inject
// the exact SHA without editing this file by passing:
//   --build-property "build.extra_flags=-DFIRMWARE_GIT_SHA=\"<sha>\""
// The guard below lets that -D win over the committed default.
#ifndef FIRMWARE_GIT_SHA
#define FIRMWARE_GIT_SHA "de4211a"
#endif

#endif // FIRMWARE_VERSION_H
