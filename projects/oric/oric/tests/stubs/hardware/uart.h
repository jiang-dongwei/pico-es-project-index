#pragma once
static const int uart_default=0;
inline bool uart_is_writable(int) { return false; }
inline void uart_putc_raw(int,char) {}
