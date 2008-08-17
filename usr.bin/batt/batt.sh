#!/bin/sh
sysctl -a | grep hw.acpi.batt | cut -c 9-30
