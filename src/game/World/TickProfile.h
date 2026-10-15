/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Fork: wall-clock and thread-CPU timing for the slow-tick log (TickLog.Threshold). A span whose wall time is
// far above its CPU time was waiting (database, lock, disk) rather than computing.

#ifndef MANGOS_TICKPROFILE_H
#define MANGOS_TICKPROFILE_H

#include <chrono>
#include <ctime>

namespace TickProfile
{
    inline double ThreadCpuMs()
    {
#ifdef __linux__
        timespec ts;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#else
        return 0.0;
#endif
    }

    struct Stamp
    {
        std::chrono::steady_clock::time_point wall;
        double cpuMs;
    };

    struct Span
    {
        double wallMs = 0.0;
        double cpuMs = 0.0;
    };

    inline Stamp Now() { return { std::chrono::steady_clock::now(), ThreadCpuMs() }; }

    inline double WallMsSince(std::chrono::steady_clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    inline Span Since(Stamp const& start)
    {
        Span span;
        span.wallMs = WallMsSince(start.wall);
        span.cpuMs = ThreadCpuMs() - start.cpuMs;
        return span;
    }
}

#endif
