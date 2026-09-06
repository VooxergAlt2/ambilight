# DDP UDP socket hardening

## Purpose

The DDP socket previously requested a larger receive buffer but ignored the result.

That made one important realtime assumption invisible:

    did lwIP actually accept the requested UDP receive buffer?

Stage 26 turns this into observable, non-fatal runtime state.

## Requested receive buffer

Firmware requests:

    SO_RCVBUF = 32768 bytes

The request is deliberately larger than the default 2340-byte RGB frame and the Stage 38 maximum 2760-byte frame so short scheduling/network bursts have room before user-space polling catches up.

## Configuration sequence

On every new socket:

1. create UDP socket
2. try SO_REUSEADDR
3. request SO_RCVBUF=32768
4. query SO_RCVBUF with getsockopt
5. bind UDP/4048
6. continue operation even if optional socket tuning failed

Socket creation/bind failures remain fatal to DDP startup.

Optional socket-option failures do not.

## Why non-fatal

lwIP/Arduino builds can differ in:

- whether SO_RCVBUF is enabled
- receive-buffer accounting semantics
- maximum accepted value
- getsockopt behavior

A firmware build should not refuse Ambilight service solely because an optimization was unavailable.

Instead the condition is exposed in telemetry.

## Telemetry

Startup prints:

    RXBUF requested=...
    actual=...
    set=ok|no
    query=ok|no
    option_warnings=...

Periodic STAT adds:

    rxreq
    rxactual
    rxset
    rxget
    optwarn

Additional internal counters are retained:

    reuseAddrSetFailures
    rxBufferSetFailures
    rxBufferQueryFailures
    lastSocketOptionErrno

## Reconfiguration

Wi-Fi provisioning can stop and reopen DDP at runtime.

Before each new socket, per-socket fields are reset:

- actualRxBufferBytes
- rxBufferSetOk
- rxBufferQueryOk
- lastSocketOptionErrno

Cumulative warning counters are intentionally retained across reconnects.

Therefore diagnostics distinguish current socket state from historical instability.

## Interpretation

Healthy example:

    rxreq=32768
    rxactual=32768
    rxset=ok
    rxget=ok
    optwarn=0

A different positive actual value is not automatically an error because platform accounting may differ.

The important hardware-validation questions are:

- did setting/querying succeed?
- are DDP packet loss/backlog counters healthy?
- is actual runtime latency acceptable?

## Relationship to sender isolation

Stage 25 sender isolation remains before DdpAssembler.

Stage 26 only makes the underlying UDP socket capacity observable.

It does not weaken:

- sender lease
- packet validation
- assembler bounds
- poll time budget
- render backlog policy
