#pragma once

/// <summary>
/// Functions which should implement the system - specific 'C' runtime code.
/// </summary>

void system_disableInterrupts();
void system_enableInterrupts();
void system_stop();
void system_init();

void system_yield_CPU();

void gpio_write(int address, int value);
