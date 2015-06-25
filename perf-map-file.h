/*
 * libperfjava: JVM agent to create a perf-<pid>.map file for consumption
 *              using the Linux perf tools
 *
 * Copyright (C) 2015 Jesper Pedersen <jesper.pedersen@comcast.net>
 *
 * Based on http://github.com/jrudolph/perf-map-agent
 * Copyright (C) 2013 Johannes Rudolph <johannes.rudolph@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

FILE*
perf_map_open(pid_t pid);

int
perf_map_close(FILE* file);

void
perf_map_write_entry(FILE* file, const void* code_addr, unsigned int code_size, const char* entry);
