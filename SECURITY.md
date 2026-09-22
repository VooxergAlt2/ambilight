# Security policy

## Scope

Ambilight ESP32-C6 is hobby firmware intended for a trusted home LAN.

The embedded HTTP interface currently has no login and is not designed to be
exposed to the public internet. Network isolation remains part of the security
model.

Security-relevant reports include, for example:

- remote memory corruption or crashes reachable over Wi-Fi
- requests that bypass documented write guards
- accidental disclosure of saved Wi-Fi credentials
- unsafe persistence behavior that can restore cleared secrets
- vulnerabilities in the fallback AP or provisioning flow
- build or repository changes that expose credentials

## Reporting a vulnerability

Please do not open a public issue containing exploit details, credentials, or
other sensitive information.

If GitHub private vulnerability reporting is enabled for this repository, use
the repository's **Security** tab and submit a private report.

If private reporting is not available, open a minimal public issue stating that
you need a private channel for a security report. Do not include the sensitive
technical details in that issue.

## Supported versions

The project is developed continuously. Security fixes are applied to the current
`main` branch. Old stage branches and historical commits are not maintained as
supported release lines.

## Deployment guidance

- keep TCP/80 on a trusted LAN
- do not forward the Web UI from a router to the internet
- replace or isolate the fallback AP as appropriate for your environment
- do not commit `include/secrets.h`
- treat unencrypted NVS as recoverable by an attacker with physical flash access
- size, fuse, and wire LED power independently from the ESP32 board

This project does not currently operate a bug bounty program.
