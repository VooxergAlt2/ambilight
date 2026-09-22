# DDP sender isolation

## Purpose

The active software scope is one HyperHDR PC at a time.

UDP itself has no session boundary, so without an explicit owner two PCs could interleave structurally valid DDP packets into one assembler state.

Stage 25 adds a sender lease before DdpAssembler.

## Endpoint identity

A sender is identified by:

    IPv4 address + UDP source port

The first structurally valid DDP datagram acquires the lease.

While the lease is active:

    owner packets   -> assembler
    foreign packets -> drop

## Structural validation before lock

A random UDP datagram must not be able to acquire ownership.

Before a sender can acquire or extend a lease, the datagram must pass:

- DDP v1
- sequence 1..15
- RGB24 data type
- destination 1
- non-empty payload
- exact datagram length
- offset inside the active runtime RGB frame
- payload end inside the active runtime RGB frame

The frame bound is:

    topology.totalLedCount() * 3

The measured default remains 2340 bytes for 780 LEDs. The historical Stage 38
2760-byte / 920-LED ceiling no longer exists: the sender gate and assembler use
the active runtime frame size, and DDP staging is allocated when topology
changes.

DdpAssembler repeats its own validation afterward as defense in depth.

## Lease

Default lease:

    1 second

Each structurally valid owner datagram refreshes the lease.

These do not refresh it:

- malformed owner datagrams
- foreign sender datagrams

At exactly the timeout boundary the owner is still valid.

The lease expires only when:

    age > 1 second

## Handoff

When the lease expires:

1. sender ownership is cleared
2. DdpAssembler active partial frame is cleared
3. last-completed sequence history is cleared
4. the next structurally valid sender can acquire ownership
5. its first sequence becomes a new stream epoch

This prevents a new HyperHDR instance from being rejected as stale merely because its sequence differs from the previous PC.

## Socket backlog behavior

All UDP datagrams still count toward raw socket workload and poll budget.

However rejected sender traffic is not allowed to force RGB render deferral.

Backlog-collapse is used only when a budget/cap boundary was reached while the poll contained accepted owner traffic and no rejected sender traffic.

Therefore a second PC cannot improve its influence by flooding UDP/4048 and causing the renderer to spend all its time draining packets that will be discarded anyway.

## Diagnostics

Periodic STAT reports:

    sender_lock
    sender
    saccept
    sinvalid
    sforeign
    sacq
    srel

Meaning:

- sender_lock: whether a lease is active
- sender: current owner IP:port
- saccept: structurally valid owner datagrams
- sinvalid: malformed/out-of-frame datagrams
- sforeign: valid datagrams from non-owner endpoints
- sacq: lease acquisitions
- srel: timeout releases

Raw:

    pkt

still means every UDP datagram consumed from the socket.

Assembler:

    asm / rejected / stale / completed

contains only traffic admitted by sender isolation.

## Explicit socket stop

Stopping DDP, for example because Wi-Fi credentials are cleared, explicitly resets:

- sender lease
- partial assembly
- sequence epoch

Accumulated statistics are retained.

## Current scope

This is automatic first-valid-sender ownership, not a permanent configured IP allow-list.

That matches the current one-PC runtime design while allowing another PC to take over after the active stream stops.


## Topology reconfiguration

Every explicit Stage 38 topology apply starts a new DDP transport epoch even
when the total LED count happens to remain unchanged.

This resets sender ownership and assembler sequence history so a GPIO/REV or
length commissioning change cannot inherit partial transport state from the
previous topology.
