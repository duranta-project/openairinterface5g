<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Fronthaul common libraries

This directory contains the logic shared across the fronthaul stack components.

## Components

- **`fh_recv`**: Provides `fh_recv_cb` -a callback at packet reception.
- **`fh_send`**: Provides `fh_send_immediate` - a way to send fronthaul packets immediately.
- **`fh_timer`**: Provides `fh_timer_cb` - a callback at each symbol.

## Threading

Callbacks `fh_recv_cb` and `fh_timer_cb` are executed from the same thread. This makes the design of the packet processor simpler by
serializing timer and packet RX events. The functions registered to these callbacks should be very fast to prevent blocking the CPU
for too long.

`fh_send_immediate` does not require a dedicated CPU and instead uses locking to allow immediate sending from many cores at the same time. It does not support delayed sending of packets. All delays need to be handled by the application.
