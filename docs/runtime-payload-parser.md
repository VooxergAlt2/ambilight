# Runtime payload parser

## Purpose

Stage 32 separated serial framing from subsystem logic.

Stage 33 separates the remaining typed payload grammar from main.cpp.

RuntimePayloadParser is pure C++ and converts command text into existing domain types.

## Supported payloads

Brightness:

    b0
    b255

Output:

    uint8_t brightness

LED topology:

    l230:18:0,160:19:0,230:20:0,160:21:0

Output:

    LedMappingProfile schema 2

Commissioning range:

    jside:2:100:10
    jgpio:20:0:37

Output:

    CommissioningRangeRequest

Spatial profile:

    y1437.5,1000,0,0,0,0,0,10

Output:

    TofSpatialProfile

Gain curve:

    q50:2048,500:3072,4000:4096

Output:

    GainPoint[8] + count

## Parse result

RuntimePayloadParseResult distinguishes:

    Ok
    Empty
    InvalidFormat
    OutOfRange

The main runtime handler decides how to report the result to the user.

## Existing domain validation is reused

The payload parser does not duplicate business rules.

LED mapping ends with:

    LedMappingProfile::valid()

Spatial parsing ends with:

    TofSpatialProfile::valid()

Gain parsing builds:

    DistanceGainCurve

and requires:

    curve.valid()

This keeps one authoritative validation rule for each domain object.

## Fixed-point spatial grammar

Length fields accept:

    integer
    integer.d

where d is exactly one decimal digit.

Examples:

    1437.5
    1000
    -45.6

More than one decimal digit is rejected.

The persisted representation remains tenths of a millimetre.

## Gain curve grammar

Each knot is:

    distance_mm:gain_q12

Separated by commas.

Rules are ultimately enforced by DistanceGainCurve:

- 2..8 points
- strictly increasing distance
- non-decreasing gain
- gain <= 4096

## Main.cpp after Stage 33

main.cpp no longer contains:

- parseUint16Token
- parseDecimalX10
- consumeComma
- parseLedMappingText
- parseSpatialProfileText
- parseGainCurveText

It only:

1. receives a typed SerialCommandEvent
2. calls RuntimePayloadParser where needed
3. applies the resulting domain object

## Native contracts

test/test_runtime_payload_parser covers:

- brightness 0 and 255
- invalid brightness characters
- brightness out of range
- LED COUNT/GPIO/REV topology
- duplicate GPIO rejection
- side count bounds
- LED topology syntax errors
- logical/raw commissioning ranges
- spatial fixed-point values
- negative sensor offsets
- one-decimal precision
- spatial range validation
- valid gain curve
- non-monotonic distance
- non-monotonic gain
- gain above unity
- malformed curve syntax
- more than eight gain points

## Validation status

Tests are committed but have not been executed in this development session.

Use the Stage 31 local validation harness when PlatformIO is available.
