# URL Quirks

This is a collection of peculiarities you may find in trurl due to bugs or
changes/improvements in libcurl's URL handling.

## The URL API

Was introduced in libcurl 7.62.0. No older libcurl versions can be used.

Build-time requirement.

## Extracting zone id

Added in libcurl 7.65.0. The `CURLUE_NO_ZONEID` error code was added in
7.81.0.

Build-time requirement.

## Normalizing IPv4 addresses

Added in libcurl 7.77.0. Before that, the source formatting was kept.

Run-time requirement.

## Allow space

The libcurl URL parser was given the ability to allow spaces in libcurl
7.78.0. trurl therefore cannot offer this feature with older libcurl versions.

Build-time requirement.

## `curl_url_strerror()`

This API call was added in 7.80.0, using a libcurl version older than this
will make trurl output less good error messages.

Build-time requirement.

## Normalizing IPv6 addresses

Implemented in libcurl 7.81.0. Before this, the source formatting was kept.

Run-time requirement.

## `CURLU_PUNYCODE`

Added in libcurl 7.88.0.

Build-time requirement.

## Accepting % in host names

The host name parser has been made stricter over time, with the most recent
enhancement merged for libcurl 8.0.0.

Run-time requirement.

## Parsing IPv6 literals when libcurl does not support IPv6

Before libcurl 8.0.0 the URL parser was not able to parse IPv6 addresses if
libcurl itself was built without IPv6 capabilities.

Run-time requirement.

## URL encoding of fragments

This was a libcurl bug, fixed in libcurl 8.1.0

Run-time requirement.

## Bad IPv4 numerical address

The normalization of IPv4 addresses would just ignore bad addresses, while
newer libcurl versions will reject host names using invalid IPv4 addresses.
Fixed in 8.1.0

Run-time requirement.

## Set illegal scheme

Permitted before libcurl 8.1.0

Run-time requirement.
